; VX9EBIOS v1.0: a resident program for plain DOS. Windows 98's EBIOS driver asks the BIOS for the extended BIOS data area in its
; real-mode start (INT 15 AH=C1) and does not load when the BIOS says there is none (carry set). On the Gigabyte M7V90PI (VX900,
; AMI 09/14/11) EBIOS loads and never returns from its initialisation (VX9TRACE v1.2: the start-up log ends with
; DEVICEINIT = EBIOS). This program answers INT 15 AH=C1 with "not supported" (carry, AH=86), so EBIOS stays out.
; It first prints what the BIOS says now. Load it before WIN (AUTOEXEC.BAT).
        .8086
_TEXT   segment word public 'CODE'
        assume cs:_TEXT,ds:_TEXT
        org 100h
start:  jmp init
old15   dd 0
h15:    cmp ah,0C1h
        jne p15
        push bp
        mov bp,sp
        or word ptr [bp+6],1
        pop bp
        mov ah,86h
        iret
p15:    jmp dword ptr cs:[old15]
init:   mov dx,offset m1
        mov ah,9
        int 21h
        mov ah,0C1h
        stc
        int 15h
        push es
        pushf
        pop ax
        and ax,1
        mov dx,ax
        call hex4
        mov dx,offset m2
        mov ah,9
        int 21h
        pop dx
        push dx
        call hex4
        mov dx,offset m3
        mov ah,9
        int 21h
        mov ax,40h
        mov es,ax
        mov dx,es:[0Eh]
        push dx
        call hex4
        mov dx,offset m4
        mov ah,9
        int 21h
        mov dx,es:[13h]
        call hex4
        mov dx,offset m5
        mov ah,9
        int 21h
        pop es
        mov cx,8
        xor si,si
d1:     mov dl,es:[si]
        xor dh,dh
        push cx
        call hex4
        mov dl,' '
        mov ah,2
        int 21h
        pop cx
        inc si
        loop d1
        pop ax
        mov dx,offset m6
        mov ah,9
        int 21h
        mov ax,3515h
        int 21h
        mov word ptr [old15],bx
        mov word ptr [old15+2],es
        mov dx,offset h15
        mov ax,2515h
        int 21h
        mov dx,offset init
        int 27h
hex4:   mov cx,4
x1:     rol dx,1
        rol dx,1
        rol dx,1
        rol dx,1
        push dx
        and dl,0Fh
        add dl,'0'
        cmp dl,'9'
        jbe x2
        add dl,7
x2:     mov ah,2
        int 21h
        pop dx
        loop x1
        ret
m1      db 'VX9EBIOS v1.0. The BIOS now: INT 15 AH=C1 carry $'
m2      db ', ES $'
m3      db '; EBDA segment at 40:0E $'
m4      db ', base memory (hex K) $'
m5      db 13,10,'first bytes of that area (byte 0 = its size in K): $'
m6      db 13,10,'Resident: INT 15 AH=C1 now answers "not supported", so Windows does not load EBIOS.',13,10,'$'
_TEXT   ends
        end start
