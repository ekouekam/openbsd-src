/* Support program for testing gdb's ability to call functions
   in the inferior, pass appropriate arguments to those functions,
   and get the returned result. */

#ifdef NO_PROTOTYPES
#define PARAMS(paramlist) ()
#else
#define PARAMS(paramlist) paramlist
#endif

char char_val1 = 'a';
char char_val2 = 'b';

short short_val1 = 10;
short short_val2 = -23;

int int_val1 = 87;
int int_val2 = -26;

long long_val1 = 789;
long long_val2 = -321;

float float_val1 = 3.14159;
float float_val2 = -2.3765;

double double_val1 = 45.654;
double double_val2 = -67.66;

#define DELTA (0.001)

char *string_val1 = "string 1";
char *string_val2 = "string 2";

char char_array_val1[] = "carray 1";
char char_array_val2[] = "carray 2";

struct struct1 {
  char c;
  short s;
  int i;
  long l;
  float f;
  double d;
  char a[4];
} struct_val1 = { 'x', 87, 76, 51, 2.1234, 9.876, "foo" };

/* Some functions that can be passed as arguments to other test
   functions, or called directly. */

int add (a, b)
int a, b;
{
  return (a + b);
}

int doubleit (a)
int a;
{
  return (a + a);
}

int (*func_val1) PARAMS((int,int)) = add;
int (*func_val2) PARAMS((int)) = doubleit;

/* An enumeration and functions that test for specific values. */

enum enumtype { enumval1, enumval2, enumval3 };
enum enumtype enum_val1 = enumval1;
enum enumtype enum_val2 = enumval2;
enum enumtype enum_val3 = enumval3;

t_enum_value1 (enum_arg)
enum enumtype enum_arg;
{
  return (enum_arg == enum_val1);
}

t_enum_value2 (enum_arg)
enum enumtype enum_arg;
{
  return (enum_arg == enum_val2);
}

t_enum_value3 (enum_arg)
enum enumtype enum_arg;
{
  return (enum_arg == enum_val3);
}

/* A function that takes a vector of integers (along with an explicit
   count) and returns their sum. */

int sum_args (argc, argv)
int argc;
int argv[];
{
  int sumval = 0;
  int idx;

  for (idx = 0; idx < argc; idx++)
    {
      sumval += argv[idx];
    }
  return (sumval);
}

/* Test that we can call functions that take structs and return
   members from that struct */

char   t_structs_c (tstruct) struct struct1 tstruct; { return (tstruct.c); }
short  t_structs_s (tstruct) struct struct1 tstruct; { return (tstruct.s); }
int    t_structs_i (tstruct) struct struct1 tstruct; { return (tstruct.i); }
long   t_structs_l (tstruct) struct struct1 tstruct; { return (tstruct.l); }
float  t_structs_f (tstruct) struct struct1 tstruct; { return (tstruct.f); }
double t_structs_d (tstruct) struct struct1 tstruct; { return (tstruct.d); }
char  *t_structs_a (tstruct) struct struct1 tstruct; { return (tstruct.a); }

/* Test that calling functions works if there are a lot of arguments.  */
int
sum10 (i0, i1, i2, i3, i4, i5, i6, i7, i8, i9)
     int i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;
{
  return i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9;
}

/* Gotta have a main to be able to generate a linked, runnable
   executable, and also provide a useful place to set a breakpoint. */

main ()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  malloc(1);
  t_structs_c(struct_val1);
}

/* Functions that expect specific values to be passed and return 
   either 0 or 1, depending upon whether the values were
   passed incorrectly or correctly, respectively. */

int t_char_values (char_arg1, char_arg2)
char char_arg1, char_arg2;
{
  return ((char_arg1 == char_val1) && (char_arg2 == char_val2));
}

int
#ifdef NO_PROTOTYPES
t_small_values (arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
     char arg1;
     short arg2;
     int arg3;
     char arg4;
     short arg5;
     char arg6;
     short arg7;
     int arg8;
     short arg9;
     short arg10;
#else
t_small_values (char arg1, short arg2, int arg3, char arg4, short arg5,
		char arg6, short arg7, int arg8, short arg9, short arg10)
#endif
{
  return arg1 + arg2 + arg3 + arg4 + arg5 + arg6 + arg7 + arg8 + arg9 + arg10;
}

int t_short_values (short_arg1, short_arg2)
short short_arg1, short_arg2;
{
  return ((short_arg1 == short_val1) && (short_arg2 == short_val2));
}

int t_int_values (int_arg1, int_arg2)
int int_arg1, int_arg2;
{
  return ((int_arg1 == int_val1) && (int_arg2 == int_val2));
}

int t_long_values (long_arg1, long_arg2)
long long_arg1, long_arg2;
{
  return ((long_arg1 == long_val1) && (long_arg2 == long_val2));
}

int t_float_values (float_arg1, float_arg2)
float float_arg1, float_arg2;
{
  return ((float_arg1 - float_val1) < DELTA
	  && (float_arg1 - float_val1) > -DELTA
	  && (float_arg2 - float_val2) < DELTA
	  && (float_arg2 - float_val2) > -DELTA);
}

int
#ifdef NO_PROTOTYPES
/* In this case we are just duplicating t_float_values, but that is the
   easiest way to deal with either ANSI or non-ANSI.  */
t_float_values2 (float_arg1, float_arg2)
     float float_arg1, float_arg2;
#else
t_float_values2 (float float_arg1, float float_arg2)
#endif
{
  return ((float_arg1 - float_val1) < DELTA
	  && (float_arg1 - float_val1) > -DELTA
	  && (float_arg2 - float_val2) < DELTA
	  && (float_arg2 - float_val2) > -DELTA);
}

int t_double_values (double_arg1, double_arg2)
double double_arg1, double_arg2;
{
  return ((double_arg1 - double_val1) < DELTA
	  && (double_arg1 - double_val1) > -DELTA
	  && (double_arg2 - double_val2) < DELTA
	  && (double_arg2 - double_val2) > -DELTA);
}

int t_string_values (string_arg1, string_arg2)
char *string_arg1, *string_arg2;
{
  return (!strcmp (string_arg1, string_val1) &&
	  !strcmp (string_arg2, string_val2));
}

int t_char_array_values (char_array_arg1, char_array_arg2)
char char_array_arg1[], char_array_arg2[];
{
  return (!strcmp (char_array_arg1, char_array_val1) &&
	  !strcmp (char_array_arg2, char_array_val2));
}


/* This used to simply compare the function pointer arguments with
   known values for func_val1 and func_val2.  Doing so is valid ANSI
   code, but on some machines (RS6000, HPPA, others?) it may fail when
   called directly by GDB.

   In a nutshell, it's not possible for GDB to determine when the address
   of a function or the address of the function's stub/trampoline should
   be passed.

   So, to avoid GDB lossage in the common case, we perform calls through the
   various function pointers and compare the return values.  For the HPPA
   at least, this allows the common case to work.

   If one wants to try something more complicated, pass the address of
   a function accepting a "double" as one of its first 4 arguments.  Call
   that function indirectly through the function pointer.  This would fail
   on the HPPA.  */

int t_func_values (func_arg1, func_arg2)
int (*func_arg1) PARAMS ((int, int));
int (*func_arg2) PARAMS ((int));
{
  return ((*func_arg1) (5,5)  == (*func_val1) (5,5)
          && (*func_arg2) (6) == (*func_val2) (6));
}

int t_call_add (func_arg1, a, b)
int (*func_arg1) PARAMS ((int, int));
int a, b;
{
  return ((*func_arg1)(a, b));
}
