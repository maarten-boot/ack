#
#include "em_abs.h"

 mes 2,EM_WSIZE,EM_PSIZE

; EM trap handler for the BASIC runtime. Where no hardware traps a
; division by zero (software floating point, or integer division done by
; libem), it is an EM trap (EIDIVZ, EFDIVZ), not a signal, so the
; signal() handlers of _ini_trp() never see it and ON ERROR cannot catch
; it. _trpem passes these traps to _trpfatal() as error 8, SIGFPE, as a
; hardware trap arrives; other traps get the default treatment (a
; message and exit). As the C runtime's _ctch_ (head_ac.e), but without
; going through a signal.

#define TRAPNO  0
#define SIGFPE  8

 exp $_trpem
 pro $_trpem,0
 lol TRAPNO
 loc EIDIVZ
 beq *1
 lol TRAPNO
 loc EFDIVZ
 beq *1
 lol TRAPNO
 trp		; the trap register is reset: default treatment
 rtt
1
 loc SIGFPE
 cal $_trpfatal	; does not return: longjmp to ON ERROR, or exit
 asp EM_WSIZE
 rtt
 end 0

; _sigtrp() makes _trpem the EM trap handler. A trap resets the trap
; register, so _ini_trp() calls this again after every caught error.

 exp $_sigtrp
 pro $_sigtrp,0
 lpi $_trpem
 sig
 asp EM_PSIZE
 ret 0
 end 0
