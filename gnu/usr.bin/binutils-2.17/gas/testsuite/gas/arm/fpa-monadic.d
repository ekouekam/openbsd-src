#objdump: -dr --prefix-addresses --show-raw-insn
#name: FPA Monadic instructions
#as: -mfpu=fpa -mcpu=arm7m

# Test FPA Monadic instructions
# This test should work for both big and little-endian assembly.

.*: *file format .*arm.*

Disassembly of section .text:
0+000 <[^>]*> ee008100 ?	mvfs	f0, f0
0+004 <[^>]*> ee008120 ?	mvfsp	f0, f0
0+008 <[^>]*> ee008140 ?	mvfsm	f0, f0
0+00c <[^>]*> ee008160 ?	mvfsz	f0, f0
0+010 <[^>]*> ee008180 ?	mvfd	f0, f0
0+014 <[^>]*> ee0081a0 ?	mvfdp	f0, f0
0+018 <[^>]*> ee0081c0 ?	mvfdm	f0, f0
0+01c <[^>]*> ee0081e0 ?	mvfdz	f0, f0
0+020 <[^>]*> ee088100 ?	mvfe	f0, f0
0+024 <[^>]*> ee088120 ?	mvfep	f0, f0
0+028 <[^>]*> ee088140 ?	mvfem	f0, f0
0+02c <[^>]*> ee088160 ?	mvfez	f0, f0
0+030 <[^>]*> ee108100 ?	mnfs	f0, f0
0+034 <[^>]*> ee108120 ?	mnfsp	f0, f0
0+038 <[^>]*> ee108140 ?	mnfsm	f0, f0
0+03c <[^>]*> ee108160 ?	mnfsz	f0, f0
0+040 <[^>]*> ee108180 ?	mnfd	f0, f0
0+044 <[^>]*> ee1081a0 ?	mnfdp	f0, f0
0+048 <[^>]*> ee1081c0 ?	mnfdm	f0, f0
0+04c <[^>]*> ee1081e0 ?	mnfdz	f0, f0
0+050 <[^>]*> ee188100 ?	mnfe	f0, f0
0+054 <[^>]*> ee188120 ?	mnfep	f0, f0
0+058 <[^>]*> ee188140 ?	mnfem	f0, f0
0+05c <[^>]*> ee188160 ?	mnfez	f0, f0
0+060 <[^>]*> ee208100 ?	abss	f0, f0
0+064 <[^>]*> ee208120 ?	abssp	f0, f0
0+068 <[^>]*> ee208140 ?	abssm	f0, f0
0+06c <[^>]*> ee208160 ?	abssz	f0, f0
0+070 <[^>]*> ee208180 ?	absd	f0, f0
0+074 <[^>]*> ee2081a0 ?	absdp	f0, f0
0+078 <[^>]*> ee2081c0 ?	absdm	f0, f0
0+07c <[^>]*> ee2081e0 ?	absdz	f0, f0
0+080 <[^>]*> ee288100 ?	abse	f0, f0
0+084 <[^>]*> ee288120 ?	absep	f0, f0
0+088 <[^>]*> ee288140 ?	absem	f0, f0
0+08c <[^>]*> ee288160 ?	absez	f0, f0
0+090 <[^>]*> ee308100 ?	rnds	f0, f0
0+094 <[^>]*> ee308120 ?	rndsp	f0, f0
0+098 <[^>]*> ee308140 ?	rndsm	f0, f0
0+09c <[^>]*> ee308160 ?	rndsz	f0, f0
0+0a0 <[^>]*> ee308180 ?	rndd	f0, f0
0+0a4 <[^>]*> ee3081a0 ?	rnddp	f0, f0
0+0a8 <[^>]*> ee3081c0 ?	rnddm	f0, f0
0+0ac <[^>]*> ee3081e0 ?	rnddz	f0, f0
0+0b0 <[^>]*> ee388100 ?	rnde	f0, f0
0+0b4 <[^>]*> ee388120 ?	rndep	f0, f0
0+0b8 <[^>]*> ee388140 ?	rndem	f0, f0
0+0bc <[^>]*> ee388160 ?	rndez	f0, f0
0+0c0 <[^>]*> ee408100 ?	sqts	f0, f0
0+0c4 <[^>]*> ee408120 ?	sqtsp	f0, f0
0+0c8 <[^>]*> ee408140 ?	sqtsm	f0, f0
0+0cc <[^>]*> ee408160 ?	sqtsz	f0, f0
0+0d0 <[^>]*> ee408180 ?	sqtd	f0, f0
0+0d4 <[^>]*> ee4081a0 ?	sqtdp	f0, f0
0+0d8 <[^>]*> ee4081c0 ?	sqtdm	f0, f0
0+0dc <[^>]*> ee4081e0 ?	sqtdz	f0, f0
0+0e0 <[^>]*> ee488100 ?	sqte	f0, f0
0+0e4 <[^>]*> ee488120 ?	sqtep	f0, f0
0+0e8 <[^>]*> ee488140 ?	sqtem	f0, f0
0+0ec <[^>]*> ee488160 ?	sqtez	f0, f0
0+0f0 <[^>]*> ee508100 ?	logs	f0, f0
0+0f4 <[^>]*> ee508120 ?	logsp	f0, f0
0+0f8 <[^>]*> ee508140 ?	logsm	f0, f0
0+0fc <[^>]*> ee508160 ?	logsz	f0, f0
0+100 <[^>]*> ee508180 ?	logd	f0, f0
0+104 <[^>]*> ee5081a0 ?	logdp	f0, f0
0+108 <[^>]*> ee5081c0 ?	logdm	f0, f0
0+10c <[^>]*> ee5081e0 ?	logdz	f0, f0
0+110 <[^>]*> ee588100 ?	loge	f0, f0
0+114 <[^>]*> ee588120 ?	logep	f0, f0
0+118 <[^>]*> ee588140 ?	logem	f0, f0
0+11c <[^>]*> ee588160 ?	logez	f0, f0
0+120 <[^>]*> ee608100 ?	lgns	f0, f0
0+124 <[^>]*> ee608120 ?	lgnsp	f0, f0
0+128 <[^>]*> ee608140 ?	lgnsm	f0, f0
0+12c <[^>]*> ee608160 ?	lgnsz	f0, f0
0+130 <[^>]*> ee608180 ?	lgnd	f0, f0
0+134 <[^>]*> ee6081a0 ?	lgndp	f0, f0
0+138 <[^>]*> ee6081c0 ?	lgndm	f0, f0
0+13c <[^>]*> ee6081e0 ?	lgndz	f0, f0
0+140 <[^>]*> ee688100 ?	lgne	f0, f0
0+144 <[^>]*> ee688120 ?	lgnep	f0, f0
0+148 <[^>]*> ee688140 ?	lgnem	f0, f0
0+14c <[^>]*> ee688160 ?	lgnez	f0, f0
0+150 <[^>]*> ee708100 ?	exps	f0, f0
0+154 <[^>]*> ee708120 ?	expsp	f0, f0
0+158 <[^>]*> ee708140 ?	expsm	f0, f0
0+15c <[^>]*> ee708160 ?	expsz	f0, f0
0+160 <[^>]*> ee708180 ?	expd	f0, f0
0+164 <[^>]*> ee7081a0 ?	expdp	f0, f0
0+168 <[^>]*> ee7081c0 ?	expdm	f0, f0
0+16c <[^>]*> ee7081e0 ?	expdz	f0, f0
0+170 <[^>]*> ee788100 ?	expe	f0, f0
0+174 <[^>]*> ee788120 ?	expep	f0, f0
0+178 <[^>]*> ee788140 ?	expem	f0, f0
0+17c <[^>]*> ee7081e0 ?	expdz	f0, f0
0+180 <[^>]*> ee808100 ?	sins	f0, f0
0+184 <[^>]*> ee808120 ?	sinsp	f0, f0
0+188 <[^>]*> ee808140 ?	sinsm	f0, f0
0+18c <[^>]*> ee808160 ?	sinsz	f0, f0
0+190 <[^>]*> ee808180 ?	sind	f0, f0
0+194 <[^>]*> ee8081a0 ?	sindp	f0, f0
0+198 <[^>]*> ee8081c0 ?	sindm	f0, f0
0+19c <[^>]*> ee8081e0 ?	sindz	f0, f0
0+1a0 <[^>]*> ee888100 ?	sine	f0, f0
0+1a4 <[^>]*> ee888120 ?	sinep	f0, f0
0+1a8 <[^>]*> ee888140 ?	sinem	f0, f0
0+1ac <[^>]*> ee888160 ?	sinez	f0, f0
0+1b0 <[^>]*> ee908100 ?	coss	f0, f0
0+1b4 <[^>]*> ee908120 ?	cossp	f0, f0
0+1b8 <[^>]*> ee908140 ?	cossm	f0, f0
0+1bc <[^>]*> ee908160 ?	cossz	f0, f0
0+1c0 <[^>]*> ee908180 ?	cosd	f0, f0
0+1c4 <[^>]*> ee9081a0 ?	cosdp	f0, f0
0+1c8 <[^>]*> ee9081c0 ?	cosdm	f0, f0
0+1cc <[^>]*> ee9081e0 ?	cosdz	f0, f0
0+1d0 <[^>]*> ee988100 ?	cose	f0, f0
0+1d4 <[^>]*> ee988120 ?	cosep	f0, f0
0+1d8 <[^>]*> ee988140 ?	cosem	f0, f0
0+1dc <[^>]*> ee988160 ?	cosez	f0, f0
0+1e0 <[^>]*> eea08100 ?	tans	f0, f0
0+1e4 <[^>]*> eea08120 ?	tansp	f0, f0
0+1e8 <[^>]*> eea08140 ?	tansm	f0, f0
0+1ec <[^>]*> eea08160 ?	tansz	f0, f0
0+1f0 <[^>]*> eea08180 ?	tand	f0, f0
0+1f4 <[^>]*> eea081a0 ?	tandp	f0, f0
0+1f8 <[^>]*> eea081c0 ?	tandm	f0, f0
0+1fc <[^>]*> eea081e0 ?	tandz	f0, f0
0+200 <[^>]*> eea88100 ?	tane	f0, f0
0+204 <[^>]*> eea88120 ?	tanep	f0, f0
0+208 <[^>]*> eea88140 ?	tanem	f0, f0
0+20c <[^>]*> eea88160 ?	tanez	f0, f0
0+210 <[^>]*> eeb08100 ?	asns	f0, f0
0+214 <[^>]*> eeb08120 ?	asnsp	f0, f0
0+218 <[^>]*> eeb08140 ?	asnsm	f0, f0
0+21c <[^>]*> eeb08160 ?	asnsz	f0, f0
0+220 <[^>]*> eeb08180 ?	asnd	f0, f0
0+224 <[^>]*> eeb081a0 ?	asndp	f0, f0
0+228 <[^>]*> eeb081c0 ?	asndm	f0, f0
0+22c <[^>]*> eeb081e0 ?	asndz	f0, f0
0+230 <[^>]*> eeb88100 ?	asne	f0, f0
0+234 <[^>]*> eeb88120 ?	asnep	f0, f0
0+238 <[^>]*> eeb88140 ?	asnem	f0, f0
0+23c <[^>]*> eeb88160 ?	asnez	f0, f0
0+240 <[^>]*> eec08100 ?	acss	f0, f0
0+244 <[^>]*> eec08120 ?	acssp	f0, f0
0+248 <[^>]*> eec08140 ?	acssm	f0, f0
0+24c <[^>]*> eec08160 ?	acssz	f0, f0
0+250 <[^>]*> eec08180 ?	acsd	f0, f0
0+254 <[^>]*> eec081a0 ?	acsdp	f0, f0
0+258 <[^>]*> eec081c0 ?	acsdm	f0, f0
0+25c <[^>]*> eec081e0 ?	acsdz	f0, f0
0+260 <[^>]*> eec88100 ?	acse	f0, f0
0+264 <[^>]*> eec88120 ?	acsep	f0, f0
0+268 <[^>]*> eec88140 ?	acsem	f0, f0
0+26c <[^>]*> eec88160 ?	acsez	f0, f0
0+270 <[^>]*> eed08100 ?	atns	f0, f0
0+274 <[^>]*> eed08120 ?	atnsp	f0, f0
0+278 <[^>]*> eed08140 ?	atnsm	f0, f0
0+27c <[^>]*> eed08160 ?	atnsz	f0, f0
0+280 <[^>]*> eed08180 ?	atnd	f0, f0
0+284 <[^>]*> eed081a0 ?	atndp	f0, f0
0+288 <[^>]*> eed081c0 ?	atndm	f0, f0
0+28c <[^>]*> eed081e0 ?	atndz	f0, f0
0+290 <[^>]*> eed88100 ?	atne	f0, f0
0+294 <[^>]*> eed88120 ?	atnep	f0, f0
0+298 <[^>]*> eed88140 ?	atnem	f0, f0
0+29c <[^>]*> eed88160 ?	atnez	f0, f0
0+2a0 <[^>]*> eee08100 ?	urds	f0, f0
0+2a4 <[^>]*> eee08120 ?	urdsp	f0, f0
0+2a8 <[^>]*> eee08140 ?	urdsm	f0, f0
0+2ac <[^>]*> eee08160 ?	urdsz	f0, f0
0+2b0 <[^>]*> eee08180 ?	urdd	f0, f0
0+2b4 <[^>]*> eee081a0 ?	urddp	f0, f0
0+2b8 <[^>]*> eee081c0 ?	urddm	f0, f0
0+2bc <[^>]*> eee081e0 ?	urddz	f0, f0
0+2c0 <[^>]*> eee88100 ?	urde	f0, f0
0+2c4 <[^>]*> eee88120 ?	urdep	f0, f0
0+2c8 <[^>]*> eee88140 ?	urdem	f0, f0
0+2cc <[^>]*> eee88160 ?	urdez	f0, f0
0+2d0 <[^>]*> eef08100 ?	nrms	f0, f0
0+2d4 <[^>]*> eef08120 ?	nrmsp	f0, f0
0+2d8 <[^>]*> eef08140 ?	nrmsm	f0, f0
0+2dc <[^>]*> eef08160 ?	nrmsz	f0, f0
0+2e0 <[^>]*> eef08180 ?	nrmd	f0, f0
0+2e4 <[^>]*> eef081a0 ?	nrmdp	f0, f0
0+2e8 <[^>]*> eef081c0 ?	nrmdm	f0, f0
0+2ec <[^>]*> eef081e0 ?	nrmdz	f0, f0
0+2f0 <[^>]*> eef88100 ?	nrme	f0, f0
0+2f4 <[^>]*> eef88120 ?	nrmep	f0, f0
0+2f8 <[^>]*> eef88140 ?	nrmem	f0, f0
0+2fc <[^>]*> eef88160 ?	nrmez	f0, f0
