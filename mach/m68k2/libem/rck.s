.define .rck
.sect .text
.sect .rom
.sect .data
.sect .bss

	.sect .text
.rck:
	move.l	(sp)+,a1
	move.l	(sp)+,a0	! descriptor address
	move.w	(sp),d0
	cmp	(a0),d0
	blt	1f
	cmp	2(a0),d0
	ble	2f
1:
	move.l	a1,-(sp)	! .trp and the trap handler may change a1
	move.w	#ERANGE,-(sp)
	jsr	.trp
	rts
2:
	jmp	(a1)
