	.level 1.1
	.code
	.align 4
; Basic shladd instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
	shladd  %r4,1,%r5,%r6
	shladd,=  %r4,1,%r5,%r6
	shladd,<  %r4,1,%r5,%r6
	shladd,<=  %r4,1,%r5,%r6
	shladd,nuv  %r4,1,%r5,%r6
	shladd,znv  %r4,1,%r5,%r6
	shladd,sv  %r4,1,%r5,%r6
	shladd,od  %r4,1,%r5,%r6
	shladd,tr  %r4,1,%r5,%r6
	shladd,<>  %r4,1,%r5,%r6
	shladd,>=  %r4,1,%r5,%r6
	shladd,>  %r4,1,%r5,%r6
	shladd,uv  %r4,1,%r5,%r6
	shladd,vnz  %r4,1,%r5,%r6
	shladd,nsv  %r4,1,%r5,%r6
	shladd,ev  %r4,1,%r5,%r6

	shladd,l  %r4,1,%r5,%r6
	shladd,l,=  %r4,1,%r5,%r6
	shladd,l,<  %r4,1,%r5,%r6
	shladd,l,<=  %r4,1,%r5,%r6
	shladd,l,nuv  %r4,1,%r5,%r6
	shladd,l,znv  %r4,1,%r5,%r6
	shladd,l,sv  %r4,1,%r5,%r6
	shladd,l,od  %r4,1,%r5,%r6
	shladd,l,tr  %r4,1,%r5,%r6
	shladd,l,<>  %r4,1,%r5,%r6
	shladd,l,>=  %r4,1,%r5,%r6
	shladd,l,>  %r4,1,%r5,%r6
	shladd,l,uv  %r4,1,%r5,%r6
	shladd,l,vnz  %r4,1,%r5,%r6
	shladd,l,nsv  %r4,1,%r5,%r6
	shladd,l,ev  %r4,1,%r5,%r6

	shladd,tsv  %r4,1,%r5,%r6
	shladd,tsv,=  %r4,1,%r5,%r6
	shladd,tsv,<  %r4,1,%r5,%r6
	shladd,tsv,<=  %r4,1,%r5,%r6
	shladd,tsv,nuv  %r4,1,%r5,%r6
	shladd,tsv,znv  %r4,1,%r5,%r6
	shladd,tsv,sv  %r4,1,%r5,%r6
	shladd,tsv,od  %r4,1,%r5,%r6
	shladd,tsv,tr  %r4,1,%r5,%r6
	shladd,tsv,<>  %r4,1,%r5,%r6
	shladd,tsv,>=  %r4,1,%r5,%r6
	shladd,tsv,>  %r4,1,%r5,%r6
	shladd,tsv,uv  %r4,1,%r5,%r6
	shladd,tsv,vnz  %r4,1,%r5,%r6
	shladd,tsv,nsv  %r4,1,%r5,%r6
	shladd,tsv,ev  %r4,1,%r5,%r6

	shladd  %r4,2,%r5,%r6
	shladd,=  %r4,2,%r5,%r6
	shladd,<  %r4,2,%r5,%r6
	shladd,<=  %r4,2,%r5,%r6
	shladd,nuv  %r4,2,%r5,%r6
	shladd,znv  %r4,2,%r5,%r6
	shladd,sv  %r4,2,%r5,%r6
	shladd,od  %r4,2,%r5,%r6
	shladd,tr  %r4,2,%r5,%r6
	shladd,<>  %r4,2,%r5,%r6
	shladd,>=  %r4,2,%r5,%r6
	shladd,>  %r4,2,%r5,%r6
	shladd,uv  %r4,2,%r5,%r6
	shladd,vnz  %r4,2,%r5,%r6
	shladd,nsv  %r4,2,%r5,%r6
	shladd,ev  %r4,2,%r5,%r6

	shladd,l  %r4,2,%r5,%r6
	shladd,l,=  %r4,2,%r5,%r6
	shladd,l,<  %r4,2,%r5,%r6
	shladd,l,<=  %r4,2,%r5,%r6
	shladd,l,nuv  %r4,2,%r5,%r6
	shladd,l,znv  %r4,2,%r5,%r6
	shladd,l,sv  %r4,2,%r5,%r6
	shladd,l,od  %r4,2,%r5,%r6
	shladd,l,tr  %r4,2,%r5,%r6
	shladd,l,<>  %r4,2,%r5,%r6
	shladd,l,>=  %r4,2,%r5,%r6
	shladd,l,>  %r4,2,%r5,%r6
	shladd,l,uv  %r4,2,%r5,%r6
	shladd,l,vnz  %r4,2,%r5,%r6
	shladd,l,nsv  %r4,2,%r5,%r6
	shladd,l,ev  %r4,2,%r5,%r6

	shladd,tsv  %r4,2,%r5,%r6
	shladd,tsv,=  %r4,2,%r5,%r6
	shladd,tsv,<  %r4,2,%r5,%r6
	shladd,tsv,<=  %r4,2,%r5,%r6
	shladd,tsv,nuv  %r4,2,%r5,%r6
	shladd,tsv,znv  %r4,2,%r5,%r6
	shladd,tsv,sv  %r4,2,%r5,%r6
	shladd,tsv,od  %r4,2,%r5,%r6
	shladd,tsv,tr  %r4,2,%r5,%r6
	shladd,tsv,<>  %r4,2,%r5,%r6
	shladd,tsv,>=  %r4,2,%r5,%r6
	shladd,tsv,>  %r4,2,%r5,%r6
	shladd,tsv,uv  %r4,2,%r5,%r6
	shladd,tsv,vnz  %r4,2,%r5,%r6
	shladd,tsv,nsv  %r4,2,%r5,%r6
	shladd,tsv,ev  %r4,2,%r5,%r6

	shladd  %r4,3,%r5,%r6
	shladd,=  %r4,3,%r5,%r6
	shladd,<  %r4,3,%r5,%r6
	shladd,<=  %r4,3,%r5,%r6
	shladd,nuv  %r4,3,%r5,%r6
	shladd,znv  %r4,3,%r5,%r6
	shladd,sv  %r4,3,%r5,%r6
	shladd,od  %r4,3,%r5,%r6
	shladd,tr  %r4,3,%r5,%r6
	shladd,<>  %r4,3,%r5,%r6
	shladd,>=  %r4,3,%r5,%r6
	shladd,>  %r4,3,%r5,%r6
	shladd,uv  %r4,3,%r5,%r6
	shladd,vnz  %r4,3,%r5,%r6
	shladd,nsv  %r4,3,%r5,%r6
	shladd,ev  %r4,3,%r5,%r6

	shladd,l  %r4,3,%r5,%r6
	shladd,l,=  %r4,3,%r5,%r6
	shladd,l,<  %r4,3,%r5,%r6
	shladd,l,<=  %r4,3,%r5,%r6
	shladd,l,nuv  %r4,3,%r5,%r6
	shladd,l,znv  %r4,3,%r5,%r6
	shladd,l,sv  %r4,3,%r5,%r6
	shladd,l,od  %r4,3,%r5,%r6
	shladd,l,tr  %r4,3,%r5,%r6
	shladd,l,<>  %r4,3,%r5,%r6
	shladd,l,>=  %r4,3,%r5,%r6
	shladd,l,>  %r4,3,%r5,%r6
	shladd,l,uv  %r4,3,%r5,%r6
	shladd,l,vnz  %r4,3,%r5,%r6
	shladd,l,nsv  %r4,3,%r5,%r6
	shladd,l,ev  %r4,3,%r5,%r6

	shladd,tsv  %r4,3,%r5,%r6
	shladd,tsv,=  %r4,3,%r5,%r6
	shladd,tsv,<  %r4,3,%r5,%r6
	shladd,tsv,<=  %r4,3,%r5,%r6
	shladd,tsv,nuv  %r4,3,%r5,%r6
	shladd,tsv,znv  %r4,3,%r5,%r6
	shladd,tsv,sv  %r4,3,%r5,%r6
	shladd,tsv,od  %r4,3,%r5,%r6
	shladd,tsv,tr  %r4,3,%r5,%r6
	shladd,tsv,<>  %r4,3,%r5,%r6
	shladd,tsv,>=  %r4,3,%r5,%r6
	shladd,tsv,>  %r4,3,%r5,%r6
	shladd,tsv,uv  %r4,3,%r5,%r6
	shladd,tsv,vnz  %r4,3,%r5,%r6
	shladd,tsv,nsv  %r4,3,%r5,%r6
	shladd,tsv,ev  %r4,3,%r5,%r6

