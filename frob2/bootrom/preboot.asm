*** frob2/netboot/preboot.asm
***
*** The purpose of this code is to disable interrupts,
*** choose a Stack region, and call the C RomMain() function.
*** It replaces the ordinary "cstart" code,
*** for use in a Boot ROM.
***
*** Assumes VDG is in default initialization with screen
*** buffer at $0400.  This code uses no ROM calls.
*** The code is relocatable.  Regardless where you load it,
*** jump to (or Basic `EXEC`) its first byte to execute it.
***
*** First prints a banner at the top of the screen.
*** Then calls _RomMain.
*** It is not intended that the _RomMain function ever returns.
*** But if _RomMain returns, print the banner backwards across the
*** middle of the screen, and get stuck.
***
*** You can CLOADM and EXEC a binary to test it.
*** But it is intended that this will eventually call
*** the RomMain() routine for the netboot ROM.

    IMPORT _RomMain

    SECTION start
		fcb 'D'
		fcb 'K'

    EXPORT program_start
program_start
preboot
    orcc #$50   ; disable interrupts
    lds #$0400  ; 1KB stack in lowest memory
    tfr s,u

    clrb
    tfr b,dp    ; dp=0

*** Display an ID at top of screen, to show preboot is running.
    leax id_string,pcr
    ldy #$0400
id_loop
    lda ,x
    anda #63
    sta ,y+
    lda ,x+
    bpl id_loop  ; while no N bit

    ldy #$0000  ; no global vars
    tfr y,u     ; no previous frame
    pshs y,u    ; 8 bytes of zeros onto stack
    pshs y,u
    lbsr _RomMain  ; and call RomMain with no args, which never returns.

*** Show id backwards if RomMain returns, and get stuck.
not_reached
    orcc #$50   ; disable interrupts
    leax id_string,pcr
    ldy #$0500  ; middle of screen
di_loop
    lda ,x
    anda #63
    sta ,-y
    lda ,x+
    bpl di_loop
di_stuck
    bra di_stuck

id_string  fcs / -- STRICKYAK FROBIO PREBOOT -- /

* Why do I need some padding before more code?
* Without this, my delay() in nullboot doesn't delay!?.

    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /
    fcs / Nando nando nando nando NANDO. /

    ENDSECTION
