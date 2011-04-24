	movgs	gr4, psr	; 0x000 00000
	movgs	gr4, pcsr	; 0x001 00001
	movgs	gr4, bpcsr	; 0x002 00002
	movgs	gr4, tbr	; 0x003 00003
	movgs	gr4, bpsr	; 0x004 00004
	movgs	gr4, hsr0	; 0x010 00020
	movgs	gr4, ccr	; 0x100 00400
	movgs	gr4, cccr	; 0x107 00407
	movgs	gr4, lr		; 0x110 00420
	movgs	gr4, lcr	; 0x111 00421
	movgs	gr4, iacc0h	; 0x118 00430
	movgs	gr4, iacc0l	; 0x119 00431
	movgs	gr4, isr	; 0x120 00440
	movgs	gr4, epcr0	; 0x200 01000
	movgs	gr4, esr0	; 0x240 01100
	movgs	gr4, esr14	; 0x24e 01116
	movgs	gr4, esr15	; 0x24f 01117
	movgs	gr4, esfr1	; 0x2a1 01241
	movgs	gr4, scr0	; 0x340 01500
	movgs	gr4, scr1	; 0x341 01501
	movgs	gr4, scr2	; 0x342 01502
	movgs	gr4, scr3	; 0x343 01503
	movgs	gr4, msr0	; 0x500 02400
	movgs	gr4, msr1	; 0x501 02401
	movgs	gr4, ear0	; 0x600 03000
	movgs	gr4, ear15	; 0x60f 03017
	movgs	gr4, iamlr0	; 0x680 03200
	movgs	gr4, iamlr1	; 0x681 03201
	movgs	gr4, iamlr2	; 0x682 03202
	movgs	gr4, iamlr3	; 0x683 03203
	movgs	gr4, iamlr4	; 0x684 03204
	movgs	gr4, iamlr5	; 0x685 03205
	movgs	gr4, iamlr6	; 0x686 03206
	movgs	gr4, iamlr7	; 0x687 03207
	movgs	gr4, iampr0	; 0x6c0 03300
	movgs	gr4, iampr1	; 0x6c1 03301
	movgs	gr4, iampr2	; 0x6c2 03302
	movgs	gr4, iampr3	; 0x6c3 03303
	movgs	gr4, iampr4	; 0x6c4 03304
	movgs	gr4, iampr5	; 0x6c5 03305
	movgs	gr4, iampr6	; 0x6c6 03306
	movgs	gr4, iampr7	; 0x6c7 03307
	movgs	gr4, damlr0	; 0x700 03400
	movgs	gr4, damlr1	; 0x701 03401
	movgs	gr4, damlr2	; 0x702 03402
	movgs	gr4, damlr3	; 0x703 03403
	movgs	gr4, damlr4	; 0x704 03404
	movgs	gr4, damlr5	; 0x705 03405
	movgs	gr4, damlr6	; 0x706 03406
	movgs	gr4, damlr7	; 0x707 03407
	movgs	gr4, damlr8	; 0x708 03410
	movgs	gr4, damlr9	; 0x709 03411
	movgs	gr4, damlr10	; 0x70a 03412
	movgs	gr4, damlr11	; 0x70b 03413
	movgs	gr4, dampr0	; 0x740 03500
	movgs	gr4, dampr1	; 0x741 03501
	movgs	gr4, dampr2	; 0x742 03502
	movgs	gr4, dampr3	; 0x743 03503
	movgs	gr4, dampr4	; 0x744 03504
	movgs	gr4, dampr5	; 0x745 03505
	movgs	gr4, dampr6	; 0x746 03506
	movgs	gr4, dampr7	; 0x747 03507
	movgs	gr4, dampr8	; 0x748 03510
	movgs	gr4, dampr9	; 0x749 03511
	movgs	gr4, dampr10	; 0x74a 03512
	movgs	gr4, dampr11	; 0x74b 03513
	movgs	gr4, amcr	; 0x780 03600
	movgs	gr4, iamvr1	; 0x785 03605
	movgs	gr4, damvr1	; 0x787 03607
	movgs	gr4, cxnr	; 0x790 03620
	movgs	gr4, ttbr	; 0x791 03621
	movgs	gr4, tplr	; 0x792 03622
	movgs	gr4, tppr	; 0x793 03623
	movgs	gr4, tpxr	; 0x794 03624
	movgs	gr4, timerh	; 0x7a0 03640
	movgs	gr4, timerl	; 0x7a1 03641
	movgs	gr4, timerd	; 0x7a2 03642
	movgs	gr4, dcr	; 0x800 04000
	movgs	gr4, brr	; 0x801 04001
	movgs	gr4, nmar	; 0x802 04002
	movgs	gr4, btbr	; 0x803 04003
	movgs	gr4, ibar0	; 0x804 04004
	movgs	gr4, ibar1	; 0x805 04005
	movgs	gr4, ibar2	; 0x806 04006
	movgs	gr4, ibar3	; 0x807 04007
	movgs	gr4, dbar0	; 0x808 04010
	movgs	gr4, dbar1	; 0x809 04011
	movgs	gr4, dbar2	; 0x80A 04012
	movgs	gr4, dbar3	; 0x80B 04013
	movgs	gr4, dbdr00	; 0x80C 04014
	movgs	gr4, dbdr01	; 0x80D 04015
	movgs	gr4, dbdr02	; 0x80E 04016
	movgs	gr4, dbdr03	; 0x80F 04017
	movgs	gr4, dbdr10	; 0x810 04020
	movgs	gr4, dbdr11	; 0x811 04021
	movgs	gr4, dbmr00	; 0x81C 04034
	movgs	gr4, dbmr01	; 0x81D 04035
	movgs	gr4, dbmr10	; 0x820 04040
	movgs	gr4, dbmr11	; 0x821 04041
