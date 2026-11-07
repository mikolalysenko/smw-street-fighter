; Custom Akuma/Bowser finale, native collision and boss state.

; smw.asm's historical phase alias overlaps the high byte of the 16-bit HP.
; Keep the phase in a free byte in the same otherwise-unused WRAM page.
!WWBossPhaseState = $7F0D65

WW_BossInit:
    LDA.B #$01
    STA.L !WWBossActive
    REP #$20
    LDA.W #500
    STA.L !WWBossHP
    LDA.W #0
    STA.L !WWBossTimer
    STA.L !WWBossAction
    LDA.W #160
    STA.L !WWBossX
    LDA.W #176
    STA.L !WWBossY
    ; C7 is the castle approach; 1C7 is the native Mode 7 Bowser room.
    ; Entering either the practice exit or C7 directly loads the real arena.
    LDA.W #$01C7
    STA.W !CurrentLevel
    SEP #$20
    ; GM11 reads the sublevel exit table, not CurrentLevel.  Select exit
    ; slot zero on the special submap so its level word is really 1C7.
    LDA.B #$C7
    STA.W $19B8                 ; ExitTableLow[0].
    LDA.B #$01
    STA.W $1F11                 ; OWPlayerSubmap: high level byte = 1.
    STA.W $141A                 ; SublevelCount: use the exit-table path.
    STZ.W $0095                 ; PlayerXPosNext+1: exit-table index zero.
    STZ.W $1B93                 ; UseSecondaryExit: keep the direct 1C7 ID.
    LDA.B #$0F
    STA.W !GameMode
    LDA.B #$00
    STA.L !WWBossPhaseState
    STA.L !WWBossHits
    STA.L !WWMoveHit
    RTS

WW_BossTick:
    STZ.W !EndLevelTimer
    STZ.B !PlayerAnimation
    STZ.B !SpriteLock
    STZ.B $41                   ; Disable native lava/OBJ/color windows.
    STZ.B $42
    STZ.B $43
    STZ.B $44
    STZ.B $40                   ; Do not add the native lava backdrop color.
    STZ.W $2123
    STZ.W $2124
    STZ.W $2125
    STZ.W $212E
    STZ.W $212F
    STZ.W $2130
    STZ.W $2131
    LDA.L !WWBossHP
    ORA.L !WWBossHP+1
    BNE .alive
    STZ.W $2106                 ; Leave the Mode 7 window/mosaic state behind.
    STZ.W $2123
    STZ.W $2124
    STZ.W $2125
    STZ.W $212E
    STZ.W $212F
    STZ.W $2130
    STZ.W $2131
    STZ.W $2132
    STZ.W $2132
    STZ.W $2132
    LDA.B #$02
    STA.B $44
    STA.W $2130
    LDA.B #$20
    STA.B $40
    STA.W $2131
    LDA.B #$1F
    STA.W $2132
    LDA.B #$1C
    STA.W $2132
    LDA.B #$16
    STA.W $2132
    LDA.B #$9F
    STA.W $0701
    LDA.B #$5B
    STA.W $0702
    LDA.B #!MenuEnding
    STA.L !WWMenu
    LDA.B #$00
    STA.L !WWBossActive
    LDA.B #!WW_SND_BOSS_KO
    JSL WW_AudioRequest
    RTS
.alive:
    LDA.L !WWBossTimer
    ORA.L !WWBossTimer+1
    BNE .positioned
    REP #$20
    LDA.W #80
    STA.B !PlayerX
    LDA.W #0
    STA.B !Layer1X
    SEP #$20
    STZ.B !PlayerDirection
    LDA.B #$00
    STA.L !WWFacing
.positioned:
    REP #$20
    LDA.L !WWBossTimer
    INC A
    STA.L !WWBossTimer
    LDA.L !WWBossHP
    CMP.W #300
    BCS .phase_ready
    LDA.L !WWBossHP
    CMP.W #100
    BCS .phase1
    SEP #$20
    LDA.B #$02
    BRA .store_phase
.phase1:
    SEP #$20
    LDA.B #$01
.store_phase:
    STA.L !WWBossPhaseState
.phase_ready:
    SEP #$20
    ; Bit 7 selects fire after a warp; the low seven bits are a cooldown.
    ; Attacks therefore alternate instead of warping forever.
    LDA.L !WWBossAction
    AND.B #$7F
    BEQ .attack_ready
    LDA.L !WWBossAction
    DEC A
    STA.L !WWBossAction
    BRA .done
.attack_ready:
    LDA.L !WWBossPhaseState
    TAX
    LDA.L WW_BossInterval,X
    STA.B $04
    LDA.L !WWBossTimer
    CMP.B $04
    BCC .done
    REP #$20
    LDA.W #0
    STA.L !WWBossTimer
    SEP #$20
    LDA.L !WWBossAction
    BMI .shots
    JSR WW_BossWarp
    BRA .done
.shots:
    JSR WW_BossFire
.done:
    ; GM14's shared menu path is menu-only for nonzero WWMenu.  The finale
    ; runs the same custom combat routines here before native player physics.
    JSR WW_Initialize
    LDA.L !WWInitialized
    CMP.B #$5A
    BNE .return
    LDA.B $10
    PHA
    LDA.B $11
    PHA
    LDA.B $12
    PHA
    LDA.B $13
    PHA
    JSR WW_UpdateFacing
    JSR WW_UpdateGuard
    JSR WW_UpdateMove
    JSR WW_UpdateShots
    JSR WW_SelectPose
    JSR WW_ResolveAction
    JSR WW_ResolveShot
    JSR WW_UpdateCombo
    PLA
    STA.B $13
    PLA
    STA.B $12
    PLA
    STA.B $11
    PLA
    STA.B $10
    LDA.B #$01
    STA.B !Powerup
    STZ.W !StarTimer
    LDA.B !Held1
    AND.B #$BF
    STA.B !Held1
    LDA.B !Press1
    AND.B #$BF
    STA.B !Press1
    LDA.B !Held2
    AND.B #$0F
    STA.B !Held2
    LDA.B !Press2
    AND.B #$0F
    STA.B !Press2
.return:
    RTS

WW_BossInterval:
    db 90,60,40
WW_BossWarp:
    LDA.B #!WW_SND_AKUMA_WARP
    JSL WW_AudioRequest
    LDA.B !PlayerX
    SEC
    SBC.L !WWBossX
    BPL .right
    LDA.B !PlayerX
    CLC
    ADC.B #64
    BRA .store
.right:
    LDA.B !PlayerX
    SEC
    SBC.B #64
.store:
    CMP.B #24
    BCS .minimum_ok
    LDA.B #24
.minimum_ok:
    CMP.B #209
    BCC .store_x
    LDA.B #208
.store_x:
    STA.L !WWBossX
    LDA.B #$90                    ; Next attack: fire, after a 16-frame tell.
    STA.L !WWBossAction
    RTS

WW_BossFire:
    LDA.B #!WW_SND_AKUMA_SUPER
    JSL WW_AudioRequest
    LDA.L !WWBossPhaseState
    INC A
    STA.B $08
    LDA.B #$00
    STA.B $09
.loop:
    LDA.B $09
    CMP.B $08
    BCS .done
    JSR WW_SpawnBossShot
    INC.B $09
    BRA .loop
.done:
    LDA.B #$08                    ; Next attack: warp, after an 8-frame tell.
    STA.L !WWBossAction
    RTS

; Enemy projectile records use the same four slots, with type 5 (boss)
; and type 6 (reflected player fireball).
WW_SpawnBossShot:
    JSR WW_SaveStripe
    JSR WW_SetShotPointer
    LDY.B #$00
.find:
    LDA [$10],Y
    BEQ .found
    INY #16
    CPY.B #64
    BNE .find
    JMP .full
.found:
    TYA
    STA.L !WWMoveArg
    LDA.B #$01
    STA [$10],Y
    LDA.B #$05
    INY
    STA [$10],Y
    INY
    REP #$20
    LDA.L !WWBossX
    STA [$10],Y
    INY #2
    LDA.L !WWBossY
    STA [$10],Y
    SEP #$20
    LDA.B !PlayerX
    SEC
    SBC.L !WWBossX
    BPL .right
    REP #$20
    LDA.W #$FFFD                ; 16-bit -3, not unsigned +253.
    BRA .vx
.right:
    REP #$20
    LDA.W #$0003
.vx:
    STA.B $04
    SEP #$20
    LDA.L !WWBossPhaseState
    BEQ .speed
    REP #$20
    LDA.B $04
    ASL A
    STA.B $04
    SEP #$20
.speed:
    LDY.B #$06
    LDA.B $04
    STA [$10],Y
    INY
    LDA.B $05
    STA [$10],Y
    LDA.B #180
    LDY.B #$08
    STA [$10],Y
    LDA.B #$01
    LDY.B #$0A
    STA [$10],Y
.full:
    JSR WW_RestoreStripe
    RTS

; Called from the normal hit path after an attack clipping box is prepared.
WW_BossHit:
    LDA.L !WWBossActive
    BNE .active
    JMP .done
.active:
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BEQ .finale
    CMP.B #!MenuGame
    BEQ .finale
    JMP .done
.finale:
    LDA.L !WWHitSource
    BNE .check
    LDA.L !WWMoveHit
    BEQ .check
    JMP .done
.check:
    LDA.L !WWHitSource
    BEQ .player_dx
    LDA.L !WWMoveArg
    TAY
    INY #2
    LDA [$10],Y
    BRA .dx_compare
.player_dx:
    LDA.B !PlayerX
.dx_compare:
    SEC
    SBC.L !WWBossX
    BPL .dx
    EOR.B #$FF
    INC A
.dx:
    CMP.B #48
    BCC .dx_ok
    JMP .done
.dx_ok:
    LDA.L !WWHitSource
    BEQ .player_dy
    LDA.L !WWMoveArg
    TAY
    INY #4
    LDA [$10],Y
    BRA .dy_compare
.player_dy:
    LDA.B !PlayerY
.dy_compare:
    SEC
    SBC.L !WWBossY
    BPL .dy
    EOR.B #$FF
    INC A
.dy:
    CMP.B #48
    BCC .dy_ok
    JMP .done
.dy_ok:
    LDA.L !WWHitSource
    BEQ .melee_damage
    CMP.B #$02
    BEQ .super_damage
    ; A reflected shot keeps its damage after the reflecting move ends.
    LDA.L !WWMoveArg
    TAY
    INY
    LDA [$10],Y
    CMP.B #$06
    BEQ .reflected_damage
    LDA.B #10
    BRA .apply_damage
.super_damage:
    LDA.B #40
    BRA .apply_damage
.reflected_damage:
    LDA.B #30
    BRA .apply_damage
.melee_damage:
    LDA.B #$01
    STA.L !WWMoveHit
    LDA.L !WWMove
    TAX
    LDA.L WW_BossDamage,X
.apply_damage:
    STA.B $04
    STZ.B $05
    REP #$20
    LDA.L !WWBossHP
    SEC
    SBC.B $04
    BCS .store
    LDA.W #$0000
.store:
    STA.L !WWBossHP
    SEP #$20
    LDA.L !WWBossHits
    INC A
    STA.L !WWBossHits
    LDA.L !WWHitSource
    BEQ .effects
    ; Projectiles are spent on contact rather than hitting every later move.
    LDA.L !WWMoveArg
    TAY
    LDA.B #$00
    STA [$10],Y
.effects:
    LDA.B #12
    JSR WW_AddMeter
    LDA.B #!WW_SND_HIT
    JSL WW_AudioRequest
.done:
    RTS
WW_BossDamage:
    db 0,10,20,20,40,25,30,35,40,60,0,30,30,35

; Update an Akuma shot: reflect on a connecting melee/special hit, or hurt Mario.
WW_UpdateEnemyShot:
    LDA.L !WWMove
    BEQ .player_hit
    TAX
    LDA.L !WWMoveTimer
    CMP.B #3
    BCC .player_hit
    CMP.B #18
    BCS .player_hit
    CMP.L WW_MeleeBegin,X
    BCC .player_hit
    CMP.L WW_MeleeEnd,X
    BCS .player_hit
    ; Use the real attack rectangle, not merely an active animation timer.
    JSR WW_SetMeleeBox
    LDA.L !WWMoveArg
    TAY
    INY #2
    LDA [$10],Y
    STA.B $04
    INY
    LDA [$10],Y
    STA.B $0A
    LDA.L !WWMoveArg
    TAY
    INY #4
    LDA [$10],Y
    STA.B $05
    INY
    LDA [$10],Y
    STA.B $0B
    LDA.B #16
    STA.B $06
    STA.B $07
    JSL $03B72B
    BCC .player_hit
    LDA.B #$01
    STA.L !WWMoveHit
    LDA.L !WWMoveArg
    TAY
    INY #6
    REP #$20
    LDA [$10],Y
    EOR.W #$FFFF
    INC A
    STA [$10],Y
    SEP #$20
    LDA.L !WWMoveArg
    TAY
    INY
    LDA.B #$06
    STA [$10],Y
    LDA.B #12
    JSR WW_AddMeter
    LDA.B #!WW_SND_BLOCK
    JSL WW_AudioRequest
    RTS
.player_hit:
    ; Compare this shot with native Mario clipping.
    LDA.L !WWMoveArg
    TAY
    INY #2
    LDA [$10],Y
    STA.B $04
    INY
    LDA [$10],Y
    STA.B $0A
    LDA.L !WWMoveArg
    TAY
    INY #4
    LDA [$10],Y
    STA.B $05
    INY
    LDA [$10],Y
    STA.B $0B
    LDA.B #16
    STA.B $06
    STA.B $07
    JSL $03B664                 ; native GetMarioClipping
    JSL $03B72B
    BCC .done
    JML WW_Hurt
.done:
    RTS

; Keep the native Mode 7 arena/NMI and replace only Bowser's sprite renderer.
; Akuma is drawn in OBJ slots; the native battle AI and clown-car OAM are skipped.
WW_BowserNative:
    LDA.W !GameMode
    CMP.B #$14
    BNE .vanilla                ; Native Bowser intro drives the GM13 fade-in.
    LDA.L !WWMenu
    CMP.B #!MenuEnding
    BEQ .ending
    CMP.B #!MenuFinale
    BEQ .custom
.vanilla:
    db $A5,$38,$4A
    JML $03A27D
.custom:
    LDA.B #$01
    STA.B !SpriteLock           ; Native routine takes its documented AI bypass.
    PHX
    JSR WW_UploadBossPaletteOnly
    PLX
    db $A5,$38,$4A
    JML $03A27D                 ; Return directly to the original bank 3 caller.
.ending:
    LDA.B #$01
    STA.B !SpriteLock
    LDA.B #$00
    STA.W $14C8,X               ; Stop native Bowser AI during the custom page.
    STA.W $0680
    STA.W $0681
    STA.W $0682                 ; No more dynamic native palette uploads.
    db $A5,$38,$4A
    JML $03A27D
