#name: MIPS branch-misc-1
#source: ../../../gas/testsuite/gas/mips/branch-misc-1.s
#objdump: --prefix-addresses -tdr --show-raw-insn
#ld: -Ttext 0x500000 -e 0x500000 -N

.*:     file format elf.*mips.*

#...

Disassembly of section \.text:
	\.\.\.
	\.\.\.
	\.\.\.
0+50003c <[^>]*> 0411fff0 	bal	0+500000 <[^>]*>
0+500040 <[^>]*> 00000000 	nop
0+500044 <[^>]*> 0411fff3 	bal	0+500014 <[^>]*>
0+500048 <[^>]*> 00000000 	nop
0+50004c <[^>]*> 0411fff6 	bal	0+500028 <[^>]*>
0+500050 <[^>]*> 00000000 	nop
0+500054 <[^>]*> 0411000a 	bal	0+500080 <[^>]*>
0+500058 <[^>]*> 00000000 	nop
0+50005c <[^>]*> 0411000d 	bal	0+500094 <[^>]*>
0+500060 <[^>]*> 00000000 	nop
0+500064 <[^>]*> 04110010 	bal	0+5000a8 <[^>]*>
0+500068 <[^>]*> 00000000 	nop
	\.\.\.
	\.\.\.
	\.\.\.
	\.\.\.
#pass
