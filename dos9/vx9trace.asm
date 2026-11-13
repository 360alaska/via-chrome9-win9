; VX9TRACE v1.4 (after each write of 12 bytes or more to a file handle (5 and up) the file is committed, INT 21 AH=68, so
; BOOTLOG.TXT on the disk holds every line Windows wrote before a freeze: the screen lines stop when Windows' display driver takes
; the screen memory, and ScanDisk found no lost chain to recover). v1.3 (line 11, green: a count for each of the 16 hardware interrupts as they reach DOS-side handlers, IRQ 0 first:
; a count that keeps moving after the freeze = Windows is alive and waiting; one that races = an interrupt storm; all still = dead).
; v1.2: a resident program for plain DOS. Windows writes its start-up log line by line through DOS while it starts, but the
; file is only given its length when it is closed, so after a freeze BOOTLOG.TXT ends early (VX900 board: v1.1 showed write after
; write to handle 5, 11 and 25 bytes, each followed by a disk write). v1.2 shows the text itself: the top ten screen lines are the
; last ten of: W text written through INT 21 AH=40 (12 bytes or more), > a file opened or run (AH=3D, 4B, AX=6C00), and
; * Windows' own INT 2F AH=16 broadcasts (AX, BX, CX). What stands there at the freeze is the end of the log.
        .8086
_TEXT   segment word public 'CODE'
        assume cs:_TEXT,ds:_TEXT
        org 100h
start:  jmp init
old21   dd 0
old2F   dd 0
tbuf    db '2F AX=0000 BX=0000 CX=0000',0
oldi    dd 16 dup(0)
icnt    dw 16 dup(0)
irqlog: pushf
        push ax
        push cx
        push dx
        push di
        push es
        inc word ptr cs:[icnt+bx]
        mov dx,cs:[icnt+bx]
        mov di,bx
        shl di,1
        shl di,1
        add di,bx
        add di,10*160
        mov ax,0B800h
        mov es,ax
        cld
        mov cx,4
iq1:    rol dx,1
        rol dx,1
        rol dx,1
        rol dx,1
        mov al,dl
        and al,0Fh
        add al,'0'
        cmp al,'9'
        jbe iq2
        add al,7
iq2:    mov ah,2Fh
        stosw
        loop iq1
        mov ax,2F20h
        stosw
        pop es
        pop di
        pop dx
        pop cx
        pop ax
        popf
        ret
irq0:  push bx
        mov bx,0
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+0]
irq1:  push bx
        mov bx,2
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+4]
irq2:  push bx
        mov bx,4
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+8]
irq3:  push bx
        mov bx,6
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+12]
irq4:  push bx
        mov bx,8
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+16]
irq5:  push bx
        mov bx,10
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+20]
irq6:  push bx
        mov bx,12
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+24]
irq7:  push bx
        mov bx,14
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+28]
irq8:  push bx
        mov bx,16
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+32]
irq9:  push bx
        mov bx,18
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+36]
irqA:  push bx
        mov bx,20
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+40]
irqB:  push bx
        mov bx,22
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+44]
irqC:  push bx
        mov bx,24
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+48]
irqD:  push bx
        mov bx,26
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+52]
irqE:  push bx
        mov bx,28
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+56]
irqF:  push bx
        mov bx,30
        call irqlog
        pop bx
        jmp dword ptr cs:[oldi+60]
; emit: DS:SI = text, CX = most characters, BL = 1 when the text ends at a zero byte, AL = the line's first character
emit:   push ax
        push ds
        push si
        push cx
        cld
        mov ax,0B800h
        mov es,ax
        mov ds,ax
        mov si,160
        xor di,di
        mov cx,9*80
        rep movsw
        pop cx
        pop si
        pop ds
        mov di,9*160
        pop ax
        mov ah,4Fh
        stosw
        mov dx,78
        cmp cx,dx
        jbe e0
        mov cx,dx
e0:     jcxz pad
e1:     lodsb
        cmp bl,1
        jne e2
        or al,al
        jz pad
e2:     cmp al,20h
        jae e3
        mov al,' '
e3:     mov ah,4Fh
        stosw
        dec dx
        loop e1
pad:    mov cx,dx
        inc cx
        mov ax,4F20h
        rep stosw
        ret
h21:    pushf
        cmp ah,40h
        je w21
        cmp ah,3Dh
        je o21
        cmp ah,4Bh
        je o21
        cmp ax,6C00h
        je x21
p21:    popf
        jmp dword ptr cs:[old21]
w21:    cmp cx,12
        jb p21
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push es
        mov si,dx
        mov bl,0
        mov al,'W'
        call emit
        pop es
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        popf
        cmp bx,5
        jb c21
        pushf
        call dword ptr cs:[old21]
        pushf
        push ax
        mov ah,68h
        pushf
        call dword ptr cs:[old21]
        pop ax
        popf
        retf 2
c21:    jmp dword ptr cs:[old21]
o21:    push ax
        push bx
        push cx
        push dx
        push si
        push di
        push es
        mov si,dx
        mov cx,78
        mov bl,1
        mov al,'>'
        call emit
        jmp short r21
x21:    push ax
        push bx
        push cx
        push dx
        push si
        push di
        push es
        mov cx,78
        mov bl,1
        mov al,'>'
        call emit
r21:    pop es
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        jmp p21
h2F:    pushf
        cmp ah,16h
        je l2F
p2F:    popf
        jmp dword ptr cs:[old2F]
l2F:    push ax
        push bx
        push cx
        push dx
        push si
        push di
        push ds
        push es
        push cs
        pop ds
        push cs
        pop es
        cld
        mov dx,ax
        mov di,offset tbuf+6
        call hexb
        mov dx,bx
        mov di,offset tbuf+14
        call hexb
        mov dx,cx
        mov di,offset tbuf+22
        call hexb
        mov si,offset tbuf
        mov cx,78
        mov bl,1
        mov al,'*'
        call emit
        pop es
        pop ds
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        jmp p2F
hexb:   push cx
        mov cx,4
hb1:    rol dx,1
        rol dx,1
        rol dx,1
        rol dx,1
        mov al,dl
        and al,0Fh
        add al,'0'
        cmp al,'9'
        jbe hb2
        add al,7
hb2:    stosb
        loop hb1
        pop cx
        ret
init:   mov dx,offset msg
        mov ah,9
        int 21h
        mov ax,352Fh
        int 21h
        mov word ptr [old2F],bx
        mov word ptr [old2F+2],es
        mov ax,3521h
        int 21h
        mov word ptr [old21],bx
        mov word ptr [old21+2],es
        mov dx,offset h2F
        mov ax,252Fh
        int 21h
        mov dx,offset h21
        mov ax,2521h
        int 21h
        mov si,offset itab
        mov di,offset oldi
        mov cx,16
ii1:    lodsb
        mov ah,35h
        push ax
        int 21h
        mov [di],bx
        mov [di+2],es
        pop ax
        mov ah,25h
        mov dx,[si]
        int 21h
        add si,2
        add di,4
        loop ii1
        mov dx,offset init
        int 27h
itab    label byte
        db 8
        dw offset irq0
        db 9
        dw offset irq1
        db 10
        dw offset irq2
        db 11
        dw offset irq3
        db 12
        dw offset irq4
        db 13
        dw offset irq5
        db 14
        dw offset irq6
        db 15
        dw offset irq7
        db 112
        dw offset irq8
        db 113
        dw offset irq9
        db 114
        dw offset irqA
        db 115
        dw offset irqB
        db 116
        dw offset irqC
        db 117
        dw offset irqD
        db 118
        dw offset irqE
        db 119
        dw offset irqF

msg     db 'VX9TRACE v1.4 resident: the top ten lines show what Windows writes to its start-up log (W), the files',13,10
        db 'it opens (>) and its own broadcasts (*); line 11 counts IRQ 0 to 15. Press Esc at the logo.',13,10
        db 'Every log line is now committed to the disk: after a freeze, read C:\BOOTLOG.TXT.',13,10,'$'
_TEXT   ends
        end start
