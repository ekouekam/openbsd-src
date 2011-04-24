#PIII SIMD instructions

.text
foo:
 addps		(%ecx),%xmm0
 addps		%xmm2,%xmm1
 addss		(%ebx),%xmm2
 addss		%xmm4,%xmm3
 andnps		0x0(%ebp),%xmm4
 andnps		%xmm6,%xmm5
 andps		(%edi),%xmm6
 andps		%xmm0,%xmm7
 cmpps		$0x2,%xmm1,%xmm0
 cmpps		$0x3,(%edx),%xmm1
 cmpss		$0x4,%xmm2,%xmm2
 cmpss		$0x5,(%esp,1),%xmm3
 cmpps		$0x6,%xmm5,%xmm4
 cmpps		$0x7,(%esi),%xmm5
 cmpss		$0x0,%xmm7,%xmm6
 cmpss		$0x1,(%eax),%xmm7
 cmpeqps	%xmm1,%xmm0
 cmpeqps	(%edx),%xmm1
 cmpeqss	%xmm2,%xmm2
 cmpeqss	(%esp,1),%xmm3
 cmpltps	%xmm5,%xmm4
 cmpltps	(%esi),%xmm5
 cmpltss	%xmm7,%xmm6
 cmpltss	(%eax),%xmm7
 cmpleps	(%ecx),%xmm0
 cmpleps	%xmm2,%xmm1
 cmpless	(%ebx),%xmm2
 cmpless	%xmm4,%xmm3
 cmpunordps	0x0(%ebp),%xmm4
 cmpunordps	%xmm6,%xmm5
 cmpunordss	(%edi),%xmm6
 cmpunordss	%xmm0,%xmm7
 cmpneqps	%xmm1,%xmm0
 cmpneqps	(%edx),%xmm1
 cmpneqss	%xmm2,%xmm2
 cmpneqss	(%esp,1),%xmm3
 cmpnltps	%xmm5,%xmm4
 cmpnltps	(%esi),%xmm5
 cmpnltss	%xmm7,%xmm6
 cmpnltss	(%eax),%xmm7
 cmpnleps	(%ecx),%xmm0
 cmpnleps	%xmm2,%xmm1
 cmpnless	(%ebx),%xmm2
 cmpnless	%xmm4,%xmm3
 cmpordps	0x0(%ebp),%xmm4
 cmpordps	%xmm6,%xmm5
 cmpordss	(%edi),%xmm6
 cmpordss	%xmm0,%xmm7
 comiss		%xmm1,%xmm0
 comiss		(%edx),%xmm1
 cvtpi2ps	%mm3,%xmm2
 cvtpi2ps	(%esp,1),%xmm3
 cvtsi2ss	%ebp,%xmm4
 cvtsi2ss	(%esi),%xmm5
 cvtps2pi	%xmm7,%mm6
 cvtps2pi	(%eax),%mm7
 cvtss2si	(%ecx),%eax
 cvtss2si	%xmm2,%ecx
 cvttps2pi	(%ebx),%mm2
 cvttps2pi	%xmm4,%mm3
 cvttss2si	0x0(%ebp),%esp
 cvttss2si	%xmm6,%ebp
 divps		%xmm1,%xmm0
 divps		(%edx),%xmm1
 divss		%xmm3,%xmm2
 divss		(%esp,1),%xmm3
 ldmxcsr	0x0(%ebp)
 stmxcsr	(%esi)
 sfence
 maxps		%xmm1,%xmm0
 maxps		(%edx),%xmm1
 maxss		%xmm3,%xmm2
 maxss		(%esp,1),%xmm3
 minps		%xmm5,%xmm4
 minps		(%esi),%xmm5
 minss		%xmm7,%xmm6
 minss		(%eax),%xmm7
 movaps		%xmm1,%xmm0
 movaps		%xmm2,(%ecx)
 movaps		(%edx),%xmm2
 movlhps	%xmm4,%xmm3
 movhps		%xmm5,(%esp,1)
 movhps		(%esi),%xmm5
 movhlps	%xmm7,%xmm6
 movlps		%xmm0,(%edi)
 movlps		(%eax),%xmm0
 movmskps	%xmm2,%ecx
 movups		%xmm3,%xmm2
 movups		%xmm4,(%edx)
 movups		0x0(%ebp),%xmm4
 movss		%xmm6,%xmm5
 movss		%xmm7,(%esi)
 movss		(%eax),%xmm7
 mulps		%xmm1,%xmm0
 mulps		(%edx),%xmm1
 mulss		%xmm2,%xmm2
 mulss		(%esp,1),%xmm3
 orps		%xmm5,%xmm4
 orps		(%esi),%xmm5
 rcpps		%xmm7,%xmm6
 rcpps		(%eax),%xmm7
 rcpss		(%ecx),%xmm0
 rcpss		%xmm2,%xmm1
 rsqrtps	(%ebx),%xmm2
 rsqrtps	%xmm4,%xmm3
 rsqrtss	0x0(%ebp),%xmm4
 rsqrtss	%xmm6,%xmm5
 shufps		$0x2,(%edi),%xmm6
 shufps		$0x3,%xmm0,%xmm7
 sqrtps		%xmm1,%xmm0
 sqrtps		(%edx),%xmm1
 sqrtss		%xmm2,%xmm2
 sqrtss		(%esp,1),%xmm3
 subps		%xmm5,%xmm4
 subps		(%esi),%xmm5
 subss		%xmm7,%xmm6
 subss		(%eax),%xmm7
 ucomiss	(%ecx),%xmm0
 ucomiss	%xmm2,%xmm1
 unpckhps	(%ebx),%xmm2
 unpckhps	%xmm4,%xmm3
 unpcklps	0x0(%ebp),%xmm4
 unpcklps	%xmm6,%xmm5
 xorps		(%edi),%xmm6
 xorps		%xmm0,%xmm7
 pavgb		%mm1,%mm0
 pavgb		(%edx),%mm1
 pavgw		%mm3,%mm2
 pavgw		(%esp,1),%mm3
 pextrw		$0x0,%mm1,%eax
 pinsrw		$0x1,(%ecx),%mm1
 pinsrw		$0x2,%edx,%mm2
 pmaxsw		%mm1,%mm0
 pmaxsw		(%edx),%mm1
 pmaxub		%mm2,%mm2
 pmaxub		(%esp,1),%mm3
 pminsw		%mm5,%mm4
 pminsw		(%esi),%mm5
 pminub		%mm7,%mm6
 pminub		(%eax),%mm7
 pmovmskb	%mm5,%eax
 pmulhuw	%mm5,%mm4
 pmulhuw	(%esi),%mm5
 psadbw		%mm7,%mm6
 psadbw		(%eax),%mm7
 pshufw		$0x1,%mm2,%mm3
 pshufw		$0x4,0x0(%ebp),%mm6
 maskmovq	%mm7,%mm0
 movntps	%xmm6,(%ebx)
 movntq		%mm2,(%eax)
 prefetchnta	(%esi)
 prefetcht0	(%eax,%ebx,4)
 prefetcht1	(%edx)
 prefetcht2	(%ecx)

# A SIMD instruction with a bad extension byte
.byte 0x2E,0x0F,0xC2,0x0A,0x08
 nop
 nop
# A bad sfence modrm byte
.byte 0x65,0x0F,0xAE,0xff
# Pad out to good alignment
 .p2align 4,0
