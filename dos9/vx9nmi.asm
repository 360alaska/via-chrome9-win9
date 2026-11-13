; VX9NMI v1.0 (plain DOS). Windows 98's PARITY driver switches the two classic NMI sources on (port 61 bits 2 and 3 cleared, NMI
; unmasked at port 70) during its start. On the Gigabyte M7V90PI Windows freezes two or three log lines after PARITY has started,
; whichever driver comes next. This program does the same switching for two seconds with its own NMI handler and counts what arrives.
        .8086
_TEXT   segment word public 'CODE'
        assume cs:_TEXT,ds:_TEXT
        org 100h
start:  jmp main
old2    dd 0
count   dw 0
seen    db 0
nmi:    push ax
        inc word ptr cs:[count]
        in al,61h
        or cs:[seen],al
        cmp word ptr cs:[count],2000
        jae noff
        and al,03h
        or al,0Ch
        out 61h,al
        and al,03h
        out 61h,al
        mov al,0Dh
        out 70h,al
        pop ax
        iret
noff:   and al,03h
        or al,0Ch
        out 61h,al
        mov al,8Dh
        out 70h,al
        pop ax
        iret
main:   mov dx,offset m1
        mov ah,9
        int 21h
        in al,61h
        mov dl,al
        call hex2
        mov ax,3502h
        int 21h
        mov word ptr [old2],bx
        mov word ptr [old2+2],es
        mov dx,offset nmi
        mov ax,2502h
        int 21h
        cli
        in al,61h
        and al,03h
        or al,0Ch
        out 61h,al
        and al,03h
        out 61h,al
        mov al,0Dh
        out 70h,al
        jmp short $+2
        in al,71h
        sti
        mov ax,40h
        mov es,ax
        mov bx,es:[6Ch]
        add bx,36
w1:     mov ax,es:[6Ch]
        sub ax,bx
        js w1
        cli
        in al,61h
        mov bl,al
        and al,03h
        or al,0Ch
        out 61h,al
        mov al,8Dh
        out 70h,al
        jmp short $+2
        in al,71h
        sti
        push bx
        push ds
        lds dx,dword ptr cs:[old2]
        mov ax,2502h
        int 21h
        pop ds
        mov dx,offset m2
        mov ah,9
        int 21h
        mov dx,[count]
        mov cl,dh
        push dx
        mov dl,cl
        call hex2
        pop dx
        call hex2
        mov dx,offset m3
        mov ah,9
        int 21h
        mov dl,[seen]
        call hex2
        mov dx,offset m4
        mov ah,9
        int 21h
        pop bx
        mov dl,bl
        call hex2
        mov dx,offset m5
        mov ah,9
        int 21h
        cmp word ptr [count],0
        je fin
        mov dx,offset m6
        mov ah,9
        int 21h
fin:    mov ax,4C00h
        int 21h
hex2:   push dx
        mov cl,4
        shr dl,cl
        call nib
        pop dx
        and dl,0Fh
nib:    add dl,'0'
        cmp dl,'9'
        jbe n1
        add dl,7
n1:     mov ah,2
        int 21h
        ret
m1      db 'VX9NMI v1.0. Port 61 before: $'
m2      db ' (bits 2, 3 set = the NMI sources are off).',13,10,'With both sources on for two seconds: NMIs counted (hex) $'
m3      db ', port 61 bits seen in the handler $'
m4      db ' (80 = system error / parity, 40 = I/O check)',13,10,'port 61 while they were on: $'
m5      db '. Sources switched off again.',13,10,'$'
m6      db '>>> NMIs arrive on this board when Windows'' PARITY driver switches them on.',13,10,'$'
_TEXT   ends
        end start
