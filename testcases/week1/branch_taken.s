	.arch armv8-a
	.text
	.align	2
start:

    movz x0, #1
    nop
    nop
    nop
    movz x1, #1
    nop
    nop
    nop
    subs x3, x0, x1
    nop
    nop
    nop
    nop
    b.eq .helper

.goback:

    nop
    nop
    nop

    // Print x0
    eor 	x5, x5, x5
    nop
    nop
    nop
	mvn 	x5, x5
    nop
    nop
    nop
	stur	x0, [x5]
    nop
    nop
    nop
    // correct: 65535
    // incorrect: 1
	ret
	
.helper:
    nop
    nop
    nop
    movz x0, #65535
    nop
    nop
    nop
    movz x1, #42069
    nop
    nop
    nop
    b .goback

    .size	start, .-start
	.ident	"GCC: (Ubuntu/Linaro 7.5.0-3ubuntu1~18.04) 7.5.0"
	.section	.note.GNU-stack,"",@progbits
