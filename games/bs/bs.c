/*	$OpenBSD: bs.c,v 1.5 1997/07/23 20:04:49 kstailey Exp $	*/
/*
 * bs.c - original author: Bruce Holloway
 *		salvo option by: Chuck A DeGaul
 * with improved user interface, autoconfiguration and code cleanup
 *		by Eric S. Raymond <esr@snark.thyrsus.com>
 * v1.2 with color support and minor portability fixes, November 1990
 * v2.0 featuring strict ANSI/POSIX conformance, November 1993.
 */

#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef A_UNDERLINE	/* BSD curses */
#define	beep()	write(1,"\007",1);
#define	cbreak	crmode
#define	saveterm savetty
#define	resetterm resetty
#define	nocbreak nocrmode
#define strchr	index
#endif /* !A_UNDERLINE */

#ifdef isxdigit		/* aha, must be an AT&T system... */
#define srand(n)	srand48(n)
#define rand()		lrand48()
#define bzero(s, n)	(void)memset((char *)(s), '\0', n)
/*
 * Try this if ungetch() fails to resolve.
 *
 * #define ungetch ungetc
 */
#endif /* isxdigit */

static bool checkplace();

/*
 * Constants for tuning the random-fire algorithm. It prefers moves that
 * diagonal-stripe the board with a stripe separation of srchstep. If
 * no such preferred moves are found, srchstep is decremented.
 */
#define BEGINSTEP	3	/* initial value of srchstep */

/* miscellaneous constants */
#define SHIPTYPES	5
#define	OTHER		(1-turn)
#define PLAYER		0
#define COMPUTER	1
#define MARK_HIT	'H'
#define MARK_MISS	'o'
#define CTRLC		'\003'	/* used as terminate command */
#define FF		'\014'	/* used as redraw command */

/* coordinate handling */
#define BWIDTH		10
#define BDEPTH		10

/* display symbols */
#define SHOWHIT		'*'
#define SHOWSPLASH	' '
#define IS_SHIP(c)	isupper(c)

/* how to position us on player board */
#define PYBASE	3
#define PXBASE	3
#define PY(y)	(PYBASE + (y))
#define PX(x)	(PXBASE + (x)*3)
#define pgoto(y, x)	(void)move(PY(y), PX(x))

/* how to position us on cpu board */
#define CYBASE	3
#define CXBASE	48
#define CY(y)	(CYBASE + (y))
#define CX(x)	(CXBASE + (x)*3)
#define cgoto(y, x)	(void)move(CY(y), CX(x))

#define ONBOARD(x, y)	(x >= 0 && x < BWIDTH && y >= 0 && y < BDEPTH)

/* other board locations */
#define COLWIDTH	80
#define PROMPTLINE	21			/* prompt line */
#define SYBASE		CYBASE + BDEPTH + 3	/* move key diagram */
#define SXBASE		63
#define MYBASE		SYBASE - 1		/* diagram caption */
#define MXBASE		64
#define HYBASE		SYBASE - 1		/* help area */
#define HXBASE		0

/* this will need to be changed if BWIDTH changes */
static char numbers[] = "   0  1  2  3  4  5  6  7  8  9";

static char carrier[] = "Aircraft Carrier";
static char battle[] = "Battleship";
static char sub[] = "Submarine";
static char destroy[] = "Destroyer";
static char ptboat[] = "PT Boat";

static char name[40];
static char dftname[] = "stranger";

/* direction constants */
#define E	0
#define SE	1
#define S	2
#define SW	3
#define W	4
#define NW	5
#define N	6
#define NE	7
static int xincr[8] = {1,  1,  0, -1, -1, -1,  0,  1};
static int yincr[8] = {0,  1,  1,  1,  0, -1, -1, -1};

/* current ship position and direction */
static int curx = (BWIDTH / 2);
static int cury = (BDEPTH / 2);

typedef struct
{
    char *name;		/* name of the ship type */
    unsigned hits;	/* how many times has this ship been hit? */
    char symbol;	/* symbol for game purposes */
    char length;	/* length of ship */
    char x, y;		/* coordinates of ship start point */
    char dir;		/* direction of `bow' */
    bool placed;	/* has it been placed on the board? */
}
ship_t;

ship_t plyship[SHIPTYPES] =
{
    { carrier,	0, 'A', 5},
    { battle,	0, 'B', 4},
    { destroy,	0, 'D', 3},
    { sub,	0, 'S', 3},
    { ptboat,	0, 'P', 2},
};

ship_t cpuship[SHIPTYPES] =
{
    { carrier,	0, 'A', 5},
    { battle,	0, 'B', 4},
    { destroy,	0, 'D', 3},
    { sub,	0, 'S', 3},
    { ptboat,	0, 'P', 2},
};

/* "Hits" board, and main board. */
static char hits[2][BWIDTH][BDEPTH], board[2][BWIDTH][BDEPTH];

static int turn;			/* 0=player, 1=computer */
static int plywon=0, cpuwon=0;		/* How many games has each won? */

static int salvo, blitz, closepack;

#define	PR	(void)addstr

static void uninitgame(sig)
/* end the game, either normally or due to signal */
int	sig;
{
    clear();
    (void)refresh();
    (void)resetterm();
    (void)echo();
    (void)endwin();
    exit(0);
}

static void announceopts()
/* announce which game options are enabled */
{
    if (salvo || blitz || closepack)
    {
	(void) printw("Playing optional game (");
	if (salvo)
	    (void) printw("salvo, ");
	else
	    (void) printw("nosalvo, ");
	if (blitz)
	    (void) printw("blitz ");
	else
	    (void) printw("noblitz, ");
	if (closepack)
	    (void) printw("closepack)");
	else
	    (void) printw("noclosepack)");
    }
    else
	(void) printw(
	"Playing standard game (noblitz, nosalvo, noclosepack)");
}

static void intro()
{
    extern char *getlogin();
    char *tmpname;

    srand(time(0L)+getpid());	/* Kick the random number generator */

    (void) signal(SIGINT,uninitgame);
    (void) signal(SIGINT,uninitgame);
    (void) signal(SIGIOT,uninitgame);		/* for assert(3) */
    if(signal(SIGQUIT,SIG_IGN) != SIG_IGN)
	(void)signal(SIGQUIT,uninitgame);

    if(tmpname = getlogin())
    {
	(void)strcpy(name,tmpname);
	name[0] = toupper(name[0]);
    }
    else
	(void)strcpy(name,dftname);

    (void)initscr();
#ifdef KEY_MIN
    keypad(stdscr, TRUE);
#endif /* KEY_MIN */
    (void)saveterm();
    (void)nonl();
    (void)cbreak();
    (void)noecho();

#ifdef PENGUIN
    (void)clear();
    (void)mvaddstr(4,29,"Welcome to Battleship!");
    (void)move(8,0);
    PR("                                                  \\\n");
    PR("                           \\                     \\ \\\n");
    PR("                          \\ \\                   \\ \\ \\_____________\n");
    PR("                         \\ \\ \\_____________      \\ \\/            |\n");
    PR("                          \\ \\/             \\      \\/             |\n");
    PR("                           \\/               \\_____/              |__\n");
    PR("           ________________/                                       |\n");
    PR("           \\  S.S. Penguin                                         |\n");
    PR("            \\                                                     /\n");
    PR("             \\___________________________________________________/\n");

    (void) mvaddstr(22,27,"Hit any key to continue..."); (void)refresh();
    (void) getch();
#endif /* PENGUIN */

#ifdef A_COLOR
    start_color();

    init_pair(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
    init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
#endif /* A_COLOR */

}

/* VARARGS1 */
static void prompt(n, f, s)
/* print a message at the prompt line */
int n;
char *f, *s;
{
    (void) move(PROMPTLINE + n, 0);
    (void) clrtoeol();
    (void) printw(f, s);
    (void) refresh();
}

static void error(s)
char *s;
{
    (void) move(PROMPTLINE + 2, 0);
    (void) clrtoeol();
    if (s)
    {
	(void) addstr(s);
	(void) beep();
    }
}

static void placeship(b, ss, vis)
int b;
ship_t *ss;
int vis;
{
    int l;

    for(l = 0; l < ss->length; ++l)
    {
	int newx = ss->x + l * xincr[ss->dir];
	int newy = ss->y + l * yincr[ss->dir];

	board[b][newx][newy] = ss->symbol;
	if (vis)
	{
	    pgoto(newy, newx);
	    (void) addch((chtype)ss->symbol);
	}
    }
    ss->hits = 0;
}

static int rnd(n)
int n;
{
    return(((rand() & 0x7FFF) % n));
}

static void randomplace(b, ss)
/* generate a valid random ship placement into px,py */
int b;
ship_t *ss;
{
    register int bwidth = BWIDTH - ss->length;
    register int bdepth = BDEPTH - ss->length;

    do {
	ss->y = rnd(bdepth);
	ss->x = rnd(bwidth);
	ss->dir = rnd(2) ? E : S;
    } while
	(!checkplace(b, ss, FALSE));
}

static void initgame()
{
    int i, j, unplaced;
    ship_t *ss;

    (void) clear();
    (void) mvaddstr(0,35,"BATTLESHIPS");
    (void) move(PROMPTLINE + 2, 0);
    announceopts();

    bzero(board, sizeof(char) * BWIDTH * BDEPTH * 2);
    bzero(hits, sizeof(char) * BWIDTH * BDEPTH * 2);
    for (i = 0; i < SHIPTYPES; i++)
    {
	ss = cpuship + i;
	ss->x = ss->y = ss->dir = ss->hits = ss->placed = 0;
	ss = plyship + i;
	ss->x = ss->y = ss->dir = ss->hits = ss->placed = 0;
    }

    /* draw empty boards */
    (void) mvaddstr(PYBASE - 2, PXBASE + 5, "Main Board");
    (void) mvaddstr(PYBASE - 1, PXBASE - 3,numbers);
    for(i=0; i < BDEPTH; ++i)
    {
	(void) mvaddch(PYBASE + i, PXBASE - 3, i + 'A');
#ifdef A_COLOR
	if (has_colors())
	    attron(COLOR_PAIR(COLOR_BLUE));
#endif /* A_COLOR */
	(void) addch(' ');
	for (j = 0; j < BWIDTH; j++)
	    (void) addstr(" . ");
#ifdef A_COLOR
	attrset(0);
#endif /* A_COLOR */
	(void) addch(' ');
	(void) addch(i + 'A');
    }
    (void) mvaddstr(PYBASE + BDEPTH, PXBASE - 3,numbers);
    (void) mvaddstr(CYBASE - 2, CXBASE + 7,"Hit/Miss Board");
    (void) mvaddstr(CYBASE - 1, CXBASE - 3, numbers);
    for(i=0; i < BDEPTH; ++i)
    {
	(void) mvaddch(CYBASE + i, CXBASE - 3, i + 'A');
#ifdef A_COLOR
	if (has_colors())
	    attron(COLOR_PAIR(COLOR_BLUE));
#endif /* A_COLOR */
	(void) addch(' ');
	for (j = 0; j < BWIDTH; j++)
	    (void) addstr(" . ");
#ifdef A_COLOR
	attrset(0);
#endif /* A_COLOR */
	(void) addch(' ');
	(void) addch(i + 'A');
    }

    (void) mvaddstr(CYBASE + BDEPTH,CXBASE - 3,numbers);

    (void) mvprintw(HYBASE,  HXBASE,
		    "To position your ships: move the cursor to a spot, then");
    (void) mvprintw(HYBASE+1,HXBASE,
		    "type the first letter of a ship type to select it, then");
    (void) mvprintw(HYBASE+2,HXBASE,
		    "type a direction ([hjkl] or [4862]), indicating how the");
    (void) mvprintw(HYBASE+3,HXBASE,
		    "ship should be pointed. You may also type a ship letter");
    (void) mvprintw(HYBASE+4,HXBASE,
		    "followed by `r' to position it randomly, or type `R' to");
    (void) mvprintw(HYBASE+5,HXBASE,
		    "place all remaining ships randomly.");

    (void) mvaddstr(MYBASE,   MXBASE, "Aiming keys:");
    (void) mvaddstr(SYBASE,   SXBASE, "y k u    7 8 9");
    (void) mvaddstr(SYBASE+1, SXBASE, " \\|/      \\|/ ");
    (void) mvaddstr(SYBASE+2, SXBASE, "h-+-l    4-+-6");
    (void) mvaddstr(SYBASE+3, SXBASE, " /|\\      /|\\ ");
    (void) mvaddstr(SYBASE+4, SXBASE, "b j n    1 2 3");

    /* have the computer place ships */
    for(ss = cpuship; ss < cpuship + SHIPTYPES; ss++)
    {
	randomplace(COMPUTER, ss);
	placeship(COMPUTER, ss, FALSE);
    }

    ss = (ship_t *)NULL;
    do {
	extern char *strchr();
	static char getcoord();
	char c, docked[SHIPTYPES + 2], *cp = docked;

	/* figure which ships still wait to be placed */
	*cp++ = 'R';
	for (i = 0; i < SHIPTYPES; i++)
	    if (!plyship[i].placed)
		*cp++ = plyship[i].symbol;
	*cp = '\0';

	/* get a command letter */
	prompt(1, "Type one of [%s] to pick a ship.", docked+1);
	do {
	    c = getcoord(PLAYER);
	} while
	    (!strchr(docked, c));

	if (c == 'R')
	    (void) ungetch('R');
	else
	{
	    /* map that into the corresponding symbol */
	    for (ss = plyship; ss < plyship + SHIPTYPES; ss++)
		if (ss->symbol == c)
		    break;

	    prompt(1, "Type one of [hjklrR] to place your %s.", ss->name);
	    pgoto(cury, curx);
	}

	do {
	    c = getch();
	} while
	    (!strchr("hjklrR", c) || c == FF);

	if (c == FF)
	{
	    (void)clearok(stdscr, TRUE);
	    (void)refresh();
	}
	else if (c == 'r')
	{
	    prompt(1, "Random-placing your %s", ss->name);
	    randomplace(PLAYER, ss);
	    placeship(PLAYER, ss, TRUE);
	    error((char *)NULL);
	    ss->placed = TRUE;
	}
	else if (c == 'R')
	{
	    prompt(1, "Placing the rest of your fleet at random...");
	    for (ss = plyship; ss < plyship + SHIPTYPES; ss++)
		if (!ss->placed)
		{
		    randomplace(PLAYER, ss);
		    placeship(PLAYER, ss, TRUE);
		    ss->placed = TRUE;
		}
	    error((char *)NULL);
	}
	else if (strchr("hjkl8462", c))
	{
	    ss->x = curx;
	    ss->y = cury;

	    switch(c)
	    {
	    case 'k': case '8': ss->dir = N; break;
	    case 'j': case '2': ss->dir = S; break;
	    case 'h': case '4': ss->dir = W; break;
	    case 'l': case '6': ss->dir = E; break;
	    }

	    if (checkplace(PLAYER, ss, TRUE))
	    {
		placeship(PLAYER, ss, TRUE);
		error((char *)NULL);
		ss->placed = TRUE;
	    }
	}

	for (unplaced = i = 0; i < SHIPTYPES; i++)
	    unplaced += !plyship[i].placed;
    } while
	(unplaced);

    turn = rnd(2);

    (void) mvprintw(HYBASE,  HXBASE,
		    "To fire, move the cursor to your chosen aiming point   ");
    (void) mvprintw(HYBASE+1,  HXBASE,
		    "and strike any key other than a motion key.            ");
    (void) mvprintw(HYBASE+2,  HXBASE,
		    "                                                       ");
    (void) mvprintw(HYBASE+3,  HXBASE,
		    "                                                       ");
    (void) mvprintw(HYBASE+4,  HXBASE,
		    "                                                       ");
    (void) mvprintw(HYBASE+5,  HXBASE,
		    "                                                       ");

    (void) prompt(0, "Press any key to start...");
    (void) getch();
}

static int getcoord(atcpu)
int atcpu;
{
    int ny, nx, c;

    if (atcpu)
	cgoto(cury,curx);
    else
	pgoto(cury, curx);
    (void)refresh();
    for (;;)
    {
	if (atcpu)
	{
	    (void) mvprintw(CYBASE + BDEPTH+1, CXBASE+11, "(%d, %c)", curx, 'A'+cury);
	    cgoto(cury, curx);
	}
	else
	{
	    (void) mvprintw(PYBASE + BDEPTH+1, PXBASE+11, "(%d, %c)", curx, 'A'+cury);
	    pgoto(cury, curx);
	}

	switch(c = getch())
	{
	case 'k': case '8':
#ifdef KEY_MIN
	case KEY_UP:
#endif /* KEY_MIN */
	    ny = cury+BDEPTH-1; nx = curx;
	    break;
	case 'j': case '2':
#ifdef KEY_MIN
	case KEY_DOWN:
#endif /* KEY_MIN */
	    ny = cury+1;	nx = curx;
	    break;
	case 'h': case '4':
#ifdef KEY_MIN
	case KEY_LEFT:
#endif /* KEY_MIN */
	    ny = cury;		nx = curx+BWIDTH-1;
	    break;
	case 'l': case '6':
#ifdef KEY_MIN
	case KEY_RIGHT:
#endif /* KEY_MIN */
	    ny = cury;		nx = curx+1;
	    break;
	case 'y': case '7':
#ifdef KEY_MIN
	case KEY_A1:
#endif /* KEY_MIN */
	    ny = cury+BDEPTH-1; nx = curx+BWIDTH-1;
	    break;
	case 'b': case '1':
#ifdef KEY_MIN
	case KEY_C1:
#endif /* KEY_MIN */
	    ny = cury+1;	nx = curx+BWIDTH-1;
	    break;
	case 'u': case '9':
#ifdef KEY_MIN
	case KEY_A3:
#endif /* KEY_MIN */
	    ny = cury+BDEPTH-1; nx = curx+1;
	    break;
	case 'n': case '3':
#ifdef KEY_MIN
	case KEY_C3:
#endif /* KEY_MIN */
	    ny = cury+1;	nx = curx+1;
	    break;
	case FF:
	    nx = curx; ny = cury;
	    (void)clearok(stdscr, TRUE);
	    (void)refresh();
	    break;
	default:
	    if (atcpu)
		(void) mvaddstr(CYBASE + BDEPTH + 1, CXBASE + 11, "      ");
	    else
		(void) mvaddstr(PYBASE + BDEPTH + 1, PXBASE + 11, "      ");
	    return(c);
	}

	curx = nx % BWIDTH;
	cury = ny % BDEPTH;
    }
}

static int collidecheck(b, y, x)
/* is this location on the selected zboard adjacent to a ship? */
int b;
int y, x;
{
    int	collide;

    /* anything on the square */
    if (collide = IS_SHIP(board[b][x][y]))
	return(collide);

    /* anything on the neighbors */
    if (!closepack)
    {
	int i;

	for (i = 0; i < 8; i++)
	{
	    int xend, yend;

	    yend = y + yincr[i];
	    xend = x + xincr[i];
	    if (ONBOARD(xend, yend))
		collide += IS_SHIP(board[b][xend][yend]);
	}
    }
    return(collide);
}

static bool checkplace(b, ss, vis)
int b;
ship_t *ss;
int vis;
{
    int l, xend, yend;

    /* first, check for board edges */
    xend = ss->x + ss->length * xincr[ss->dir];
    yend = ss->y + ss->length * yincr[ss->dir];
    if (!ONBOARD(xend, yend))
    {
	if (vis)
	    switch(rnd(3))
	    {
	    case 0:
		error("Ship is hanging from the edge of the world");
		break;
	    case 1:
		error("Try fitting it on the board");
		break;
	    case 2:
		error("Figure I won't find it if you put it there?");
		break;
	    }
	return(0);
    }

    for(l = 0; l < ss->length; ++l)
    {
	if(collidecheck(b, ss->y+l*yincr[ss->dir], ss->x+l*xincr[ss->dir]))
	{
	    if (vis)
		switch(rnd(3))
		{
		    case 0:
			error("There's already a ship there");
			break;
		    case 1:
			error("Collision alert!  Aaaaaagh!");
			break;
		    case 2:
			error("Er, Admiral, what about the other ship?");
			break;
		    }
	    return(FALSE);
	    }
	}
    return(TRUE);
}

static int awinna()
{
    int i, j;
    ship_t *ss;

    for(i=0; i<2; ++i)
    {
	ss = (i) ? cpuship : plyship;
	for(j=0; j < SHIPTYPES; ++j, ++ss)
	    if(ss->length > ss->hits)
		break;
	if (j == SHIPTYPES)
	    return(OTHER);
    }
    return(-1);
}

static ship_t *hitship(x, y)
/* register a hit on the targeted ship */
int x, y;
{
    ship_t *sb, *ss;
    char sym;
    int oldx, oldy;

    getyx(stdscr, oldy, oldx);
    sb = (turn) ? plyship : cpuship;
    if(!(sym = board[OTHER][x][y]))
	return((ship_t *)NULL);
    for(ss = sb; ss < sb + SHIPTYPES; ++ss)
	if(ss->symbol == sym)
	{
	    if (++ss->hits < ss->length)	/* still afloat? */
		return((ship_t *)NULL);
	    else				/* sunk! */
	    {
		int i, j;

		if (!closepack)
		    for (j = -1; j <= 1; j++)
		    {
			int bx = ss->x + j * xincr[(ss->dir + 2) % 8];
			int by = ss->y + j * yincr[(ss->dir + 2) % 8];

			for (i = -1; i <= ss->length; ++i)
			{
			    int x, y;

			    x = bx + i * xincr[ss->dir];
			    y = by + i * yincr[ss->dir];
			    if (ONBOARD(x, y))
			    {
				hits[turn][x][y] = MARK_MISS;
				if (turn % 2 == PLAYER)
				{
				    cgoto(y, x);
#ifdef A_COLOR
				    if (has_colors())
					attron(COLOR_PAIR(COLOR_GREEN));
#endif /* A_COLOR */
				    (void)addch(MARK_MISS);
#ifdef A_COLOR
				    attrset(0);
#endif /* A_COLOR */
				}
			    }
			}
		    }

		for (i = 0; i < ss->length; ++i)
		{
		    int x = ss->x + i * xincr[ss->dir];
		    int y = ss->y + i * yincr[ss->dir];

		    hits[turn][x][y] = ss->symbol;
		    if (turn % 2 == PLAYER)
		    {
			cgoto(y, x);
			(void) addch(ss->symbol);
		    }
		}

		(void) move(oldy, oldx);
		return(ss);
	    }
	}
    (void) move(oldy, oldx);
    return((ship_t *)NULL);
}

static int plyturn()
{
    ship_t *ss;
    bool hit;
    char *m;

    prompt(1, "Where do you want to shoot? ");
    for (;;)
    {
	(void) getcoord(COMPUTER);
	if (hits[PLAYER][curx][cury])
	{
	    prompt(1, "You shelled this spot already! Try again.");
	    beep();
	}
	else
	    break;
    }
    hit = IS_SHIP(board[COMPUTER][curx][cury]);
    hits[PLAYER][curx][cury] = hit ? MARK_HIT : MARK_MISS;
    cgoto(cury, curx);
#ifdef A_COLOR
    if (has_colors())
	if (hit)
	    attron(COLOR_PAIR(COLOR_RED));
	else
	    attron(COLOR_PAIR(COLOR_GREEN));
#endif /* A_COLOR */
    (void) addch((chtype)hits[PLAYER][curx][cury]);
#ifdef A_COLOR
    attrset(0);
#endif /* A_COLOR */

    prompt(1, "You %s.", hit ? "scored a hit" : "missed");
    if(hit && (ss = hitship(curx, cury)))
    {
	switch(rnd(5))
	{
	case 0:
	    m = " You sank my %s!";
	    break;
	case 1:
	    m = " I have this sinking feeling about my %s....";
	    break;
	case 2:
	    m = " My %s has gone to Davy Jones's locker!";
	    break;
	case 3:
	    m = " Glub, glub -- my %s is headed for the bottom!";
	    break;
	case 4:
	    m = " You'll pick up survivors from my my %s, I hope...!";
	    break;
	}
	(void)printw(m, ss->name);
	(void)beep();
	return(awinna() == -1);
    }
    return(hit);
}

static int sgetc(s)
char *s;
{
    char *s1;
    int ch;

    (void)refresh();
    for(;;)
    {
	ch = getch();
	if (islower(ch))
	    ch = toupper(ch);
	if (ch == CTRLC)
	    uninitgame();
	for (s1=s; *s1 && ch != *s1; ++s1)
	    continue;
	if (*s1)
	{
	    (void) addch((chtype)ch);
	    (void)refresh();
	    return(ch);
	    }
	}
}


static void randomfire(px, py)
/* random-fire routine -- implements simple diagonal-striping strategy */
int	*px, *py;
{
    static int turncount = 0;
    static int srchstep = BEGINSTEP;
    static int huntoffs;		/* Offset on search strategy */
    int ypossible[BWIDTH * BDEPTH], xpossible[BWIDTH * BDEPTH], nposs;
    int ypreferred[BWIDTH * BDEPTH], xpreferred[BWIDTH * BDEPTH], npref;
    int x, y, i;

    if (turncount++ == 0)
	huntoffs = rnd(srchstep);

    /* first, list all possible moves */
    nposs = npref = 0;
    for (x = 0; x < BWIDTH; x++)
	for (y = 0; y < BDEPTH; y++)
	    if (!hits[COMPUTER][x][y])
	    {
		xpossible[nposs] = x;
		ypossible[nposs] = y;
		nposs++;
		if (((x+huntoffs) % srchstep) != (y % srchstep))
		{
		    xpreferred[npref] = x;
		    ypreferred[npref] = y;
		    npref++;
		}
	    }

    if (npref)
    {
	i = rnd(npref);

	*px = xpreferred[i];
	*py = ypreferred[i];
    }
    else if (nposs)
    {
	i = rnd(nposs);

	*px = xpossible[i];
	*py = ypossible[i];

	if (srchstep > 1)
	    --srchstep;
    }
    else
    {
	error("No moves possible?? Help!");
	exit(1);
	/*NOTREACHED*/
    }
}

#define S_MISS	0
#define S_HIT	1
#define S_SUNK	-1

static bool cpufire(x, y)
/* fire away at given location */
int	x, y;
{
    bool hit, sunk;
    ship_t *ss;

    hits[COMPUTER][x][y] = (hit = (board[PLAYER][x][y])) ? MARK_HIT : MARK_MISS;
    (void) mvprintw(PROMPTLINE, 0,
	"I shoot at %c%d. I %s!", y + 'A', x, hit ? "hit" : "miss");
    if (sunk = (hit && (ss = hitship(x, y))))
	(void) printw(" I've sunk your %s", ss->name);
    (void)clrtoeol();

    pgoto(y, x);
#ifdef A_COLOR
    if (has_colors())
	if (hit)
	    attron(COLOR_PAIR(COLOR_RED));
	else
	    attron(COLOR_PAIR(COLOR_GREEN));
#endif /* A_COLOR */
    (void)addch((chtype)(hit ? SHOWHIT : SHOWSPLASH));
#ifdef A_COLOR
    attrset(0);
#endif /* A_COLOR */

    return(hit ? (sunk ? S_SUNK : S_HIT) : S_MISS);
}

/*
 * This code implements a fairly irregular FSM, so please forgive the rampant
 * unstructuredness below. The five labels are states which need to be held
 * between computer turns.
 */
static bool cputurn()
{
#define POSSIBLE(x, y)	(ONBOARD(x, y) && !hits[COMPUTER][x][y])
#define RANDOM_FIRE	0
#define RANDOM_HIT	1
#define HUNT_DIRECT	2
#define FIRST_PASS	3
#define REVERSE_JUMP	4
#define SECOND_PASS	5
    static int next = RANDOM_FIRE;
    static bool used[4];
    static ship_t ts;
    int navail, x, y, d, n, hit = S_MISS;

    switch(next)
    {
    case RANDOM_FIRE:	/* last shot was random and missed */
    refire:
	randomfire(&x, &y);
	if (!(hit = cpufire(x, y)))
	    next = RANDOM_FIRE;
	else
	{
	    ts.x = x; ts.y = y;
	    ts.hits = 1;
	    next = (hit == S_SUNK) ? RANDOM_FIRE : RANDOM_HIT;
	}
	break;

    case RANDOM_HIT:	/* last shot was random and hit */
	used[E/2] = used[S/2] = used[W/2] = used[N/2] = FALSE;
	/* FALLTHROUGH */

    case HUNT_DIRECT:	/* last shot hit, we're looking for ship's long axis */
	for (d = navail = 0; d < 4; d++)
	{
	    x = ts.x + xincr[d*2]; y = ts.y + yincr[d*2];
	    if (!used[d] && POSSIBLE(x, y))
		navail++;
	    else
		used[d] = TRUE;
	}
	if (navail == 0)	/* no valid places for shots adjacent... */
	    goto refire;	/* ...so we must random-fire */
	else
	{
	    for (d = 0, n = rnd(navail) + 1; n; n--)
		while (used[d])
		    d++;

	    assert(d <= 4);

	    used[d] = FALSE;
	    x = ts.x + xincr[d*2];
	    y = ts.y + yincr[d*2];

	    assert(POSSIBLE(x, y));

	    if (!(hit = cpufire(x, y)))
		next = HUNT_DIRECT;
	    else
	    {
		ts.x = x; ts.y = y; ts.dir = d*2; ts.hits++;
		next = (hit == S_SUNK) ? RANDOM_FIRE : FIRST_PASS;
	    }
	}
	break;

    case FIRST_PASS:	/* we have a start and a direction now */
	x = ts.x + xincr[ts.dir];
	y = ts.y + yincr[ts.dir];
	if (POSSIBLE(x, y) && (hit = cpufire(x, y)))
	{
	    ts.x = x; ts.y = y; ts.hits++;
	    next = (hit == S_SUNK) ? RANDOM_FIRE : FIRST_PASS;
	}
	else
	    next = REVERSE_JUMP;
	break;

    case REVERSE_JUMP:	/* nail down the ship's other end */
	d = ts.dir + 4;
	x = ts.x + ts.hits * xincr[d];
	y = ts.y + ts.hits * yincr[d];
	if (POSSIBLE(x, y) && (hit = cpufire(x, y)))
	{
	    ts.x = x; ts.y = y; ts.dir = d; ts.hits++;
	    next = (hit == S_SUNK) ? RANDOM_FIRE : SECOND_PASS;
	}
	else
	    next = RANDOM_FIRE;
	break;

    case SECOND_PASS:	/* kill squares not caught on first pass */
	x = ts.x + xincr[ts.dir];
	y = ts.y + yincr[ts.dir];
	if (POSSIBLE(x, y) && (hit = cpufire(x, y)))
	{
	    ts.x = x; ts.y = y; ts.hits++;
	    next = (hit == S_SUNK) ? RANDOM_FIRE: SECOND_PASS;
	    break;
	}
	else
	    next = RANDOM_FIRE;
	break;
    }

    /* check for continuation and/or winner */
    if (salvo)
    {
	(void)refresh();
	(void)sleep(1);
    }
    if (awinna() != -1)
	return(FALSE);

#ifdef DEBUG
    (void) mvprintw(PROMPTLINE + 2, 0,
		    "New state %d, x=%d, y=%d, d=%d",
		    next, x, y, d);
#endif /* DEBUG */
    return(hit);
}

playagain()
{
    int j;
    ship_t *ss;

    for (ss = cpuship; ss < cpuship + SHIPTYPES; ss++)
	for(j = 0; j < ss->length; j++)
	{
	    cgoto(ss->y + j * yincr[ss->dir], ss->x + j * xincr[ss->dir]);
	    (void)addch((chtype)ss->symbol);
	}

    if(awinna())
	++cpuwon;
    else
	++plywon;
    j = 18 + strlen(name);
    if(plywon >= 10)
	++j;
    if(cpuwon >= 10)
	++j;
    (void) mvprintw(1,(COLWIDTH-j)/2,
		    "%s: %d     Computer: %d",name,plywon,cpuwon);

    prompt(2, (awinna()) ? "Want to be humiliated again, %s [yn]? "
	   : "Going to give me a chance for revenge, %s [yn]? ",name);
    return(sgetc("YN") == 'Y');
}

static void do_options(c,op)
int c;
char *op[];
{
    register int i;

    if (c > 1)
    {
	for (i=1; i<c; i++)
	{
	    switch(op[i][0])
	    {
	    default:
	    case '?':
		(void) fprintf(stderr, "Usage: battle [-s | -b] [-c]\n");
		(void) fprintf(stderr, "\tWhere the options are:\n");
		(void) fprintf(stderr, "\t-s : play a salvo game\n");
		(void) fprintf(stderr, "\t-b : play a blitz game\n");
		(void) fprintf(stderr, "\t-c : ships may be adjacent\n");
		exit(1);
		break;
	    case '-':
		switch(op[i][1])
		{
		case 'b':
		    blitz = 1;
		    if (salvo == 1)
		    {
			(void) fprintf(stderr,
				"Bad Arg: -b and -s are mutually exclusive\n");
			exit(1);
		    }
		    break;
		case 's':
		    salvo = 1;
		    if (blitz == 1)
		    {
			(void) fprintf(stderr,
				"Bad Arg: -s and -b are mutually exclusive\n");
			exit(1);
		    }
		    break;
		case 'c':
		    closepack = 1;
		    break;
		default:
		    (void) fprintf(stderr,
			    "Bad arg: type \"%s ?\" for usage message\n", op[0]);
		    exit(1);
		}
	    }
	}
    }
}

static int scount(who)
int who;
{
    register int i, shots;
    register ship_t *sp;

    if (who)
	sp = cpuship;	/* count cpu shots */
    else
	sp = plyship;	/* count player shots */

    for (i=0, shots = 0; i < SHIPTYPES; i++, sp++)
    {
	if (sp->hits >= sp->length)
	    continue;		/* dead ship */
	else
	    shots++;
    }
    return(shots);
}

main(argc, argv)
int argc;
char *argv[];
{
    /* revoke privs */
    setegid(getgid());
    setgid(getgid());

    do_options(argc, argv);

    intro();
    do {
	initgame();
	while(awinna() == -1)
	{
	    if (!blitz)
	    {
		if (!salvo)
		{
	    	    if(turn)
			(void) cputurn();
		    else
			(void) plyturn();
		}
		else
		{
		    register int i;

		    i = scount(turn);
		    while (i--)
		    {
			if (turn)
			{
			    if (cputurn() && awinna() != -1)
				i = 0;
			}
			else
			{
			    if (plyturn() && awinna() != -1)
				i = 0;
			}
		    }
		}
	    }
	    else
	    	while(turn ? cputurn() : plyturn())
		    continue;
	    turn = OTHER;
	}
    } while
	(playagain());
    uninitgame();
    /*NOTREACHED*/
}

/* bs.c ends here */
