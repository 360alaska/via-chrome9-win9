;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Copyright (c) 2022  Michal Necasek
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Define a macro which produces a DIB engine thunk, taking care of all
; externs and publics.
; The DIB_xxxExt functions take one additional parameter. To minimize
; stack space copying and usage, the thunk pops off the return address,
; pushes the additional parameter (always the last parameter of
; DIB_xxxExt), pushes the return address back, and jumps to DIB_xxxExt.
; The AX, ECX, and ES registers are modified with no ill effect since
; they aren't used by the Pascal calling convention to pass arguments.

;; Thunk macro with additional parameters.
DIBTHK	macro	name, param
extrn	DIB_&name&Ext : far
public	name
name:
	mov	ax, DGROUP	; Load ES with our data segment.
	mov	es, ax
	assume	es:DGROUP
	pop	ecx		; Save the 16:16 return address in ECX.
	push	param		; Push the additional parameter.
	push	ecx		; Put the return address back.
	jmp	DIB_&name&Ext	; Off to the DIB Engine we go.
	endm

;; Simple forwarder macro.
DIBFWD	macro	name
extrn	DIB_&name : far
public	name
name:
	jmp	DIB_&name	; Just jump to the DIB Engine.
	endm

;; v10.91: capturing forwarder. Same as DIBFWD, but while _vx_rawcap_on is set the first 20 words above the return address (the Pascal arguments, last
;; parameter lowest; an unknown number of them belongs to the caller's frame) are copied into the next free 64-byte slot of _vx_rawcap_buf (word 0 is the tag,
;; 16 slots; a full buffer only counts the drop). Every register it touches is restored. dibcall.c turns the slots into records when the capture is read.
DIBCAP	macro	name, tag
	local	done, drop
extrn	DIB_&name : far
public	name
name:
	push	bp
	mov	bp, sp
	push	ds
	push	es
	push	si
	push	di
	push	cx
	push	ax
	mov	ax, DGROUP
	mov	ds, ax
	assume	ds:DGROUP
	cmp	word ptr _vx_rawcap_on, 0
	je	done
	mov	ax, word ptr _vx_rawcap_n
	cmp	ax, 16
	jae	drop
	inc	word ptr _vx_rawcap_n
	shl	ax, 6
	add	ax, offset DGROUP:_vx_rawcap_buf
	mov	di, ax
	mov	ax, ds
	mov	es, ax
	mov	word ptr es:[di], tag
	add	di, 2
	push	ds
	mov	ax, ss
	mov	ds, ax
	lea	si, [bp+6]
	mov	cx, 20
	cld
	rep	movsw
	pop	ds
	jmp	short done
drop:
	cmp	word ptr _vx_rawcap_drop, 0FFFFh	; v10.330 (review #186): the count stops at 65535 (it wrapped to 0)
	je	short done
	inc	word ptr _vx_rawcap_drop
done:
	pop	ax
	pop	cx
	pop	di
	pop	si
	pop	es
	pop	ds
	pop	bp
	assume	ds:nothing
	jmp	DIB_&name
	endm

;; v10.99: the same capturing forwarder under another public name, so that dibcall.c can hook StretchDIBits and still hand the call on (and keep the capture).
DIBCAPN	macro	name, tag, label
	local	done, drop
extrn	DIB_&name : far
public	label
label:
	push	bp
	mov	bp, sp
	push	ds
	push	es
	push	si
	push	di
	push	cx
	push	ax
	mov	ax, DGROUP
	mov	ds, ax
	assume	ds:DGROUP
	cmp	word ptr _vx_rawcap_on, 0
	je	done
	mov	ax, word ptr _vx_rawcap_n
	cmp	ax, 16
	jae	drop
	inc	word ptr _vx_rawcap_n
	shl	ax, 6
	add	ax, offset DGROUP:_vx_rawcap_buf
	mov	di, ax
	mov	ax, ds
	mov	es, ax
	mov	word ptr es:[di], tag
	add	di, 2
	push	ds
	mov	ax, ss
	mov	ds, ax
	lea	si, [bp+6]
	mov	cx, 20
	cld
	rep	movsw
	pop	ds
	jmp	short done
drop:
	cmp	word ptr _vx_rawcap_drop, 0FFFFh	; v10.330 (review #186): the count stops at 65535 (it wrapped to 0)
	je	short done
	inc	word ptr _vx_rawcap_drop
done:
	pop	ax
	pop	cx
	pop	di
	pop	si
	pop	es
	pop	ds
	pop	bp
	assume	ds:nothing
	jmp	DIB_&name
	endm

;; Additional variables that need to be pushed are in the data segment.
_DATA	segment public 'DATA'

extrn	_lpDriverPDevice : dword
extrn	_wPalettized : word
extrn	_vx_rawcap_on : word
extrn	_vx_rawcap_n : word
extrn	_vx_rawcap_drop : word
extrn	_vx_rawcap_buf : word

_DATA	ends

DGROUP	group 	_DATA

_TEXT	segment	public 'CODE'

.386
assume	ds:nothing, es:nothing

;; Thunks that push an additional parameter.
;; Sorted by ordinal number.
DIBTHK	EnumObj, 		_lpDriverPDevice
DIBTHK	RealizeObject,		_lpDriverPDevice
DIBTHK	DibBlt,			_wPalettized
DIBTHK	GetPalette,		_lpDriverPDevice
DIBTHK	SetPaletteTranslate,	_lpDriverPDevice
DIBTHK	GetPaletteTranslate,	_lpDriverPDevice
DIBTHK	UpdateColors,		_lpDriverPDevice
;DIBTHK	SetCursor,		_lpDriverPDevice
;DIBTHK	MoveCursor,		_lpDriverPDevice

;; There is collision with driver's SetCursor ans WINAPI function of same name
;; and this is hack to resolve it without modify of headers
extrn	SetCursor_driver : far
public	SetCursor
SetCursor:
	jmp SetCursor_driver

;; Forwarders that simply jump to the DIB Engine.
;; Sorted by ordinal number.
ifndef HWBLT
DIBFWD	BitBlt
endif
DIBFWD	ColorInfo
;DIBFWD	Control
DIBFWD	EnumDFonts
;DIBFWD	Output (v10.90: dibcall.c)
DIBCAP	Pixel, 6
DIBCAP	Strblt, 2
DIBCAP	ScanLR, 5
DIBFWD	DeviceMode
;DIBFWD	ExtTextOut
DIBFWD	GetCharWidth
DIBFWD	DeviceBitmap
DIBCAP	FastBorder, 1
DIBFWD	SetAttribute
DIBFWD	CreateDIBitmap
DIBCAPN	DibToDevice, 3, DibToDevice_fwd
ifndef HWBLT
DIBFWD	StretchBlt
endif
DIBCAPN	StretchDIBits, 4, StretchDIBits_fwd
DIBFWD	SelectBitmap
DIBFWD	BitmapBits
DIBFWD	Inquire



_TEXT	ends

end

