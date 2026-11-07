; Native power-up meter, Guard+Punch pickup, and Map16 attack interactions.

; Keep the original generic-carried collision setup, but make its later BIT/BVC
; see Punch only while Guard is held too. This stays within the existing hook.
pushpc
org $019FE0
    JML WW_PickupHeldCarried
org $01AA42
    JML WW_PickupContact        ; Restore raw Punch before the native pickup gate.
pullpc

WW_Powerup:
    PHA
    LDA.B #50
    JSR WW_AddMeter
    PLA
    SEC
    SBC.B #$74
    JML $01C53C

WW_PickupHeldCarried:
    JSL $019138                   ; RTL wrapper for the bank-$01 RTS routine.
    LDA.B !PlayerAnimation        ; The 4-byte hook also replaces this opcode.
    PHA
    LDA.L !WWPrevious2
    AND.B #$20
    BEQ .mask_punch
    LDA.L !WWPrevious1
    AND.B #$40
    BEQ .mask_punch
    LDA.B !Held1
    ORA.B #$40
    STA.B !Held1
    PLA
    JML $019FE5
.mask_punch:
    LDA.B !Held1
    AND.B #$BF
    STA.B !Held1
    PLA
    JML $019FE5

WW_PickupBoard:
    LDA.L !WWPrevious2
    AND.B #$20
    BEQ .reject
    LDA.L !WWPrevious1
    AND.B #$40
    BEQ .reject
    JML $01E6D3                   ; Guard+Punch: execute the native pickup body.
.reject:
    JML $01E6E3                   ; Otherwise take the original BVC rejection.

WW_PickupContact:
    LDA.W $140D                   ; Original SpinJumpFlag / PlayerRidingYoshi test.
    ORA.W $187A
    BNE .native
    LDA.L !WWPrevious2
    AND.B #$20
    BEQ .mask
    LDA.L !WWPrevious1
    AND.B #$40
    BEQ .mask
    LDA.B !Held1
    ORA.B #$40
    STA.B !Held1
    LDA.B #$00                    ; Preserve the original spin/Yoshi Z result.
    JML $01AA48
.mask:
    LDA.B !Held1
    AND.B #$BF
    STA.B !Held1
    LDA.B #$00
.native:
    JML $01AA48

; Use the attack box prepared by WW_SetMeleeBox to hit one Map16 block.
WW_BlockAttack:
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BEQ .early_done
    LDA.L !WWMoveHit
    BNE .early_done
    LDA.L !WWMove
    BEQ .early_done
    REP #$20
    LDA.L !WWDrawX
    STA.W $009A                 ; TouchBlockXPos, including the screen high byte.
    LDA.L !WWDrawY
    STA.W $0098                 ; TouchBlockYPos, including the screen high byte.
    SEP #$20
    LDA.B #$01
    STA.L !WWMoveHit
    STA.L !WWNativeCall
    PEA $F5B6                     ; native RTS returns through the existing damage hook
    JML $00F465                   ; Y = Map16 tile number at the attack point.
.early_done:
    RTS
WW_BlockAttackAfterLookup:
    TYA
    BEQ .done
    LDY.B #$01                  ; Interaction side 1 (side activation, not head-only).
    JSL $00F127                 ; Native block generation/collision handling.
.done:
    RTS
