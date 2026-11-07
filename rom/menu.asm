; Roster selection, continue, ending and finale state transitions.

WW_MenuTick:
    SEP #$30
    LDA.L !WWMenu
    CMP.B #!MenuSelect
    BEQ WW_MenuSelectTick
    CMP.B #!MenuContinue
    BNE .not_continue
    JMP WW_MenuContinueTick
.not_continue:
    CMP.B #!MenuEnding
    BNE .not_ending
    JMP WW_MenuEndingTick
.not_ending:
    CMP.B #!MenuFinale
    BNE .not_finale
    JMP WW_MenuFinaleTick
.not_finale:
    RTS

WW_MenuSelectTick:
    LDA.B !Press1
    AND.B #$01
    BEQ .not_right
    JSR WW_ChoiceRight
    BRA .confirm
.not_right:
    LDA.B !Press1
    AND.B #$02
    BEQ .not_left
    JSR WW_ChoiceLeft
    BRA .confirm
.not_left:
    LDA.B !Press1
    AND.B #$08
    BEQ .not_up
    JSR WW_ChoiceUp
    BRA .confirm
.not_up:
    LDA.B !Press1
    AND.B #$04
    BEQ .confirm
    JSR WW_ChoiceDown
.confirm:
    LDA.B !Press1
    AND.B #$B0                  ; B, Y or Start.
    BNE .accept
    LDA.B !Press2
    AND.B #$80                  ; A.
    BNE .accept
    RTS
.accept:
    LDA.L !WWSelected
    BNE .keep_resources
    LDA.L !WWChoice
    TAX
    LDA.L WW_RosterVitality,X
    STA.L !WWHealth
    LDA.B #100
    STA.L !WWMeter
    LDA.B #$00
    STA.L !WWMeter+1
.keep_resources:
    JSL WW_AudioStop
    LDA.L !WWChoice
    STA.L !WWCharacter
    LDA.B #$00
    STA.L !WWMove
    STA.L !WWMoveTimer
    STA.L !WWMoveHit
    STA.L !WWAirSpecial
    STA.L !WWSuperBuffer
    JSR WW_ClearShots
    LDA.B #$01
    STA.L !WWSelected
    LDA.B #$00
    STA.L !WWMenu
    STA.B !SpriteLock
    STA.W !PauseFlag
    STA.W $13D3
    LDA.B #$FF
    STA.L !WWLastPose
    STA.L !WWLastPalette
    STA.L !WWLastCharacter
    STA.L !WWUploadSet
    LDA.B #!WW_SND_CONFIRM
    JSL WW_AudioRequest
    LDA.B #$09
    STA.W $1DFC
    RTS

WW_ChoiceRight:
    LDA.L !WWChoice
    INC A
    CMP.B #!RosterCount
    BCC .store
    LDA.B #$00
.store:
    STA.L !WWChoice
    LDA.B #!WW_SND_SELECT
    JSL WW_AudioRequest
    LDA.B #$03
    STA.W $1DF9
    RTS
WW_ChoiceLeft:
    LDA.L !WWChoice
    DEC A
    BPL .store
    LDA.B #!RosterCount-1
.store:
    STA.L !WWChoice
    LDA.B #!WW_SND_SELECT
    JSL WW_AudioRequest
    LDA.B #$03
    STA.W $1DF9
    RTS
WW_ChoiceUp:
    LDA.L !WWChoice
    SEC
    SBC.B #6
    BPL .store
    CLC
    ADC.B #!RosterCount
.store:
    STA.L !WWChoice
    LDA.B #!WW_SND_SELECT
    JSL WW_AudioRequest
    LDA.B #$03
    STA.W $1DF9
    RTS
WW_ChoiceDown:
    LDA.L !WWChoice
    CLC
    ADC.B #6
    CMP.B #!RosterCount
    BCC .store
    SEC
    SBC.B #!RosterCount
.store:
    STA.L !WWChoice
    LDA.B #!WW_SND_SELECT
    JSL WW_AudioRequest
    LDA.B #$03
    STA.W $1DF9
    RTS

WW_MenuContinueTick:
    LDA.B !Press1
    AND.B #$B0
    BNE .accept
    LDA.B !Press2
    AND.B #$80
    BNE .accept
    REP #$20
    LDA.L !WWContinueTimer
    BEQ .timeout
    DEC A
    STA.L !WWContinueTimer
    SEP #$20
    RTS
.timeout:
    SEP #$20
    LDA.B #$00
    STA.L !WWMenu
    RTS
.accept:
    LDA.B #$00
    STA.L !WWDeath
    STA.L !WWSelected
    LDA.B #!MenuSelect
    STA.L !WWMenu
    LDA.B #100
    STA.L !WWMeter
    LDA.B #$00
    STA.L !WWMeter+1
    RTS

WW_MenuEndingTick:
    LDA.B !Press1
    AND.B #$B0
    BNE .finish
    LDA.B !Press2
    AND.B #$80
    BNE .finish
    RTS
.finish:
    LDA.B #$00
    STA.L !WWMenu
    STA.B !SpriteLock
    LDA.B #$0B
    STA.W !GameMode
    RTS

WW_MenuFinaleTick:
    LDA.B !Press1
    AND.B #$B0
    BNE .start
    LDA.B !Press2
    AND.B #$80
    BNE .start
    LDA.B !Press1
    AND.B #$20                  ; Select cancels practice warp.
    BNE .cancel
    RTS
.start:
    LDA.B #$01
    STA.L !WWBossActive
    LDX.B #$1F
    LDA.B #$C7
.fill_exit:
    STA.W $010B,X
    DEX
    BPL .fill_exit
    LDA.B #$01
    STA.W $13BF
    STA.W $141A
    STZ.W $0109
    STZ.W $1B93
    STZ.W $141D
    STZ.B !PlayerAnimation
    STZ.W !EndLevelTimer
    LDA.B #$0F
    STA.W !GameMode
    LDA.B #!MenuFinale
    STA.L !WWMenu
    STA.B !SpriteLock
    REP #$20
    LDA.W #500
    STA.L !WWBossHP
    LDA.W #0
    STA.L !WWBossPhase
    STA.L !WWBossTimer
    SEP #$20
    JSR WW_BossInit
    RTS
.cancel:
    LDA.B #$00
    STA.L !WWBossActive
    STA.L !WWMenu
    STA.B !SpriteLock
    RTS
