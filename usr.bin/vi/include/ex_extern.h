/*	$OpenBSD: ex_extern.h,v 1.9 2009/06/10 14:03:18 millert Exp $	*/

int ex(SCR **);
int ex_cmd(SCR *);
int ex_range(SCR *, EXCMD *, int *);
int ex_is_abbrev(char *, size_t);
int ex_is_unmap(char *, size_t);
void ex_badaddr
(SCR *, EXCMDLIST const *, enum badaddr, enum nresult);
int ex_abbr(SCR *, EXCMD *);
int ex_unabbr(SCR *, EXCMD *);
int ex_append(SCR *, EXCMD *);
int ex_change(SCR *, EXCMD *);
int ex_insert(SCR *, EXCMD *);
int ex_next(SCR *, EXCMD *);
int ex_prev(SCR *, EXCMD *);
int ex_rew(SCR *, EXCMD *);
int ex_args(SCR *, EXCMD *);
char **ex_buildargv(SCR *, EXCMD *, char *);
int argv_init(SCR *, EXCMD *);
int argv_exp0(SCR *, EXCMD *, char *, size_t);
int argv_exp1(SCR *, EXCMD *, char *, size_t, int);
int argv_exp2(SCR *, EXCMD *, char *, size_t);
int argv_exp3(SCR *, EXCMD *, char *, size_t);
int argv_free(SCR *);
int ex_at(SCR *, EXCMD *);
int ex_bang(SCR *, EXCMD *);
int ex_cd(SCR *, EXCMD *);
int ex_cscope(SCR *, EXCMD *);
int cscope_display(SCR *);
int cscope_search(SCR *, TAGQ *, TAG *);
int ex_delete(SCR *, EXCMD *);
int ex_display(SCR *, EXCMD *);
int ex_edit(SCR *, EXCMD *);
int ex_equal(SCR *, EXCMD *);
int ex_file(SCR *, EXCMD *);
int ex_filter(SCR *, EXCMD *, MARK *, MARK *, MARK *, char *, enum filtertype);
int ex_global(SCR *, EXCMD *);
int ex_v(SCR *, EXCMD *);
int ex_g_insdel(SCR *, lnop_t, recno_t);
int ex_screen_copy(SCR *, SCR *);
int ex_screen_end(SCR *);
int ex_optchange(SCR *, int, char *, u_long *);
int ex_exrc(SCR *);
int ex_run_str(SCR *, char *, char *, size_t, int, int);
int ex_join(SCR *, EXCMD *);
int ex_map(SCR *, EXCMD *);
int ex_unmap(SCR *, EXCMD *);
int ex_mark(SCR *, EXCMD *);
int ex_mkexrc(SCR *, EXCMD *);
int ex_copy(SCR *, EXCMD *);
int ex_move(SCR *, EXCMD *);
int ex_open(SCR *, EXCMD *);
int ex_perl(SCR*, EXCMD *);
int ex_preserve(SCR *, EXCMD *);
int ex_recover(SCR *, EXCMD *);
int ex_list(SCR *, EXCMD *);
int ex_number(SCR *, EXCMD *);
int ex_pr(SCR *, EXCMD *);
int ex_print(SCR *, EXCMD *, MARK *, MARK *, u_int32_t);
int ex_ldisplay(SCR *, const char *, size_t, size_t, u_int);
int ex_scprint(SCR *, MARK *, MARK *);
int ex_printf(SCR *, const char *, ...);
int ex_puts(SCR *, const char *);
int ex_fflush(SCR *sp);
int ex_put(SCR *, EXCMD *);
int ex_quit(SCR *, EXCMD *);
int ex_read(SCR *, EXCMD *);
int ex_readfp(SCR *, char *, FILE *, MARK *, recno_t *, int);
int ex_bg(SCR *, EXCMD *);
int ex_fg(SCR *, EXCMD *);
int ex_resize(SCR *, EXCMD *);
int ex_sdisplay(SCR *);
int ex_script(SCR *, EXCMD *);
int sscr_exec(SCR *, recno_t);
int sscr_check_input(SCR *);
int sscr_input(SCR *);
int sscr_end(SCR *);
int ex_set(SCR *, EXCMD *);
int ex_shell(SCR *, EXCMD *);
int ex_exec_proc(SCR *, EXCMD *, char *, const char *, int);
int proc_wait(SCR *, pid_t, const char *, int, int);
int ex_shiftl(SCR *, EXCMD *);
int ex_shiftr(SCR *, EXCMD *);
int ex_source(SCR *, EXCMD *);
int ex_stop(SCR *, EXCMD *);
int ex_s(SCR *, EXCMD *);
int ex_subagain(SCR *, EXCMD *);
int ex_subtilde(SCR *, EXCMD *);
int re_compile(SCR *, char *, size_t, char **, size_t *, regex_t *, u_int);
void re_error(SCR *, int, regex_t *);
int ex_tag_first(SCR *, char *);
int ex_tag_push(SCR *, EXCMD *);
int ex_tag_next(SCR *, EXCMD *);
int ex_tag_prev(SCR *, EXCMD *);
int ex_tag_nswitch(SCR *, TAG *, int);
int ex_tag_Nswitch(SCR *, TAG *, int);
int ex_tag_pop(SCR *, EXCMD *);
int ex_tag_top(SCR *, EXCMD *);
int ex_tag_display(SCR *);
int ex_tag_copy(SCR *, SCR *);
int tagq_free(SCR *, TAGQ *);
void tag_msg(SCR *, tagmsg_t, char *);
int ex_tagf_alloc(SCR *, char *);
int ex_tag_free(SCR *);
int ex_tcl(SCR*, EXCMD *);
int ex_txt(SCR *, TEXTH *, ARG_CHAR_T, u_int32_t);
int ex_undo(SCR *, EXCMD *);
int ex_help(SCR *, EXCMD *);
int ex_usage(SCR *, EXCMD *);
int ex_viusage(SCR *, EXCMD *);
void ex_cinit(EXCMD *, int, int, recno_t, recno_t, int, ARGS **);
void ex_cadd(EXCMD *, ARGS *, char *, size_t);
int ex_getline(SCR *, FILE *, size_t *);
int ex_ncheck(SCR *, int);
int ex_init(SCR *);
void ex_emsg(SCR *, char *, exm_t);
int ex_version(SCR *, EXCMD *);
int ex_visual(SCR *, EXCMD *);
int ex_wn(SCR *, EXCMD *);
int ex_wq(SCR *, EXCMD *);
int ex_write(SCR *, EXCMD *);
int ex_xit(SCR *, EXCMD *);
int ex_writefp(SCR *, char *, FILE *, MARK *, MARK *, u_long *, u_long *, int);
int ex_yank(SCR *, EXCMD *);
int ex_z(SCR *, EXCMD *);
