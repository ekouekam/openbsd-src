#as: -mcpu=548 -mfar-mode
#objdump: -d -r
#name: c54x cons tests, w/extended addressing
#source: cons.s

.*: +file format .*c54x.*

Disassembly of section .text:

00000000 <binary>:
   0:	0003.*
   1:	0004.*

00000002 <octal>:
   2:	0009.*
   3:	000a.*
   4:	000b.*

00000005 <hex>:
   5:	000f.*
   6:	0010.*

00000007 <field>:
   7:	6440.*
   8:	0123.*
   9:	4000.*
   a:	0000.*
   b:	1234.*

0000000c <byte>:
   c:	00aa.*
   d:	00bb.*

0000000e <word>:
   e:	0ccc.*

0000000f <xlong>:
   f:	0eee.*
  10:	efff.*
	...

00000012 <long>:
  12:	eeee.*
  13:	ffff.*

00000014 <int>:
  14:	dddd.*

00000015 <xfloat>:
  15:	3fff.*
  16:	ffac.*
	...

00000018 <float>:
  18:	3fff.*
  19:	ffac.*

0000001a <string>:
  1a:	0061.*
  1b:	0062.*
  1c:	0063.*
  1d:	0064.*
  1e:	0061.*
  1f:	0062.*
  20:	0063.*
  21:	0064.*
  22:	0065.*
  23:	0066.*
  24:	0067.*
  25:	0030.*

00000026 <pstring>:
  26:	6162.*
  27:	6364.*
  28:	6162.*
  29:	6364.*
  2a:	6566.*
  2b:	6700.*

0000002c <DAT1>:
  2c:	0000.*
  2d:	abcd.*
  2e:	0000.*
  2f:	0141.*
  30:	0000.*
  31:	0067.*
  32:	0000.*
  33:	006f.*

00000034 <xlong.0>:
  34:	0000.*
.*34: ARELEXT.*
  35:	002c.*
  36:	aabb.*
  37:	ccdd.*

00000038 <DAT2>:
  38:	0000.*
	...

0000003a <DAT3>:
  3a:	1234.*
  3b:	5678.*
  3c:	0000.*
  3d:	aabb.*
  3e:	ccdd.*
