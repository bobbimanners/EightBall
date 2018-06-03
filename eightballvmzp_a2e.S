;
; Apple II zero page is used for VM
; registers: evalptr, PC, SP, FP

; $80-$9a are used by cc65 ZP segment so avoid this range

; Zero-page usage is documented in:
; ProDOS 8 Technical Reference Manual Section A.4
; The upper half of ZP is generally used by BASIC only, so we can use it.

.exportzp _evalptr   := $a0
.exportzp _pc        := $a1
.exportzp _sp        := $a3
.exportzp _fp        := $a5
.exportzp _wordptr   := $a7
.exportzp _byteptr   := $a9
.exportzp _evalstack := $ab ; 32 bytes

