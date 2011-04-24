/* Instruction opcode header for frv.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.

*/

#ifndef FRV_OPC_H
#define FRV_OPC_H

/* -- opc.h */

#undef  CGEN_DIS_HASH_SIZE
#define CGEN_DIS_HASH_SIZE 128
#undef  CGEN_DIS_HASH
#define CGEN_DIS_HASH(buffer, value) (((value) >> 18) & 127)

/* Allows reason codes to be output when assembler errors occur.  */
#define CGEN_VERBOSE_ASSEMBLER_ERRORS

/* Vliw support.  */
#define FRV_VLIW_SIZE 8 /* fr550 has largest vliw size of 8.  */
#define PAD_VLIW_COMBO ,UNIT_NIL,UNIT_NIL,UNIT_NIL,UNIT_NIL

typedef CGEN_ATTR_VALUE_ENUM_TYPE VLIW_COMBO[FRV_VLIW_SIZE];

typedef struct
{
  int                    next_slot;
  int                    constraint_violation;
  unsigned long          mach;
  unsigned long          elf_flags;
  CGEN_ATTR_VALUE_ENUM_TYPE * unit_mapping;
  VLIW_COMBO *           current_vliw;
  CGEN_ATTR_VALUE_ENUM_TYPE   major[FRV_VLIW_SIZE];
  const CGEN_INSN *      insn[FRV_VLIW_SIZE];
} FRV_VLIW;

int frv_is_branch_major (CGEN_ATTR_VALUE_ENUM_TYPE, unsigned long);
int frv_is_float_major  (CGEN_ATTR_VALUE_ENUM_TYPE, unsigned long);
int frv_is_media_major  (CGEN_ATTR_VALUE_ENUM_TYPE, unsigned long);
int frv_is_branch_insn  (const CGEN_INSN *);
int frv_is_float_insn   (const CGEN_INSN *);
int frv_is_media_insn   (const CGEN_INSN *);
void frv_vliw_reset     (FRV_VLIW *, unsigned long, unsigned long);
int frv_vliw_add_insn   (FRV_VLIW *, const CGEN_INSN *);
int spr_valid           (long);
/* -- */
/* Enum declaration for frv instruction types.  */
typedef enum cgen_insn_type {
  FRV_INSN_INVALID, FRV_INSN_ADD, FRV_INSN_SUB, FRV_INSN_AND
 , FRV_INSN_OR, FRV_INSN_XOR, FRV_INSN_NOT, FRV_INSN_SDIV
 , FRV_INSN_NSDIV, FRV_INSN_UDIV, FRV_INSN_NUDIV, FRV_INSN_SMUL
 , FRV_INSN_UMUL, FRV_INSN_SMU, FRV_INSN_SMASS, FRV_INSN_SMSSS
 , FRV_INSN_SLL, FRV_INSN_SRL, FRV_INSN_SRA, FRV_INSN_SLASS
 , FRV_INSN_SCUTSS, FRV_INSN_SCAN, FRV_INSN_CADD, FRV_INSN_CSUB
 , FRV_INSN_CAND, FRV_INSN_COR, FRV_INSN_CXOR, FRV_INSN_CNOT
 , FRV_INSN_CSMUL, FRV_INSN_CSDIV, FRV_INSN_CUDIV, FRV_INSN_CSLL
 , FRV_INSN_CSRL, FRV_INSN_CSRA, FRV_INSN_CSCAN, FRV_INSN_ADDCC
 , FRV_INSN_SUBCC, FRV_INSN_ANDCC, FRV_INSN_ORCC, FRV_INSN_XORCC
 , FRV_INSN_SLLCC, FRV_INSN_SRLCC, FRV_INSN_SRACC, FRV_INSN_SMULCC
 , FRV_INSN_UMULCC, FRV_INSN_CADDCC, FRV_INSN_CSUBCC, FRV_INSN_CSMULCC
 , FRV_INSN_CANDCC, FRV_INSN_CORCC, FRV_INSN_CXORCC, FRV_INSN_CSLLCC
 , FRV_INSN_CSRLCC, FRV_INSN_CSRACC, FRV_INSN_ADDX, FRV_INSN_SUBX
 , FRV_INSN_ADDXCC, FRV_INSN_SUBXCC, FRV_INSN_ADDSS, FRV_INSN_SUBSS
 , FRV_INSN_ADDI, FRV_INSN_SUBI, FRV_INSN_ANDI, FRV_INSN_ORI
 , FRV_INSN_XORI, FRV_INSN_SDIVI, FRV_INSN_NSDIVI, FRV_INSN_UDIVI
 , FRV_INSN_NUDIVI, FRV_INSN_SMULI, FRV_INSN_UMULI, FRV_INSN_SLLI
 , FRV_INSN_SRLI, FRV_INSN_SRAI, FRV_INSN_SCANI, FRV_INSN_ADDICC
 , FRV_INSN_SUBICC, FRV_INSN_ANDICC, FRV_INSN_ORICC, FRV_INSN_XORICC
 , FRV_INSN_SMULICC, FRV_INSN_UMULICC, FRV_INSN_SLLICC, FRV_INSN_SRLICC
 , FRV_INSN_SRAICC, FRV_INSN_ADDXI, FRV_INSN_SUBXI, FRV_INSN_ADDXICC
 , FRV_INSN_SUBXICC, FRV_INSN_CMPB, FRV_INSN_CMPBA, FRV_INSN_SETLO
 , FRV_INSN_SETHI, FRV_INSN_SETLOS, FRV_INSN_LDSB, FRV_INSN_LDUB
 , FRV_INSN_LDSH, FRV_INSN_LDUH, FRV_INSN_LD, FRV_INSN_LDBF
 , FRV_INSN_LDHF, FRV_INSN_LDF, FRV_INSN_LDC, FRV_INSN_NLDSB
 , FRV_INSN_NLDUB, FRV_INSN_NLDSH, FRV_INSN_NLDUH, FRV_INSN_NLD
 , FRV_INSN_NLDBF, FRV_INSN_NLDHF, FRV_INSN_NLDF, FRV_INSN_LDD
 , FRV_INSN_LDDF, FRV_INSN_LDDC, FRV_INSN_NLDD, FRV_INSN_NLDDF
 , FRV_INSN_LDQ, FRV_INSN_LDQF, FRV_INSN_LDQC, FRV_INSN_NLDQ
 , FRV_INSN_NLDQF, FRV_INSN_LDSBU, FRV_INSN_LDUBU, FRV_INSN_LDSHU
 , FRV_INSN_LDUHU, FRV_INSN_LDU, FRV_INSN_NLDSBU, FRV_INSN_NLDUBU
 , FRV_INSN_NLDSHU, FRV_INSN_NLDUHU, FRV_INSN_NLDU, FRV_INSN_LDBFU
 , FRV_INSN_LDHFU, FRV_INSN_LDFU, FRV_INSN_LDCU, FRV_INSN_NLDBFU
 , FRV_INSN_NLDHFU, FRV_INSN_NLDFU, FRV_INSN_LDDU, FRV_INSN_NLDDU
 , FRV_INSN_LDDFU, FRV_INSN_LDDCU, FRV_INSN_NLDDFU, FRV_INSN_LDQU
 , FRV_INSN_NLDQU, FRV_INSN_LDQFU, FRV_INSN_LDQCU, FRV_INSN_NLDQFU
 , FRV_INSN_LDSBI, FRV_INSN_LDSHI, FRV_INSN_LDI, FRV_INSN_LDUBI
 , FRV_INSN_LDUHI, FRV_INSN_LDBFI, FRV_INSN_LDHFI, FRV_INSN_LDFI
 , FRV_INSN_NLDSBI, FRV_INSN_NLDUBI, FRV_INSN_NLDSHI, FRV_INSN_NLDUHI
 , FRV_INSN_NLDI, FRV_INSN_NLDBFI, FRV_INSN_NLDHFI, FRV_INSN_NLDFI
 , FRV_INSN_LDDI, FRV_INSN_LDDFI, FRV_INSN_NLDDI, FRV_INSN_NLDDFI
 , FRV_INSN_LDQI, FRV_INSN_LDQFI, FRV_INSN_NLDQFI, FRV_INSN_STB
 , FRV_INSN_STH, FRV_INSN_ST, FRV_INSN_STBF, FRV_INSN_STHF
 , FRV_INSN_STF, FRV_INSN_STC, FRV_INSN_STD, FRV_INSN_STDF
 , FRV_INSN_STDC, FRV_INSN_STQ, FRV_INSN_STQF, FRV_INSN_STQC
 , FRV_INSN_STBU, FRV_INSN_STHU, FRV_INSN_STU, FRV_INSN_STBFU
 , FRV_INSN_STHFU, FRV_INSN_STFU, FRV_INSN_STCU, FRV_INSN_STDU
 , FRV_INSN_STDFU, FRV_INSN_STDCU, FRV_INSN_STQU, FRV_INSN_STQFU
 , FRV_INSN_STQCU, FRV_INSN_CLDSB, FRV_INSN_CLDUB, FRV_INSN_CLDSH
 , FRV_INSN_CLDUH, FRV_INSN_CLD, FRV_INSN_CLDBF, FRV_INSN_CLDHF
 , FRV_INSN_CLDF, FRV_INSN_CLDD, FRV_INSN_CLDDF, FRV_INSN_CLDQ
 , FRV_INSN_CLDSBU, FRV_INSN_CLDUBU, FRV_INSN_CLDSHU, FRV_INSN_CLDUHU
 , FRV_INSN_CLDU, FRV_INSN_CLDBFU, FRV_INSN_CLDHFU, FRV_INSN_CLDFU
 , FRV_INSN_CLDDU, FRV_INSN_CLDDFU, FRV_INSN_CLDQU, FRV_INSN_CSTB
 , FRV_INSN_CSTH, FRV_INSN_CST, FRV_INSN_CSTBF, FRV_INSN_CSTHF
 , FRV_INSN_CSTF, FRV_INSN_CSTD, FRV_INSN_CSTDF, FRV_INSN_CSTQ
 , FRV_INSN_CSTBU, FRV_INSN_CSTHU, FRV_INSN_CSTU, FRV_INSN_CSTBFU
 , FRV_INSN_CSTHFU, FRV_INSN_CSTFU, FRV_INSN_CSTDU, FRV_INSN_CSTDFU
 , FRV_INSN_STBI, FRV_INSN_STHI, FRV_INSN_STI, FRV_INSN_STBFI
 , FRV_INSN_STHFI, FRV_INSN_STFI, FRV_INSN_STDI, FRV_INSN_STDFI
 , FRV_INSN_STQI, FRV_INSN_STQFI, FRV_INSN_SWAP, FRV_INSN_SWAPI
 , FRV_INSN_CSWAP, FRV_INSN_MOVGF, FRV_INSN_MOVFG, FRV_INSN_MOVGFD
 , FRV_INSN_MOVFGD, FRV_INSN_MOVGFQ, FRV_INSN_MOVFGQ, FRV_INSN_CMOVGF
 , FRV_INSN_CMOVFG, FRV_INSN_CMOVGFD, FRV_INSN_CMOVFGD, FRV_INSN_MOVGS
 , FRV_INSN_MOVSG, FRV_INSN_BRA, FRV_INSN_BNO, FRV_INSN_BEQ
 , FRV_INSN_BNE, FRV_INSN_BLE, FRV_INSN_BGT, FRV_INSN_BLT
 , FRV_INSN_BGE, FRV_INSN_BLS, FRV_INSN_BHI, FRV_INSN_BC
 , FRV_INSN_BNC, FRV_INSN_BN, FRV_INSN_BP, FRV_INSN_BV
 , FRV_INSN_BNV, FRV_INSN_FBRA, FRV_INSN_FBNO, FRV_INSN_FBNE
 , FRV_INSN_FBEQ, FRV_INSN_FBLG, FRV_INSN_FBUE, FRV_INSN_FBUL
 , FRV_INSN_FBGE, FRV_INSN_FBLT, FRV_INSN_FBUGE, FRV_INSN_FBUG
 , FRV_INSN_FBLE, FRV_INSN_FBGT, FRV_INSN_FBULE, FRV_INSN_FBU
 , FRV_INSN_FBO, FRV_INSN_BCTRLR, FRV_INSN_BRALR, FRV_INSN_BNOLR
 , FRV_INSN_BEQLR, FRV_INSN_BNELR, FRV_INSN_BLELR, FRV_INSN_BGTLR
 , FRV_INSN_BLTLR, FRV_INSN_BGELR, FRV_INSN_BLSLR, FRV_INSN_BHILR
 , FRV_INSN_BCLR, FRV_INSN_BNCLR, FRV_INSN_BNLR, FRV_INSN_BPLR
 , FRV_INSN_BVLR, FRV_INSN_BNVLR, FRV_INSN_FBRALR, FRV_INSN_FBNOLR
 , FRV_INSN_FBEQLR, FRV_INSN_FBNELR, FRV_INSN_FBLGLR, FRV_INSN_FBUELR
 , FRV_INSN_FBULLR, FRV_INSN_FBGELR, FRV_INSN_FBLTLR, FRV_INSN_FBUGELR
 , FRV_INSN_FBUGLR, FRV_INSN_FBLELR, FRV_INSN_FBGTLR, FRV_INSN_FBULELR
 , FRV_INSN_FBULR, FRV_INSN_FBOLR, FRV_INSN_BCRALR, FRV_INSN_BCNOLR
 , FRV_INSN_BCEQLR, FRV_INSN_BCNELR, FRV_INSN_BCLELR, FRV_INSN_BCGTLR
 , FRV_INSN_BCLTLR, FRV_INSN_BCGELR, FRV_INSN_BCLSLR, FRV_INSN_BCHILR
 , FRV_INSN_BCCLR, FRV_INSN_BCNCLR, FRV_INSN_BCNLR, FRV_INSN_BCPLR
 , FRV_INSN_BCVLR, FRV_INSN_BCNVLR, FRV_INSN_FCBRALR, FRV_INSN_FCBNOLR
 , FRV_INSN_FCBEQLR, FRV_INSN_FCBNELR, FRV_INSN_FCBLGLR, FRV_INSN_FCBUELR
 , FRV_INSN_FCBULLR, FRV_INSN_FCBGELR, FRV_INSN_FCBLTLR, FRV_INSN_FCBUGELR
 , FRV_INSN_FCBUGLR, FRV_INSN_FCBLELR, FRV_INSN_FCBGTLR, FRV_INSN_FCBULELR
 , FRV_INSN_FCBULR, FRV_INSN_FCBOLR, FRV_INSN_JMPL, FRV_INSN_CALLL
 , FRV_INSN_JMPIL, FRV_INSN_CALLIL, FRV_INSN_CALL, FRV_INSN_RETT
 , FRV_INSN_REI, FRV_INSN_TRA, FRV_INSN_TNO, FRV_INSN_TEQ
 , FRV_INSN_TNE, FRV_INSN_TLE, FRV_INSN_TGT, FRV_INSN_TLT
 , FRV_INSN_TGE, FRV_INSN_TLS, FRV_INSN_THI, FRV_INSN_TC
 , FRV_INSN_TNC, FRV_INSN_TN, FRV_INSN_TP, FRV_INSN_TV
 , FRV_INSN_TNV, FRV_INSN_FTRA, FRV_INSN_FTNO, FRV_INSN_FTNE
 , FRV_INSN_FTEQ, FRV_INSN_FTLG, FRV_INSN_FTUE, FRV_INSN_FTUL
 , FRV_INSN_FTGE, FRV_INSN_FTLT, FRV_INSN_FTUGE, FRV_INSN_FTUG
 , FRV_INSN_FTLE, FRV_INSN_FTGT, FRV_INSN_FTULE, FRV_INSN_FTU
 , FRV_INSN_FTO, FRV_INSN_TIRA, FRV_INSN_TINO, FRV_INSN_TIEQ
 , FRV_INSN_TINE, FRV_INSN_TILE, FRV_INSN_TIGT, FRV_INSN_TILT
 , FRV_INSN_TIGE, FRV_INSN_TILS, FRV_INSN_TIHI, FRV_INSN_TIC
 , FRV_INSN_TINC, FRV_INSN_TIN, FRV_INSN_TIP, FRV_INSN_TIV
 , FRV_INSN_TINV, FRV_INSN_FTIRA, FRV_INSN_FTINO, FRV_INSN_FTINE
 , FRV_INSN_FTIEQ, FRV_INSN_FTILG, FRV_INSN_FTIUE, FRV_INSN_FTIUL
 , FRV_INSN_FTIGE, FRV_INSN_FTILT, FRV_INSN_FTIUGE, FRV_INSN_FTIUG
 , FRV_INSN_FTILE, FRV_INSN_FTIGT, FRV_INSN_FTIULE, FRV_INSN_FTIU
 , FRV_INSN_FTIO, FRV_INSN_BREAK, FRV_INSN_MTRAP, FRV_INSN_ANDCR
 , FRV_INSN_ORCR, FRV_INSN_XORCR, FRV_INSN_NANDCR, FRV_INSN_NORCR
 , FRV_INSN_ANDNCR, FRV_INSN_ORNCR, FRV_INSN_NANDNCR, FRV_INSN_NORNCR
 , FRV_INSN_NOTCR, FRV_INSN_CKRA, FRV_INSN_CKNO, FRV_INSN_CKEQ
 , FRV_INSN_CKNE, FRV_INSN_CKLE, FRV_INSN_CKGT, FRV_INSN_CKLT
 , FRV_INSN_CKGE, FRV_INSN_CKLS, FRV_INSN_CKHI, FRV_INSN_CKC
 , FRV_INSN_CKNC, FRV_INSN_CKN, FRV_INSN_CKP, FRV_INSN_CKV
 , FRV_INSN_CKNV, FRV_INSN_FCKRA, FRV_INSN_FCKNO, FRV_INSN_FCKNE
 , FRV_INSN_FCKEQ, FRV_INSN_FCKLG, FRV_INSN_FCKUE, FRV_INSN_FCKUL
 , FRV_INSN_FCKGE, FRV_INSN_FCKLT, FRV_INSN_FCKUGE, FRV_INSN_FCKUG
 , FRV_INSN_FCKLE, FRV_INSN_FCKGT, FRV_INSN_FCKULE, FRV_INSN_FCKU
 , FRV_INSN_FCKO, FRV_INSN_CCKRA, FRV_INSN_CCKNO, FRV_INSN_CCKEQ
 , FRV_INSN_CCKNE, FRV_INSN_CCKLE, FRV_INSN_CCKGT, FRV_INSN_CCKLT
 , FRV_INSN_CCKGE, FRV_INSN_CCKLS, FRV_INSN_CCKHI, FRV_INSN_CCKC
 , FRV_INSN_CCKNC, FRV_INSN_CCKN, FRV_INSN_CCKP, FRV_INSN_CCKV
 , FRV_INSN_CCKNV, FRV_INSN_CFCKRA, FRV_INSN_CFCKNO, FRV_INSN_CFCKNE
 , FRV_INSN_CFCKEQ, FRV_INSN_CFCKLG, FRV_INSN_CFCKUE, FRV_INSN_CFCKUL
 , FRV_INSN_CFCKGE, FRV_INSN_CFCKLT, FRV_INSN_CFCKUGE, FRV_INSN_CFCKUG
 , FRV_INSN_CFCKLE, FRV_INSN_CFCKGT, FRV_INSN_CFCKULE, FRV_INSN_CFCKU
 , FRV_INSN_CFCKO, FRV_INSN_CJMPL, FRV_INSN_CCALLL, FRV_INSN_ICI
 , FRV_INSN_DCI, FRV_INSN_ICEI, FRV_INSN_DCEI, FRV_INSN_DCF
 , FRV_INSN_DCEF, FRV_INSN_WITLB, FRV_INSN_WDTLB, FRV_INSN_ITLBI
 , FRV_INSN_DTLBI, FRV_INSN_ICPL, FRV_INSN_DCPL, FRV_INSN_ICUL
 , FRV_INSN_DCUL, FRV_INSN_BAR, FRV_INSN_MEMBAR, FRV_INSN_LRAI
 , FRV_INSN_LRAD, FRV_INSN_TLBPR, FRV_INSN_COP1, FRV_INSN_COP2
 , FRV_INSN_CLRGR, FRV_INSN_CLRFR, FRV_INSN_CLRGA, FRV_INSN_CLRFA
 , FRV_INSN_COMMITGR, FRV_INSN_COMMITFR, FRV_INSN_COMMITGA, FRV_INSN_COMMITFA
 , FRV_INSN_FITOS, FRV_INSN_FSTOI, FRV_INSN_FITOD, FRV_INSN_FDTOI
 , FRV_INSN_FDITOS, FRV_INSN_FDSTOI, FRV_INSN_NFDITOS, FRV_INSN_NFDSTOI
 , FRV_INSN_CFITOS, FRV_INSN_CFSTOI, FRV_INSN_NFITOS, FRV_INSN_NFSTOI
 , FRV_INSN_FMOVS, FRV_INSN_FMOVD, FRV_INSN_FDMOVS, FRV_INSN_CFMOVS
 , FRV_INSN_FNEGS, FRV_INSN_FNEGD, FRV_INSN_FDNEGS, FRV_INSN_CFNEGS
 , FRV_INSN_FABSS, FRV_INSN_FABSD, FRV_INSN_FDABSS, FRV_INSN_CFABSS
 , FRV_INSN_FSQRTS, FRV_INSN_FDSQRTS, FRV_INSN_NFDSQRTS, FRV_INSN_FSQRTD
 , FRV_INSN_CFSQRTS, FRV_INSN_NFSQRTS, FRV_INSN_FADDS, FRV_INSN_FSUBS
 , FRV_INSN_FMULS, FRV_INSN_FDIVS, FRV_INSN_FADDD, FRV_INSN_FSUBD
 , FRV_INSN_FMULD, FRV_INSN_FDIVD, FRV_INSN_CFADDS, FRV_INSN_CFSUBS
 , FRV_INSN_CFMULS, FRV_INSN_CFDIVS, FRV_INSN_NFADDS, FRV_INSN_NFSUBS
 , FRV_INSN_NFMULS, FRV_INSN_NFDIVS, FRV_INSN_FCMPS, FRV_INSN_FCMPD
 , FRV_INSN_CFCMPS, FRV_INSN_FDCMPS, FRV_INSN_FMADDS, FRV_INSN_FMSUBS
 , FRV_INSN_FMADDD, FRV_INSN_FMSUBD, FRV_INSN_FDMADDS, FRV_INSN_NFDMADDS
 , FRV_INSN_CFMADDS, FRV_INSN_CFMSUBS, FRV_INSN_NFMADDS, FRV_INSN_NFMSUBS
 , FRV_INSN_FMAS, FRV_INSN_FMSS, FRV_INSN_FDMAS, FRV_INSN_FDMSS
 , FRV_INSN_NFDMAS, FRV_INSN_NFDMSS, FRV_INSN_CFMAS, FRV_INSN_CFMSS
 , FRV_INSN_FMAD, FRV_INSN_FMSD, FRV_INSN_NFMAS, FRV_INSN_NFMSS
 , FRV_INSN_FDADDS, FRV_INSN_FDSUBS, FRV_INSN_FDMULS, FRV_INSN_FDDIVS
 , FRV_INSN_FDSADS, FRV_INSN_FDMULCS, FRV_INSN_NFDMULCS, FRV_INSN_NFDADDS
 , FRV_INSN_NFDSUBS, FRV_INSN_NFDMULS, FRV_INSN_NFDDIVS, FRV_INSN_NFDSADS
 , FRV_INSN_NFDCMPS, FRV_INSN_MHSETLOS, FRV_INSN_MHSETHIS, FRV_INSN_MHDSETS
 , FRV_INSN_MHSETLOH, FRV_INSN_MHSETHIH, FRV_INSN_MHDSETH, FRV_INSN_MAND
 , FRV_INSN_MOR, FRV_INSN_MXOR, FRV_INSN_CMAND, FRV_INSN_CMOR
 , FRV_INSN_CMXOR, FRV_INSN_MNOT, FRV_INSN_CMNOT, FRV_INSN_MROTLI
 , FRV_INSN_MROTRI, FRV_INSN_MWCUT, FRV_INSN_MWCUTI, FRV_INSN_MCUT
 , FRV_INSN_MCUTI, FRV_INSN_MCUTSS, FRV_INSN_MCUTSSI, FRV_INSN_MDCUTSSI
 , FRV_INSN_MAVEH, FRV_INSN_MSLLHI, FRV_INSN_MSRLHI, FRV_INSN_MSRAHI
 , FRV_INSN_MDROTLI, FRV_INSN_MCPLHI, FRV_INSN_MCPLI, FRV_INSN_MSATHS
 , FRV_INSN_MQSATHS, FRV_INSN_MSATHU, FRV_INSN_MCMPSH, FRV_INSN_MCMPUH
 , FRV_INSN_MABSHS, FRV_INSN_MADDHSS, FRV_INSN_MADDHUS, FRV_INSN_MSUBHSS
 , FRV_INSN_MSUBHUS, FRV_INSN_CMADDHSS, FRV_INSN_CMADDHUS, FRV_INSN_CMSUBHSS
 , FRV_INSN_CMSUBHUS, FRV_INSN_MQADDHSS, FRV_INSN_MQADDHUS, FRV_INSN_MQSUBHSS
 , FRV_INSN_MQSUBHUS, FRV_INSN_CMQADDHSS, FRV_INSN_CMQADDHUS, FRV_INSN_CMQSUBHSS
 , FRV_INSN_CMQSUBHUS, FRV_INSN_MQLCLRHS, FRV_INSN_MQLMTHS, FRV_INSN_MQSLLHI
 , FRV_INSN_MQSRAHI, FRV_INSN_MADDACCS, FRV_INSN_MSUBACCS, FRV_INSN_MDADDACCS
 , FRV_INSN_MDSUBACCS, FRV_INSN_MASACCS, FRV_INSN_MDASACCS, FRV_INSN_MMULHS
 , FRV_INSN_MMULHU, FRV_INSN_MMULXHS, FRV_INSN_MMULXHU, FRV_INSN_CMMULHS
 , FRV_INSN_CMMULHU, FRV_INSN_MQMULHS, FRV_INSN_MQMULHU, FRV_INSN_MQMULXHS
 , FRV_INSN_MQMULXHU, FRV_INSN_CMQMULHS, FRV_INSN_CMQMULHU, FRV_INSN_MMACHS
 , FRV_INSN_MMACHU, FRV_INSN_MMRDHS, FRV_INSN_MMRDHU, FRV_INSN_CMMACHS
 , FRV_INSN_CMMACHU, FRV_INSN_MQMACHS, FRV_INSN_MQMACHU, FRV_INSN_CMQMACHS
 , FRV_INSN_CMQMACHU, FRV_INSN_MQXMACHS, FRV_INSN_MQXMACXHS, FRV_INSN_MQMACXHS
 , FRV_INSN_MCPXRS, FRV_INSN_MCPXRU, FRV_INSN_MCPXIS, FRV_INSN_MCPXIU
 , FRV_INSN_CMCPXRS, FRV_INSN_CMCPXRU, FRV_INSN_CMCPXIS, FRV_INSN_CMCPXIU
 , FRV_INSN_MQCPXRS, FRV_INSN_MQCPXRU, FRV_INSN_MQCPXIS, FRV_INSN_MQCPXIU
 , FRV_INSN_MEXPDHW, FRV_INSN_CMEXPDHW, FRV_INSN_MEXPDHD, FRV_INSN_CMEXPDHD
 , FRV_INSN_MPACKH, FRV_INSN_MDPACKH, FRV_INSN_MUNPACKH, FRV_INSN_MDUNPACKH
 , FRV_INSN_MBTOH, FRV_INSN_CMBTOH, FRV_INSN_MHTOB, FRV_INSN_CMHTOB
 , FRV_INSN_MBTOHE, FRV_INSN_CMBTOHE, FRV_INSN_MNOP, FRV_INSN_MCLRACC_0
 , FRV_INSN_MCLRACC_1, FRV_INSN_MRDACC, FRV_INSN_MRDACCG, FRV_INSN_MWTACC
 , FRV_INSN_MWTACCG, FRV_INSN_MCOP1, FRV_INSN_MCOP2, FRV_INSN_FNOP
} CGEN_INSN_TYPE;

/* Index of `invalid' insn place holder.  */
#define CGEN_INSN_INVALID FRV_INSN_INVALID

/* Total number of insns in table.  */
#define MAX_INSNS ((int) FRV_INSN_FNOP + 1)

/* This struct records data prior to insertion or after extraction.  */
struct cgen_fields
{
  int length;
  long f_nil;
  long f_anyof;
  long f_pack;
  long f_op;
  long f_ope1;
  long f_ope2;
  long f_ope3;
  long f_ope4;
  long f_GRi;
  long f_GRj;
  long f_GRk;
  long f_FRi;
  long f_FRj;
  long f_FRk;
  long f_CPRi;
  long f_CPRj;
  long f_CPRk;
  long f_ACCGi;
  long f_ACCGk;
  long f_ACC40Si;
  long f_ACC40Ui;
  long f_ACC40Sk;
  long f_ACC40Uk;
  long f_CRi;
  long f_CRj;
  long f_CRk;
  long f_CCi;
  long f_CRj_int;
  long f_CRj_float;
  long f_ICCi_1;
  long f_ICCi_2;
  long f_ICCi_3;
  long f_FCCi_1;
  long f_FCCi_2;
  long f_FCCi_3;
  long f_FCCk;
  long f_eir;
  long f_s10;
  long f_s12;
  long f_d12;
  long f_u16;
  long f_s16;
  long f_s6;
  long f_s6_1;
  long f_u6;
  long f_s5;
  long f_u12_h;
  long f_u12_l;
  long f_u12;
  long f_int_cc;
  long f_flt_cc;
  long f_cond;
  long f_ccond;
  long f_hint;
  long f_LI;
  long f_lock;
  long f_debug;
  long f_A;
  long f_ae;
  long f_spr_h;
  long f_spr_l;
  long f_spr;
  long f_label16;
  long f_labelH6;
  long f_labelL18;
  long f_label24;
  long f_LRAE;
  long f_LRAD;
  long f_LRAS;
  long f_TLBPRopx;
  long f_TLBPRL;
  long f_ICCi_1_null;
  long f_ICCi_2_null;
  long f_ICCi_3_null;
  long f_FCCi_1_null;
  long f_FCCi_2_null;
  long f_FCCi_3_null;
  long f_rs_null;
  long f_GRi_null;
  long f_GRj_null;
  long f_GRk_null;
  long f_FRi_null;
  long f_FRj_null;
  long f_ACCj_null;
  long f_rd_null;
  long f_cond_null;
  long f_ccond_null;
  long f_s12_null;
  long f_label16_null;
  long f_misc_null_1;
  long f_misc_null_2;
  long f_misc_null_3;
  long f_misc_null_4;
  long f_misc_null_5;
  long f_misc_null_6;
  long f_misc_null_7;
  long f_misc_null_8;
  long f_misc_null_9;
  long f_misc_null_10;
  long f_misc_null_11;
  long f_LRA_null;
  long f_TLBPR_null;
  long f_LI_off;
  long f_LI_on;
  long f_reloc_ann;
};

#define CGEN_INIT_PARSE(od) \
{\
}
#define CGEN_INIT_INSERT(od) \
{\
}
#define CGEN_INIT_EXTRACT(od) \
{\
}
#define CGEN_INIT_PRINT(od) \
{\
}


#endif /* FRV_OPC_H */
