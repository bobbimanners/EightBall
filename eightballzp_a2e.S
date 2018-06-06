;
; Apple II zero page is used for pointer for indirect addressing

; $80-$9a are used by cc65 ZP segment so avoid this range

; Zero-page usage is documented in:
; ProDOS 8 Technical Reference Manual Section A.4
; The upper half of ZP is generally used by BASIC only, so we can use it.

.exportzp _addrptr   := $a0

