# name: OFFSET_IMM regression
# as:
# objdump: -dr --prefix-addresses --show-raw-insn

.*: +file format .*arm.*

Disassembly of section .text:
0+0 <[^>]+> e51f0004 ?	ldr	r0, \[pc, #-4\]	; 0+4 <[^>]+>
0+4 <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
0+8 <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
0+c <[^>]+> e1a00000 ?	nop			\(mov r0,r0\)
