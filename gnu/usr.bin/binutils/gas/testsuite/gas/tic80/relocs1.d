#objdump: -d
#name: TIc80 simple relocs, global/local funcs & branches (code)

.*: +file format .*tic80.*

Disassembly of section .text:

00000000 <_sfunc>:
   0:	f0 ff 6c 08.*
   4:	0c 00 59 f8.*
   8:	00 00 59 10.*
   c:	00 90 38 f8 00 00 00 00.*
  14:	00 00 51 10.*
  18:	0c 00 51 f8.*
  1c:	1f 80 38 00.*
  20:	10 80 6c 08.*

00000024 <_gfunc>:
  24:	f0 ff 6c 08.*
  28:	0c 00 59 f8.*
  2c:	00 00 59 10.*
  30:	00 90 38 f8 00 00 00 00.*
  38:	00 00 51 10.*
  3c:	0c 00 51 f8.*
  40:	1f 80 38 00.*
  44:	10 80 6c 08.*

00000048 <_branches>:
  48:	f0 ff 6c 08.*
  4c:	0c 00 59 f8.*
  50:	00 00 59 10.*
  54:	00 00 51 10.*
  58:	04 00 59 10.*
  5c:	00 00 51 10.*
  60:	04 00 51 18.*
  64:	0a 80 ac 10.*
  68:	03 00 ba 10.*
  6c:	12 80 a5 30.*
  70:	04 00 51 10.*
  74:	05 80 a4 f8.*
  78:	00 90 38 f8 24 00 00 00.*
  80:	04 00 51 10.*
  84:	04 80 24 00.*
  88:	00 90 38 f8 00 00 00 00.*
  90:	04 00 51 10.*
  94:	04 00 51 10.*
  98:	01 80 ac 10.*
  9c:	04 00 59 10.*
  a0:	00 00 51 18.*
  a4:	04 00 51 10.*
  a8:	0a 80 ec 18.*
  ac:	02 00 fa 10.*
  b0:	f0 ff a5 38.*
  b4:	0c 00 51 f8.*
  b8:	1f 80 38 00.*
  bc:	10 80 6c 08.*
