/* -*- buffer-read-only: t -*-
 *
 *    overload.h
 *
 *    Copyright (C) 1997, 1998, 2000, 2001, 2005, 2006, 2007 by Larry Wall
 *    and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *  !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 *  This file is built by overload.pl
 */

enum {
    fallback_amg,
    to_sv_amg,
    to_av_amg,
    to_hv_amg,
    to_gv_amg,
    to_cv_amg,
    inc_amg,
    dec_amg,
    bool__amg,
    numer_amg,
    string_amg,
    not_amg,
    copy_amg,
    abs_amg,
    neg_amg,
    iter_amg,
    int_amg,
    lt_amg,
    le_amg,
    gt_amg,
    ge_amg,
    eq_amg,
    ne_amg,
    slt_amg,
    sle_amg,
    sgt_amg,
    sge_amg,
    seq_amg,
    sne_amg,
    nomethod_amg,
    add_amg,
    add_ass_amg,
    subtr_amg,
    subtr_ass_amg,
    mult_amg,
    mult_ass_amg,
    div_amg,
    div_ass_amg,
    modulo_amg,
    modulo_ass_amg,
    pow_amg,
    pow_ass_amg,
    lshift_amg,
    lshift_ass_amg,
    rshift_amg,
    rshift_ass_amg,
    band_amg,
    band_ass_amg,
    bor_amg,
    bor_ass_amg,
    bxor_amg,
    bxor_ass_amg,
    ncmp_amg,
    scmp_amg,
    compl_amg,
    atan2_amg,
    cos_amg,
    sin_amg,
    exp_amg,
    log_amg,
    sqrt_amg,
    repeat_amg,
    repeat_ass_amg,
    concat_amg,
    concat_ass_amg,
    smart_amg,
    DESTROY_amg,
    max_amg_code
    /* Do not leave a trailing comma here.  C9X allows it, C89 doesn't. */
};

#define NofAMmeth max_amg_code

