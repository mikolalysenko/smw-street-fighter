; Full-roster per-frame combat, controller handling, vitality and projectiles.

WW_Main:
    SEP #$30
if defined("WW_PERFORMANCE")
    JSL WW_PerfStart
endif
    JSR WW_ReadInput
    LDA.W !MessageBox
    BEQ .message_clear
    JMP .native_only
.message_clear:
    LDA.W $0109
    BEQ .game_level
    JMP .native_only               ; Native intro level and its delayed message.
.game_level:
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BNE .ordinary_menu
    LDA.L !WWBossActive
    BEQ .ordinary_menu
    JSR WW_BossTick
    BRA .check_pause
.ordinary_menu:
    LDA.L !WWMenu
    BEQ .check_pause
    JSR WW_MenuTick
    JMP .mask_all
.check_pause:
    LDA.W !PauseFlag
    BEQ .not_paused
    STZ.W !PauseFlag
    STZ.W $13D3
.not_paused:
    LDA.B !Held1
    AND.B #$10
    BEQ .no_pause
    LDA.B !Held1
    AND.B #$30
    CMP.B #$30
    BNE .pause_roster
    LDA.B #!MenuFinale
    STA.L !WWMenu
    JMP .mask_all
.pause_roster:
    JSR WW_SaveNativeScreen
    LDA.B #!MenuSelect
    STA.L !WWMenu
    LDA.B #!WW_SND_PLAYER_SELECT
    JSL WW_AudioRequest
    JMP .mask_all
.no_pause:
    LDA.W !EndLevelTimer
    BNE .frozen
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BEQ .combat
    LDA.B !SpriteLock
    ORA.B !PlayerAnimation
    BNE .frozen
.combat:

    JSR WW_Initialize
    LDA.L !WWMenu
    BEQ .combat_moves
    LDA.L !WWBossActive
    BEQ .menu_only
.menu_only:
    JSR WW_MenuTick
    JMP .mask_all
.combat_moves:
    STZ.W !PauseFlag
    STZ.W $13D3
    JSR WW_UpdateFacing
    JSR WW_UpdateGuard
    JSR WW_UpdateMove
    JSR WW_UpdateShots
    JSR WW_SelectPose
    JSR WW_ResolveAction
    JSR WW_ResolveShot
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BNE .no_boss_graphics
    JSR WW_ResolveBoss
    JSR WW_UploadBossFrame
.no_boss_graphics:
    JSR WW_UpdateCombo
    LDA.B #$01
    STA.B !Powerup
    STZ.W !StarTimer

    ; B and D-pad remain native. Combat owns Y/X/A/L/R.
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
    BRA .native
.frozen:
    LDA.W !PauseFlag
    BNE .native
    LDA.B #$00
    STA.L !WWMove
    STA.L !WWMoveTimer
    JSR WW_ClearShots
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BNE .native
    JSR WW_ResolveBoss
    JSR WW_UploadBossFrame
    BRA .native
.mask_all:
    STZ.B !SpriteLock
.native:
    LDA.L !WWAudioBootDelay
    BEQ .audio_update
    CMP.B #$80
    BNE .audio_countdown
    LDA.B #$00
    STA.L !WWAudioBootDelay
    BRA .audio_update
.audio_countdown:
    DEC A
    STA.L !WWAudioBootDelay
    BNE .native_only
    LDA.L !WWMenu
    CMP.B #!MenuSelect
    BNE .native_only
    LDA.B #!WW_SND_PLAYER_SELECT
    JSL WW_AudioRequest
    LDA.B #$80                       ; keep custom DMA off for the native upload frame
    STA.L !WWAudioBootDelay
    BRA .native_only
.audio_update:
    JSL WW_AudioUpdate
.native_only:
    LDA.W $4200
    ORA.B #$81
    STA.W $4200
    SEP #$30
    LDA.W !MessageBox
    BEQ .no_message
    JML $00A1DF
.no_message:
    JML $00A1E4

WW_ReadInput:
    LDA.W $4218
    AND.B #$F0
    STA.B !Held2
    LDA.W $4219
    STA.B !Held1
    LDA.W $4218
    AND.B #$C0
    ORA.B !Held1
    STA.B !Held1
    LDA.B !Held2
    EOR.L !WWPrevious2
    AND.B !Held2
    STA.B !Press2
    LDA.B !Held1
    EOR.L !WWPrevious1
    AND.B !Held1
    STA.B !Press1
    LDA.B !Held2
    STA.L !WWPrevious2
    LDA.B !Held1
    STA.L !WWPrevious1
    RTS

WW_Initialize:
    REP #$20
    LDA.L !WWFrameCounter
    INC A
    STA.L !WWFrameCounter
    SEP #$20
    LDA.L !WWInitialized
    CMP.B #$5A
    BEQ .check_level
    LDA.L !WWSelectedBackup
    BEQ .first
    STA.L !WWSelected
    LDA.L !WWChoiceBackup
    STA.L !WWChoice
    LDA.L !WWCharacterBackup
    STA.L !WWCharacter
    BRA .reset
.check_level:
    LDA.L !WWDeath
    BNE .reset
    REP #$20
    LDA.W !CurrentLevel
    CMP.L !WWLevel
    SEP #$20
    BNE .reset
    RTS
.first:
    LDA.B #$00
    STA.L !WWCharacter
    STA.L !WWChoice
    STA.L !WWSelected
    LDA.B #100
    STA.L !WWMeter
    LDA.B #$00
    STA.L !WWMeter+1
.reset:
    LDA.L $7F0E00
    BEQ .audio_stopped
    JSL WW_AudioStop
.audio_stopped:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_RosterVitality,X
    STA.L !WWHealth
    LDA.B #$00
    STA.L !WWDeath
    STA.L !WWMove
    STA.L !WWMoveTimer
    STA.L !WWSuperBuffer
    STA.L !WWAirSpecial
    STA.L !WWFlash
    STA.L !WWGuardAge
    STA.L !WWCombo
    STA.L !WWComboTimer
    STA.L !WWStun
    STA.L !WWLevelHurt
    STA.L !WWRapidCount
    STA.L !WWRapidTimer
    STA.L !WWBossAction
    LDA.B #$FF
    STA.L !WWGrabbed
    STA.L !WWLastPose
    STA.L !WWLastPalette
    STA.L !WWLastShotGfx
    STA.L !WWLastCharacter
    LDA.B #$00
    STA.L !WWUploadSet
    JSR WW_ClearShots
    LDA.B !PlayerDirection
    STA.L !WWFacing
    REP #$20
    LDA.W !CurrentLevel
    STA.L !WWLevel
    CMP.W #$00C7
    BNE .not_finale
    LDA.B #!MenuFinale
    STA.L !WWMenu
    JSR WW_BossInit
    LDA.B #$01
    STA.L !WWBossActive
.not_finale:
    SEP #$20
    LDA.B #$5A
    STA.L !WWInitialized
    LDA.L !WWSelected
    BNE .done
    JSR WW_SaveNativeScreen
    LDA.B #!MenuSelect
    STA.L !WWMenu
    LDA.B #30                        ; native level music/sample uploads run first
    STA.L !WWAudioBootDelay
.done:
    RTS

WW_UpdateFacing:
    LDA.B !Held1
    AND.B #$03
    BEQ .done
    AND.B #$01
    BEQ .left
    LDA.B #$00
    STA.L !WWFacing
    RTS
.left:
    LDA.B #$01
    STA.L !WWFacing
.done:
    RTS

WW_UpdateGuard:
    LDA.B !Held2
    AND.B #$20
    BEQ .clear
    LDA.L !WWMove
    BNE .clear
    LDA.L !WWGuardAge
    CMP.B #$FF
    BCS .done
    INC A
    STA.L !WWGuardAge
.done:
    RTS
.clear:
    LDA.B #$00
    STA.L !WWGuardAge
    RTS

WW_UpdateCombo:
    LDA.L !WWComboTimer
    BEQ .done
    DEC A
    STA.L !WWComboTimer
    BNE .done
    LDA.B #$00
    STA.L !WWCombo
.done:
    RTS

WW_UpdateMove:
    LDA.B !Press2
    AND.B #$10
    BEQ .no_super_buffer
    LDA.B #$01
    STA.L !WWSuperBuffer
.no_super_buffer:
    LDA.L !WWFlash
    BEQ .no_flash
    DEC A
    STA.L !WWFlash
.no_flash:
    LDA.B !PlayerInAir
    BNE .air
    LDA.B #$00
    STA.L !WWAirSpecial
.air:
    LDA.L !WWGrabbed
    CMP.B #$FF
    BEQ .no_grab
    JSR WW_UpdateGrab
.no_grab:
    LDA.L !WWMove
    BEQ .request_jump
    INC A
    LDA.L !WWMoveTimer
    INC A
    STA.L !WWMoveTimer
    LDA.L !WWMove
    TAX
    LDA.L !WWMoveTimer
    CMP.L WW_MoveDurations,X
    BCC .active
    LDA.B #$00
    STA.L !WWMove
    STA.L !WWMoveTimer
    STA.L !WWMoveHit
    RTS
.request_jump:
    JMP .request
.active:
    LDA.L !WWFacing
    STA.B !PlayerDirection
    LDA.L !WWMove
    CMP.B #!MoveShoryuken
    BEQ .shoryuken
    CMP.B #!MoveHurricane
    BEQ .hurricane
    CMP.B #!MoveRush
    BEQ .rush
    CMP.B #!MoveDive
    BEQ .dive
    CMP.B #!MoveTeleport
    BEQ .teleport
    CMP.B #!MoveHadouken
    BNE .not_hadouken
    JMP .spawn_check
.not_hadouken:
    CMP.B #!MoveFlame
    BNE .not_flame
    JMP .spawn_check
.not_flame:
    CMP.B #!MoveSuper
    BNE .not_super
    JMP .super_update
.not_super:
    JMP .melee_check
.shoryuken:
    LDA.L !WWMoveTimer
    CMP.B #$01
    BNE .shoryu_motion
    LDA.B #$B4
    STA.B !PlayerYSpeed
    LDA.B #$01
    STA.B !PlayerInAir
    STA.L !WWAirSpecial
.shoryu_motion:
    LDA.L !WWMoveTimer
    CMP.B #22
    BCC .shoryu_speed
    JMP .melee_check
.shoryu_speed:
    JSR WW_SpecialForwardSpeed
    JMP .melee_check
.hurricane:
    LDA.B #$01
    STA.L !WWAirSpecial
    JSR WW_SpecialForwardSpeed
    LDA.B !PlayerInAir
    BNE .hurricane_air
    JMP .melee_check
.hurricane_air:
    STZ.B !PlayerYSpeed
    JMP .melee_check
.rush:
    JSR WW_SpecialForwardSpeed
    LDA.B #$01
    STA.L !WWAirSpecial
    JMP .melee_check
.dive:
    JSR WW_SpecialForwardSpeed
    LDA.B #$30
    STA.B !PlayerYSpeed
    LDA.B #$01
    STA.B !PlayerInAir
    JMP .melee_check
.teleport:
    LDA.L !WWMoveTimer
    CMP.B #13
    BEQ .teleport_go
    JMP .done
.teleport_go:
    REP #$20
    LDA.B !PlayerX
    PHA
    LDA.L !WWFacing
    TAX
    PLA
    CPX.B #$00
    BNE .teleport_left
    CLC
    ADC.W #96
    BRA .teleport_store
.teleport_left:
    SEC
    SBC.W #96
.teleport_store:
    STA.B !PlayerX
    SEP #$20
    RTS
.spawn_check:
    LDA.L !WWMoveTimer
    CMP.B #12
    BNE .flame_check
    LDA.B #$00
    JSR WW_SpawnShot
    RTS
.flame_check:
    LDA.L !WWMove
    CMP.B #!MoveFlame
    BEQ .flame_move
    JMP .done
.flame_move:
    LDA.L !WWMoveTimer
    CMP.B #10
    BEQ .flame_emit
    JMP .done
.flame_emit:
    LDA.B #$02
    JSR WW_SpawnShot
    RTS
.super_update:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_RosterSuperKind,X
    BEQ .super_shots
    CMP.B #$05
    BEQ .super_shots
    CMP.B #$01
    BEQ .super_rush
    CMP.B #$02
    BEQ .super_flurry
    CMP.B #$03
    BEQ .super_throw
    CMP.B #$04
    BEQ .super_electric
    RTS
.super_rush:
    JSR WW_SpecialForwardSpeed
    JMP .melee_check
.super_flurry:
    JMP .melee_check
.super_throw:
    JMP .melee_check
.super_electric:
    JMP .melee_check
.super_shots:
    LDA.L !WWMoveTimer
    CMP.B #16
    BEQ .emit
    CMP.B #24
    BEQ .emit_second
    CMP.B #32
    BEQ .emit_third
    JMP .done
.emit_second:
    LDA.L !WWSuperLevel
    CMP.B #2
    BCC .done
    BRA .emit
.emit_third:
    LDA.L !WWSuperLevel
    CMP.B #3
    BCC .done
.emit:
    LDA.B #$01
    JSR WW_SpawnShot
    RTS
.melee_check:
    LDA.L !WWMove
    TAX
    LDA.L WW_MeleeBegin,X
    CMP.L !WWMoveTimer
    BCC .done
    LDA.L WW_MeleeEnd,X
    CMP.L !WWMoveTimer
    BCC .done
    BEQ .done
    JSR WW_SetMeleeBox
    LDA.B #$00
    STA.L !WWHitSource
    JSR WW_HitSprites
    JSR WW_BlockAttack
.done:
    RTS
.request:
    LDA.L !WWSuperBuffer
    BNE .start_buffered_super
    LDA.B !Held2
    AND.B #$10
    BEQ .special
.start_buffered_super:
    LDA.B #$00
    STA.L !WWSuperBuffer
    LDA.L !WWSuperLevel
    JSR WW_StartSuper
    RTS
.special:
    LDA.B !Held2
    AND.B #$80
    BEQ .punch
    LDA.B !Held1
    AND.B #$08
    BEQ .down_special
    LDA.L !WWCharacter
    TAX
    LDA.L WW_RosterUp,X
    JMP .start_checked
.down_special:
    LDA.B !Held1
    AND.B #$04
    BEQ .neutral_special
    LDA.L !WWCharacter
    TAX
    LDA.L WW_RosterDown,X
    JMP .start_checked
.neutral_special:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_RosterSpecial,X
    JMP .start_checked
.punch:
    LDA.B !Held2
    AND.B #$20
    BNE .done
    LDA.B !Held1
    AND.B #$40
    BEQ .kick
    LDA.B #!MovePunch
    JSR WW_StartMove
    RTS
.kick:
    LDA.B !Press2
    AND.B #$40
    BEQ .done
    LDA.B #!MoveKick
    JSR WW_StartMove
    RTS
.start_checked:
    CMP.B #!MoveShoryuken
    BEQ .check_air
    CMP.B #!MoveHurricane
    BEQ .check_air
    CMP.B #!MoveRush
    BEQ .check_air
    CMP.B #!MoveDive
    BEQ .check_air
    JMP WW_StartMove
.check_air:
    PHA
    LDA.B !PlayerInAir
    BEQ .air_ok
    LDA.L !WWAirSpecial
    BNE .air_denied
.air_ok:
    PLA
    JMP WW_StartMove
.air_denied:
    PLA
    RTS

WW_StartSuper:
    REP #$20
    LDA.L !WWMeter
    CMP.W #100
    BCC .not_ready
    LDX.B #$01
    CMP.W #200
    BCC .level_ready
    LDX.B #$02
    CMP.W #300
    BCC .level_ready
    LDX.B #$03
.level_ready:
    SEP #$20
    TXA
    JSR WW_StartMoveSuper
    LDA.B #18
    STA.L !WWFlash
    LDA.B #$0D
    STA.W $1DF9
.not_ready:
    SEP #$20
    RTS

; A currently contains super level 1-3. StartMoveSuper preserves it and
; subtracts level*100 from the meter before starting the shared super state.
WW_StartMoveSuper:
    PHA
    LDA.B #!MoveSuper
    JSR WW_StartMove
    PLA
    STA.L !WWSuperLevel
    TAX
    LDA.B #$00
    STA.L !WWMeter
    STA.L !WWMeter+1
    CPX.B #$02
    BCC .one
    LDA.B #100
    STA.L !WWMeter
    CPX.B #$03
    BCC .one
    LDA.B #200
    STA.L !WWMeter
.one:
    RTS

WW_StartMove:
    STA.L !WWMove
    LDA.B #$00
    STA.L !WWMoveTimer
    STA.L !WWMoveHit
    JSR WW_PlayMoveSound
    LDA.B #$03
    STA.W $1DF9
    RTS

WW_PlayMoveSound:
    LDA.L !WWMove
    CMP.B #!MoveSuper
    BEQ .super
    CMP.B #!MovePunch
    BEQ .swing
    CMP.B #!MoveKick
    BEQ .swing
    CMP.B #!MoveHadouken
    BEQ .special
    CMP.B #!MoveFlame
    BEQ .special
    CMP.B #!MoveShoryuken
    BEQ .up
    CMP.B #!MoveHurricane
    BEQ .down
    CMP.B #!MoveRush
    BEQ .down
    CMP.B #!MoveDive
    BEQ .down
    CMP.B #!MoveThrow
    BEQ .up
    CMP.B #!MoveTeleport
    BEQ .up
    CMP.B #!MoveElectric
    BEQ .down
    CMP.B #!MoveFlurry
    BEQ .up
    RTS
.swing:
    LDA.B #!WW_SND_SWING
    JMP .play
.special:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_SoundSpecial,X
    JMP .play
.up:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_SoundUp,X
    JMP .play
.down:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_SoundDown,X
    JMP .play
.super:
    LDA.L !WWCharacter
    TAX
    LDA.L WW_SoundSuper,X
.play:
    JSL WW_AudioRequest
    RTS

WW_SpecialForwardSpeed:
    LDA.L !WWMove
    CMP.B #!MoveSuper
    BNE .normal
    LDA.L !WWSuperLevel
    ASL A
    CLC
    ADC.B #28
    BRA .sign
.normal:
    LDA.B #28
.sign:
    PHA
    LDA.L !WWFacing
    TAX
    PLA
    CPX.B #$00
    BNE .left
    STA.B !PlayerXSpeed
    RTS
.left:
    EOR.B #$FF
    INC A
    STA.B !PlayerXSpeed
    RTS

WW_MeleeBegin:
    db 0,5,7,0,4,2,4,4,2,4,0,2,0,3
WW_MeleeEnd:
    db 0,11,15,0,60,21,29,34,62,12,0,58,0,40

WW_SelectPose:
    LDA.L !WWMove
    BEQ .normal
    TAX
    LDA.L WW_MovePoses,X
    STA.L !WWPose
    RTS
.normal:
    LDA.L !WWStun
    BNE .hurt
    LDA.L !WWGuardAge
    BNE .guard
    LDA.B !PlayerInAir
    BNE .jump
    LDA.B !PlayerIsDucking
    BNE .crouch
    LDA.B !PlayerXSpeed
    BEQ .idle
    LDA.W $0014
    AND.B #$08
    BEQ .idle
    LDA.B #!PoseWalk
    STA.L !WWPose
    RTS
.idle:
    LDA.B #!PoseIdle
    STA.L !WWPose
    RTS
.jump:
    LDA.B #!PoseJump
    STA.L !WWPose
    RTS
.crouch:
    LDA.B #!PoseCrouch
    STA.L !WWPose
    RTS
.guard:
    LDA.B #!PoseGuard
    STA.L !WWPose
    RTS
.hurt:
    LDA.B #!PoseHurt
    STA.L !WWPose
    RTS

; Four 16-byte shot records. $10-$12 is a long pointer to the record base.
WW_SaveStripe:
    LDA.B $12
    STA.L !WWNativeStripe
    RTS
WW_RestoreStripe:
    LDA.L !WWNativeStripe
    STA.B $12
    RTS

WW_SetShotPointer:
    LDA.B #$80
    STA.B $10
    LDA.B #$0D
    STA.B $11
    LDA.B #$7F
    STA.B $12
    RTS

WW_ClearShots:
    JSR WW_SaveStripe
    JSR WW_SetShotPointer
    LDY.B #0
    LDA.B #$00
.loop:
    STA [$10],Y
    INY #16
    CPY.B #64
    BNE .loop
    JSR WW_RestoreStripe
    RTS

; A = projectile type. Returns with a free record initialized, if available.
WW_SpawnShot:
    STA.B $0E
    JSR WW_SaveStripe
    JSR WW_SetShotPointer
    LDY.B #0
.find:
    LDA [$10],Y
    BEQ .found
    INY #16
    CPY.B #64
    BNE .find
    JSR WW_RestoreStripe
    RTS
.found:
    TYA
    STA.L !WWMoveArg
    LDA.B #$01
    STA [$10],Y
    LDA.B $0E
    INY
    STA [$10],Y
    DEY
    LDA.B $0E
    TAX
    LDA.L WW_ShotLife,X
    PHX
    LDY.B #$08
    STA [$10],Y
    PLY                         ; Y = type.
    REP #$20
    LDA.B !PlayerY
    CLC
    ADC.W #8
    LDY.B #$04
    STA [$10],Y
    LDA.B !PlayerX
    PHA
    LDA.L !WWFacing
    TAX
    PLA
    CPX.B #$00
    BNE .left
    CLC
    ADC.W #24
    BRA .x
.left:
    SEC
    SBC.W #24
.x:
    LDY.B #$02
    STA [$10],Y
    TYX
    LDA.L WW_ShotSpeed,X
    PHA
    LDA.L !WWFacing
    TAX
    PLA
    CPX.B #$00
    BEQ .velocity
    EOR.W #$FFFF
    INC A
.velocity:
    LDY.B #$06
    STA [$10],Y
    TYX
    LDA.L WW_ShotDamage,X
    LDY.B #$0A
    STA [$10],Y
    SEP #$20
    LDA.B #$0D
    STA.W $1DF9
    JSR WW_RestoreStripe
    RTS

WW_ShotLife:
    db 90,120,80,90,100,90,90,90
WW_ShotSpeed:
    dw 4,6,3,5,3,3,4,4
WW_ShotDamage:
    db 1,3,2,1,2,1,1,1

WW_UpdateShots:
    JSR WW_SaveStripe
    JSR WW_SetShotPointer
    LDA.B #$00
    STA.L !WWMoveArg
.loop:
    LDA.L !WWMoveArg
    TAY
    LDA [$10],Y
    BNE .active
.next:
    LDA.L !WWMoveArg
    CLC
    ADC.B #16
    STA.L !WWMoveArg
    CMP.B #64
    BNE .loop
    JSR WW_RestoreStripe
    RTS
.active:
    LDY.B #$08
    LDA [$10],Y
    DEC A
    STA [$10],Y
    BNE .life_ok
    JMP .deactivate
.life_ok:
    LDA.L !WWMoveArg
    TAY
    INY #2
    REP #$20
    LDA [$10],Y
    INY #4
    CLC
    ADC [$10],Y
    DEY #4
    STA [$10],Y
    SEC
    SBC.B !Layer1X
    CMP.W #300
    BCS .deactivate16
    SEP #$20
    LDA.L !WWMoveArg
    TAY
    INY
    LDA [$10],Y
    CMP.B #$05
    BNE .player_shot
    JSR WW_UpdateEnemyShot
    JMP .next
.player_shot:
    LDA.L !WWMoveArg
    TAY
    INY #2
    LDA [$10],Y
    STA.B $00
    INY
    LDA [$10],Y
    STA.B $08
    LDA.L !WWMoveArg
    TAY
    INY #4
    LDA [$10],Y
    STA.B $01
    INY
    LDA [$10],Y
    STA.B $09
    LDA.L !WWMoveArg
    TAY
    INY
    LDA [$10],Y
    TAX
    LDA.L WW_ShotWidth,X
    STA.B $02
    LDA.L WW_ShotHeight,X
    STA.B $03
    CPX.B #$01
    BNE .normal_source
    LDA.B #$02
    BRA .source
.normal_source:
    LDA.B #$01
.source:
    STA.L !WWHitSource
    JSR WW_HitSprites
    JMP .next
.deactivate16:
    SEP #$20
.deactivate:
    LDA.L !WWMoveArg
    TAY
    LDA.B #$00
    STA [$10],Y
    JMP .next
WW_ShotWidth:
    db 16,32,32,16,16,16,16,16
WW_ShotHeight:
    db 16,24,24,16,20,16,16,16

WW_SetMeleeBox:
    REP #$20
    LDA.B !PlayerY
    CLC
    ADC.W #6
    STA.L !WWDrawY
    LDA.B !PlayerX
    PHA
    LDA.L !WWFacing
    TAX
    PLA
    CPX.B #$00
    BNE .left
    CLC
    ADC.W #12
    BRA .x
.left:
    SEC
    SBC.W #32
.x:
    STA.L !WWDrawX
    SEP #$20
    LDA.L !WWDrawY
    STA.B $01
    LDA.L !WWDrawY+1
    STA.B $09
    LDA.L !WWDrawX
    STA.B $00
    LDA.L !WWDrawX+1
    STA.B $08
    LDA.L !WWMove
    TAX
    LDA.L WW_MeleeWidth,X
    STA.B $02
    LDA.L WW_MeleeHeight,X
    STA.B $03
    LDA.L !WWCharacter
    CMP.B #$07                 ; Dhalsim's stretched normals.
    BNE .vega
    LDA.L !WWMove
    CMP.B #!MoveKick+1
    BCS .vega
    LDA.B #40
    STA.B $02
.vega:
    LDA.L !WWCharacter
    CMP.B #$09
    BNE .electric
    LDA.L !WWMove
    CMP.B #!MoveKick+1
    BCS .electric
    LDA.B $02
    CLC
    ADC.B #12
    STA.B $02
.electric:
    LDA.L !WWMove
    CMP.B #!MoveElectric
    BNE .done
    LDA.L !WWFacing
    BNE .electric_left
    LDA.B $00
    SEC
    SBC.B #24
    STA.B $00
    BRA .electric_width
.electric_left:
    LDA.B $00
    CLC
    ADC.B #4
    STA.B $00
.electric_width:
    LDA.B #48
    STA.B $02
    LDA.B #32
    STA.B $03
.done:
    RTS
WW_MeleeWidth:
    db 0,20,28,0,32,20,32,32,24,28,0,48,0,32
WW_MeleeHeight:
    db 0,20,16,0,28,30,20,24,24,28,0,32,0,24

WW_HitSprites:
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BNE .normal
    JSR WW_BossHit
    RTS
.normal:
    JSR WW_BossHit
    LDX.B #11
.loop:
    LDA.W !SpriteStatus,X
    CMP.B #$08
    BEQ .status_ok
    CMP.B #$09
    BEQ .status_ok
    JMP .next
.status_ok:
    LDA.W !SpriteOffscreenX,X
    ORA.W !SpriteOffscreenY,X
    BEQ .visible
    JMP .next
.visible:
    LDA.W !SpriteTweakerD,X
    BMI .next
    AND.B #$02
    BNE .next
    LDA.W !SpriteNumber,X
    CMP.B #$C9
    BCS .next
    CMP.B #$74
    BCC .target
    CMP.B #$85
    BCC .next
.target:
    JSL $03B69F
    JSL $03B72B
    BCC .next
    LDA.L !WWMove
    CMP.B #!MoveThrow
    BEQ .grab
    LDA.L !WWMove
    CMP.B #!MoveSuper
    BNE .kill
    LDA.L !WWCharacter
    CMP.B #$06
    BEQ .grab
.kill:
    JSR WW_KillSprite
    BRA .after
.grab:
    LDA.L !WWGrabbed
    CMP.B #$FF
    BEQ .grab_ok
    JMP .next
.grab_ok:
    TXA
    STA.L !WWGrabbed
    LDA.B #30
    STA.L !WWGrabTimer
    LDA.B #60
    STA.W $154C,X
    BRA .after
.after:
    LDA.L !WWHitSource
    CMP.B #$01
    BEQ .shot_spent
    CMP.B #$02
    BEQ .next
    LDA.B #12
    JSR WW_AddMeter
    BRA .next
.shot_spent:
    JSR WW_SetShotPointer
    LDA.L !WWMoveArg
    TAY
    LDA.B #$00
    STA [$10],Y
    LDA.B #12
    JSR WW_AddMeter
    RTS
.next:
    DEX
    BMI .done
    JMP .loop
.done:
    RTS

WW_UpdateGrab:
    LDA.L !WWGrabbed
    TAX
    LDA.L !WWGrabTimer
    BEQ .slam
    DEC A
    STA.L !WWGrabTimer
    LDA.B !PlayerX
    STA.B $E4,X
    LDA.B !PlayerX+1
    STA.W !SpriteXHigh,X
    LDA.B !PlayerY
    STA.B $D8,X
    LDA.B !PlayerY+1
    STA.W !SpriteYHigh,X
    RTS
.slam:
    JSR WW_KillSprite
    LDA.B #$FF
    STA.L !WWGrabbed
    LDA.B #24
    JSR WW_AddMeter
    RTS

WW_KillSprite:
    LDA.B #$02
    STA.W !SpriteStatus,X
    LDA.B #$D0
    STA.W !SpriteYSpeed,X
    LDA.L !WWFacing
    BNE .left
    LDA.B #$20
    BRA .speed
.left:
    LDA.B #$E0
.speed:
    STA.W !SpriteXSpeed,X
    LDA.B #!WW_SND_HIT
    JSL WW_AudioRequest
    LDA.B #$03
    STA.W $1DF9
    JSR WW_AwardCombo
    RTS

WW_AwardCombo:
    PHX
    LDA.L !WWCombo
    INC A
    CMP.B #12
    BCC .count
    LDA.B #11
.count:
    STA.L !WWCombo
    LDA.B #90
    STA.L !WWComboTimer
    LDA.L !WWCombo
    DEC A
    ASL A
    TAX
    REP #$20
    LDA.W $0F34
    CLC
    ADC.L WW_ComboAwards,X
    STA.W $0F34
    SEP #$20
    BCC .done
    INC.W $0F36
.done:
    PLX
    RTS

WW_AddMeter:
    PHX
    REP #$20
    AND.W #$00FF
    CLC
    ADC.L !WWMeter
    CMP.W #!MaxMeter
    BCC .store
    LDA.W #!MaxMeter
.store:
    STA.L !WWMeter
    SEP #$20
    PLX
    RTS

; Skip native player/sprite simulation for the duration of a custom menu.
WW_AfterDraw:
    SEP #$30
if defined("WW_PERFORMANCE")
    JSL WW_PerfEnd
endif
    LDA.L !WWInitialized
    CMP.B #$5A
    BNE .nmi_ready
    LDA.W $4200
    ORA.B #$81
    STA.W $4200
.nmi_ready:
    LDA.L !WWMenu
    BEQ .normal
    CMP.B #!MenuFinale
    BEQ .boss_draw
    JML $00A2F0                 ; GM14's final JMP ConsolidateOAM.
.boss_draw:
    JSR WW_DrawBoss
    JML $00A2F0
.normal:
    LDA.B #$13
    STA.W $212C                   ; custom HUD; no native BG3/status layer
    REP #$20
    LDA.B !PlayerX
    JML $00A2F7

WW_Hurt:
    SEP #$30
    LDA.L !WWNativeCall
    BEQ .normal_damage
    DEC A
    BEQ .block_return
.block_return:
    LDA.B #$00
    STA.L !WWNativeCall
    JML WW_BlockAttackAfterLookup
.normal_damage:
    LDA.W !GameMode
    CMP.B #$14
    BEQ .in_level
    JMP .native
.in_level:
    LDA.B !PlayerAnimation
    BEQ .check_invulnerability
    JMP .return
.check_invulnerability:
    LDA.W !IFrameTimer
    ORA.W !StarTimer
    ORA.W !EndLevelTimer
    BEQ .check_health
    JMP .return
.check_health:
    LDA.L !WWHealth
    BNE .health_ok
    JMP .return
.health_ok:
    LDA.B !Held2
    AND.B #$20
    BEQ .unblocked
    LDA.L !WWMove
    BNE .unblocked
    LDA.L !WWGuardAge
    BEQ .unblocked
    CMP.B #7
    BCS .blocked
    LDA.B #24
    JSR WW_AddMeter
    LDA.B #18
    STA.W !IFrameTimer
    LDA.B #$09
    STA.W $1DFC
    JMP .return
.blocked:
    LDA.B #2
    BRA .damage
.unblocked:
    LDA.B #20
.damage:
    STA.B $0F
    LDA.L !WWHealth
    SEC
    SBC.B $0F
    BCS .positive
    LDA.B #$00
.positive:
    STA.L !WWHealth
    BEQ .ko
    LDA.L !WWCharacter
    CMP.B #$02
    BNE .normal_hurt
    LDA.B #!WW_SND_CHUN_HURT
    BRA .hurt_sound
.normal_hurt:
    LDA.B #!WW_SND_HURT
.hurt_sound:
    JSL WW_AudioRequest
    LDA.B #$08
    JSR WW_AddMeter
    LDA.B #$3C
    STA.W !IFrameTimer
    LDA.B #$C0
    STA.B !PlayerYSpeed
    LDA.B #$01
    STA.B !PlayerInAir
    LDA.B !PlayerDirection
    BNE .knock_right
    LDA.B #$F0
    BRA .knock
.knock_right:
    LDA.B #$10
.knock:
    STA.B !PlayerXSpeed
    LDA.B #$00
    STA.L !WWMove
    STA.L !WWMoveTimer
    LDA.B #22
    STA.L !WWStun
    STA.L !WWLevelHurt
    LDA.B #$09
    STA.W $1DFC
.return:
    RTL
.ko:
    LDA.L !WWCharacter
    CMP.B #$02
    BNE .normal_ko
    LDA.B #!WW_SND_CHUN_KO
    BRA .ko_sound
.normal_ko:
    LDA.B #!WW_SND_KO
.ko_sound:
    JSL WW_AudioRequest
    LDA.B #$01
    STA.L !WWDeath
    LDA.B #$00
    STA.L !WWMove
    JSR WW_ClearShots
    JML $00F606
.native:
    LDA.B !PlayerAnimation
    BNE .return
    JML $00F5BB
