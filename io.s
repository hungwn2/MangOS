global inb
; return a byte from a given I/O port
; stack: [esp + 4] The address of the I/O port
; [esp ] The return address
inb:
    mov dx, [esp+4]
    in al, dx
    ret

global outb

; outb - send a byte to I/O port
; stack args: [esp +8] data byte
;             [esp + 4] I/O port
;             [esp] Return address
outb:
    mov al, [esp+8]
    mov dx, [esp+4]
    out dx, al
    ret
