	idnt	"C:\Users\dplan\Developer\Serena\Kernel\Sources\klib\Bytes.c"
	machine	68030
	opt o+,ol+,op+,oc+,ot+,oj+,ob+,om+
	section	"CODE",code
	public	_memcpy
	cnop	0,4
_memcpy
	movem.l	l124,-(a7)
	move.l	(4+l126,a7),a3
	move.l	a3,a0
	move.l	(8+l126,a7),a1
	move.l	a3,a2
	add.l	(12+l126,a7),a2
	move.l	a2,d0
	sub.l	a3,d0
	move.l	d0,d1
	lsr.l	#2,d1
	beq	l73
l71
	move.b	(a1)+,(a0)+
	move.b	(a1)+,(a0)+
	move.b	(a1)+,(a0)+
	move.b	(a1)+,(a0)+
	subq.l	#1,d1
	bne	l71
l73
	cmp.l	a0,a2
	bls	l74
l72
	move.b	(a1)+,(a0)+
	cmp.l	a0,a2
	bhi	l72
l74
	move.l	a3,d0
l124	reg	a2/a3
	movem.l	(a7)+,a2/a3
l126	equ	8
	rts
; stacksize=8
	machine	68030
	opt o+,ol+,op+,oc+,ot+,oj+,ob+,om+
	public	_memmove
	cnop	0,4
_memmove
	movem.l	l127,-(a7)
	move.l	(12+l129,a7),d5
	move.l	(4+l129,a7),d1
	move.l	(8+l129,a7),a5
	cmp.l	a5,d1
	beq	l92
	tst.l	d5
	beq	l92
	move.l	a5,a1
	move.l	d1,a2
	lea	(a5,d5.l),a4
	move.l	a4,d0
	subq.l	#1,d0
	move.l	d1,a0
	add.l	d5,a0
	move.l	a0,d4
	subq.l	#1,d4
	cmp.l	d0,d1
	bhi	l7
	cmp.l	d0,d4
	bcs	l7
	move.l	d0,a0
	move.l	d4,a3
	cmp.l	d0,a5
	bhi	l92
l87
	move.b	(a0),(a3)
	subq.l	#1,a0
	subq.l	#1,a3
	cmp.l	a0,a5
	bls	l87
	bra	l92
l7
	move.l	a5,d0
	moveq	#3,d2
	and.l	d0,d2
	move.l	d1,d0
	and.l	#3,d0
	cmp.l	d2,d0
	bne	l14
	tst.l	d2
	bls	l16
	lea	(1,a5),a1
	move.l	d1,a2
	addq.l	#1,a2
	move.l	d1,a6
	move.b	(a5),(a6)
	subq.l	#1,d2
l16
	tst.l	d2
	bls	l18
	move.b	(a1)+,(a2)+
	subq.l	#1,d2
l18
	tst.l	d2
	bls	l20
	move.b	(a1)+,(a2)+
l20
	move.l	a4,d0
	and.l	#4294967292,d0
	move.l	d0,d3
	cmp.l	a1,d3
	bls	l91
l88
	move.l	(a1)+,(a2)+
	cmp.l	a1,d3
	bhi	l88
l91
	cmp.l	a4,d3
	bcc	l92
	cmp.l	a1,a4
	bls	l27
	move.b	(a1)+,(a2)+
l27
	cmp.l	a1,a4
	bls	l29
	move.b	(a1)+,(a2)+
l29
	cmp.l	a1,a4
	bls	l92
	move.b	(a1),(a2)
	bra	l92
l14
	cmp.l	a5,a4
	bls	l92
l89
	move.b	(a1)+,(a2)+
	cmp.l	a1,a4
	bhi	l89
l92
	move.l	d1,d0
l127	reg	a2/a3/a4/a5/a6/d2/d3/d4/d5
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4/d5
l129	equ	36
	rts
; stacksize=36
	machine	68030
	opt o+,ol+,op+,oc+,ot+,oj+,ob+,om+
	public	_memset
	cnop	0,4
_memset
	movem.l	l130,-(a7)
	move.l	(12+l132,a7),d5
	move.l	(4+l132,a7),a5
	move.l	a5,a1
	move.b	(11+l132,a7),d3
	moveq	#16,d0
	cmp.l	d5,d0
	bls	l47
	lea	(a5,d5.l),a3
	cmp.l	a5,a3
	bls	l120
l111
	move.b	d3,(a1)+
	cmp.l	a1,a3
	bhi	l111
	bra	l120
l47
	move.l	a5,d0
	moveq	#3,d1
	and.l	d0,d1
	tst.l	d1
	bls	l52
l112
	move.b	d3,(a1)+
	move.l	a1,d0
	and.l	#3,d0
	bne	l112
	moveq	#4,d0
	sub.l	d1,d0
	sub.l	d0,d5
l52
	lea	(a1,d5.l),a4
	move.l	a1,a0
	moveq	#0,d2
	move.b	d3,d2
	moveq	#24,d1
	move.l	d2,d0
	lsl.l	d1,d0
	moveq	#16,d7
	move.l	d2,d1
	lsl.l	d7,d1
	or.l	d1,d0
	move.l	d2,d1
	lsl.l	#8,d1
	or.l	d1,d0
	move.l	d0,d6
	or.l	d2,d6
	move.l	d5,d0
	lsr.l	#2,d0
	lsl.l	#2,d0
	lea	(a1,d0.l),a2
	move.l	a2,d0
	sub.l	a1,d0
	asr.l	#2,d0
	move.l	d0,d4
	lsr.l	#4,d4
	beq	l118
l113
	move.l	d2,(a0)+
	move.l	d2,(a0)+
	move.l	d2,(a0)+
	move.l	d2,(a0)+
	subq.l	#1,d4
	bne	l113
l118
	cmp.l	a0,a2
	bls	l119
l114
	move.l	d6,(a0)+
	cmp.l	a0,a2
	bhi	l114
l119
	move.l	a0,a1
	cmp.l	a0,a4
	bls	l120
l115
	move.b	d3,(a1)+
	cmp.l	a1,a4
	bhi	l115
l120
	move.l	a5,d0
l130	reg	a2/a3/a4/a5/d2/d3/d4/d5/d6/d7
	movem.l	(a7)+,a2/a3/a4/a5/d2/d3/d4/d5/d6/d7
l132	equ	40
	rts
; stacksize=40
