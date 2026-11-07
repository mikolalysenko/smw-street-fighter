; Akuma pilot rendering and per-frame graphics resolution.

WW_ResolveBoss:
    JSR WW_SaveStripe
    LDA.B #$00
    STA.L !WWResolveCharacter
    LDA.L !WWBossAction
    AND.B #$7F
    BEQ .idle
    LDA.L !WWBossAction
    BMI .warp
    LDA.B #31                   ; FC_POSE_AKUMA_FIRE.
    BRA .resolve
.warp:
    LDA.B #35                   ; FC_POSE_AKUMA_WARP.
    BRA .resolve
.idle:
    LDA.B #30                   ; FC_POSE_AKUMA_PILOT.
.resolve:
    JSR WW_GetDescriptor
    LDA.L !WWBossTimer
    STA.B $04
    JSR WW_ResolveFrame
    LDA.B $F0
    STA.L !WWBossGfxPointer
    LDA.B $F1
    STA.L !WWBossGfxPointer+1
    LDA.B $F2
    STA.L !WWBossGfxPointer+2
    LDA.B $F7
    STA.L !WWBossOriginX
    LDA.B $F8
    STA.L !WWBossOriginY
    JSR WW_RestoreStripe
    RTS

WW_UploadBossPaletteOnly:
    ; CODE_03DFCC has just prepared the native 16-byte BG entry.  Append
    ; both custom OBJ palettes to its native dynamic-CGRAM upload queue.
    STZ.W $0680                 ; PaletteTableUse_Dynamic.
    LDA.B #$20
    STA.W $0694
    LDA.B #$80
    STA.W $0695
    LDA.L !WWCharacter
    ASL A
    ADC.L !WWCharacter
    TAX
    LDA.L WW_RosterPalettePointers,X
    STA.B $F7
    LDA.L WW_RosterPalettePointers+1,X
    STA.B $F8
    LDA.L WW_RosterPalettePointers+2,X
    STA.B $F9
    LDY.B #$00
.player:
    LDA [$F7],Y
    STA.W $0696,Y
    INY
    CPY.B #$20
    BNE .player
    LDA.B #$20
    STA.W $06B6
    LDA.B #$90
    STA.W $06B7
    LDA.B #((WW_RosterPalettes+$260)&$FF)
    STA.B $F7
    LDA.B #(((WW_RosterPalettes+$260)>>8)&$FF)
    STA.B $F8
    LDA.B #bank(WW_RosterPalettes)
    STA.B $F9
    LDY.B #$00
.boss:
    LDA [$F7],Y
    STA.W $06B8,Y
    INY
    CPY.B #$20
    BNE .boss
    STZ.W $06D8
    LDA.B #$57
    STA.W $0681
    RTS

WW_UploadBossFrame:
    LDA.L !WWBossActive
    BNE .active
    JMP .done
.active:
    LDX.B #31
.save_scratch:
    LDA.B $00,X
    PHA
    DEX
    BPL .save_scratch
    JSR WW_ResolveBoss
    LDX.B #$00
.restore_scratch:
    PLA
    STA.B $00,X
    INX
    CPX.B #32
    BNE .restore_scratch
    LDA.W $2115
    PHA
    LDA.B #$80
    STA.W $2100
    STA.W $2115               ; 4bpp DMA increments after the high byte.
    REP #$20
    LDA.W #$1801
    STA.W $4320               ; Native palettes leave channel 2 on $2122.
    SEP #$20
    ; MenuFinale takes the selection path in WW_DMA, so maintain the gameplay
    ; action and projectile frames here while the native arena keeps running.
    LDA.L !WWLastGfxPointer
    CMP.L !WWGfxPointer
    BNE .player_upload
    LDA.L !WWLastGfxPointer+1
    CMP.L !WWGfxPointer+1
    BNE .player_upload
    LDA.L !WWLastGfxPointer+2
    CMP.L !WWGfxPointer+2
    BEQ .projectile_check
.player_upload:
    LDA.L !WWGfxPointer
    STA.B $F0
    LDA.L !WWGfxPointer+1
    STA.B $F1
    LDA.L !WWGfxPointer+2
    STA.B $F2
    REP #$20
    LDA.W #$6000
    STA.L !WWUploadBase
    SEP #$20
    JSR WW_UploadActionFrame
    LDA.L !WWGfxPointer
    STA.L !WWLastGfxPointer
    LDA.L !WWGfxPointer+1
    STA.L !WWLastGfxPointer+1
    LDA.L !WWGfxPointer+2
    STA.L !WWLastGfxPointer+2
.projectile_check:
    LDA.L !WWLastShotPointer
    CMP.L !WWShotGfxPointer
    BNE .projectile_upload
    LDA.L !WWLastShotPointer+1
    CMP.L !WWShotGfxPointer+1
    BNE .projectile_upload
    LDA.L !WWLastShotPointer+2
    CMP.L !WWShotGfxPointer+2
    BEQ .boss_check
.projectile_upload:
    LDA.L !WWShotGfxPointer
    STA.B $F0
    LDA.L !WWShotGfxPointer+1
    STA.B $F1
    LDA.L !WWShotGfxPointer+2
    STA.B $F2
    JSR WW_UploadProjectileFrame
    LDA.L !WWShotGfxPointer
    STA.L !WWLastShotPointer
    LDA.L !WWShotGfxPointer+1
    STA.L !WWLastShotPointer+1
    LDA.L !WWShotGfxPointer+2
    STA.L !WWLastShotPointer+2
.boss_check:
    LDA.L !WWLastBossGfxPointer
    CMP.L !WWBossGfxPointer
    BNE .boss_upload
    LDA.L !WWLastBossGfxPointer+1
    CMP.L !WWBossGfxPointer+1
    BNE .boss_upload
    LDA.L !WWLastBossGfxPointer+2
    CMP.L !WWBossGfxPointer+2
    BEQ .palettes
.boss_upload:
    LDA.L !WWBossGfxPointer
    STA.B $F0
    LDA.L !WWBossGfxPointer+1
    STA.B $F1
    LDA.L !WWBossGfxPointer+2
    STA.B $F2
    ; Tile 192 in the native OBJ page: $C000 + 192*32 = $D800.
    ; This sits between projectile/HUD art and does not overwrite the player.
    REP #$20
    LDA.W #$6C00
    STA.L !WWUploadBase
    SEP #$20
    JSR WW_UploadActionFrame
    LDA.L !WWBossGfxPointer
    STA.L !WWLastBossGfxPointer
    LDA.L !WWBossGfxPointer+1
    STA.L !WWLastBossGfxPointer+1
    LDA.L !WWBossGfxPointer+2
    STA.L !WWLastBossGfxPointer+2
.palettes:
    ; The native palette DMA still owns CGRAM in the Mode 7 arena.  Reassert
    ; both OBJ palettes every frame without touching the native BG palettes.
    LDA.L !WWCharacter
    ASL A
    ADC.L !WWCharacter
    TAX
    LDA.L WW_RosterPalettePointers,X
    STA.B $F7
    LDA.L WW_RosterPalettePointers+1,X
    STA.B $F8
    LDA.L WW_RosterPalettePointers+2,X
    STA.B $F9
    LDY.B #$80
    STY.W $2121
    JSR WW_UploadPalette
    SEP #$20
    LDY.B #$00
.player_palette_mirror:
    LDA [$F7],Y
    STA.W $0803,Y               ; MainPalette, CGRAM $80.
    STA.W $0A05,Y               ; CopyPalette mirror.
    INY
    CPY.B #32
    BNE .player_palette_mirror
    LDY.B #$90
    STY.W $2121
    LDA.B #((WW_RosterPalettes+$260)&$FF)
    STA.B $F7
    LDA.B #(((WW_RosterPalettes+$260)>>8)&$FF)
    STA.B $F8
    LDA.B #bank(WW_RosterPalettes)
    STA.B $F9
    JSR WW_UploadPalette
    SEP #$20
    LDY.B #$00
.boss_palette_mirror:
    LDA [$F7],Y
    STA.W $0823,Y               ; MainPalette, CGRAM $90.
    STA.W $0A25,Y               ; CopyPalette mirror.
    INY
    CPY.B #32
    BNE .boss_palette_mirror
    LDA.B #$0F
    STA.W $0DAF
    STA.W $2100
    PLA
    STA.W $2115                 ; Restore native Mode 7 VRAM addressing.
.done:
    RTS

WW_DrawFinalePlayer:
    LDA.B !PlayerHiddenTiles
    CMP.B #$FF
    BEQ .done
    LDA.W !IFrameTimer
    AND.B #$01
    BNE .done
    REP #$20
    LDA.L !WWGfxOriginX
    SEP #$20
    STA.B $04
    STZ.B $05
    LDA.L !WWGfxOriginY
    STA.B $06
    STZ.B $07
    REP #$20
    LDA.B !PlayerX
    SEC
    SBC.B !Layer1X
    CLC
    ADC.W #6
    SEC
    SBC.W #24
    STA.L !WWDrawX
    LDA.B !PlayerY
    CLC
    ADC.W #32
    SEC
    SBC.B !Layer1Y
    SEC
    SBC.W #48
    STA.L !WWDrawY
    SEP #$20
    LDA.B #(WW_RosterActionOamRight&$FF)
    STA.B $08
    LDA.B #((WW_RosterActionOamRight>>8)&$FF)
    STA.B $09
    LDA.B #bank(WW_RosterActionOamRight)
    STA.B $0A
    LDA.B !PlayerDirection
    BEQ .table
    LDA.B #(WW_RosterActionOamLeft&$FF)
    STA.B $08
    LDA.B #((WW_RosterActionOamLeft>>8)&$FF)
    STA.B $09
.table:
    LDA.B #$00
    STA.B $0C
    LDA.B #$02
    STA.B $0D
    LDA.B #$20
    STA.B $0E
    LDA.B #$04
    STA.B $0F
    LDA.B #36
    STA.B $10
    JSR WW_DrawTiles
.done:
    RTS

WW_DrawFinaleShots:
    LDA.B #$00
    STA.L !WWMoveArg
.loop:
    JSR WW_SetShotPointer
    LDA.L !WWMoveArg
    TAY
    LDA [$10],Y
    BNE .active
    JMP .next
.active:
    REP #$20
    LDA.L !WWShotOriginX
    SEP #$20
    STA.B $04
    STZ.B $05
    LDA.L !WWShotOriginY
    STA.B $06
    STZ.B $07
    REP #$20
    LDA.L !WWMoveArg
    TAY
    INY #2
    LDA [$10],Y
    SEC
    SBC.B !Layer1X
    SEC
    SBC.W #16
    STA.L !WWDrawX
    LDA.L !WWMoveArg
    TAY
    INY #4
    LDA [$10],Y
    SEC
    SBC.B !Layer1Y
    SEC
    SBC.W #16
    STA.L !WWDrawY
    SEP #$20
    LDA.B #(WW_FinaleShotOamRight&$FF)
    STA.B $08
    LDA.B #((WW_FinaleShotOamRight>>8)&$FF)
    STA.B $09
    LDA.B #bank(WW_FinaleShotOamRight)
    STA.B $0A
    LDA.L !WWMoveArg
    TAY
    INY #7
    LDA [$10],Y
    BPL .table
    LDA.B #(WW_FinaleShotOamLeft&$FF)
    STA.B $08
    LDA.B #((WW_FinaleShotOamLeft>>8)&$FF)
    STA.B $09
.table:
    LDA.L !WWMoveArg
    STA.B $04
    CLC
    ADC.B #$60
    STA.B $0C                   ; Low OAM base $0360 + slot*16 (OBJ 88+).
    LDA.B #$03
    STA.B $0D
    LDA.B $04
    LSR #2
    CLC
    ADC.B #$78
    STA.B $0E                   ; Expanded size $0478 + slot*4.
    LDA.B #$04
    STA.B $0F
    LDA.B #4
    STA.B $10
    JSR WW_DrawTiles
.next:
    LDA.L !WWMoveArg
    CLC
    ADC.B #16
    STA.L !WWMoveArg
    CMP.B #64
    BEQ .done
    JMP .loop
.done:
    RTS

WW_DrawBoss:
    LDA.L !WWBossActive
    BEQ .done
    JSR WW_SaveStripe
    JSR WW_DrawFinalePlayer
    JSR WW_DrawFinaleShots
    SEP #$30
    REP #$20
    LDA.L !WWBossOriginX
    SEP #$20
    STA.B $04
    STZ.B $05
    LDA.L !WWBossOriginY
    STA.B $06
    STZ.B $07
    REP #$20
    LDA.L !WWBossX
    SEC
    SBC.B $04
    SEC
    SBC.B !Layer1X
    STA.L !WWDrawX
    LDA.L !WWBossY
    SEC
    SBC.B $06
    SEC
    SBC.B !Layer1Y
    STA.L !WWDrawY
    SEP #$20
    LDA.B #(WW_BossOamRight&$FF)
    STA.B $08
    LDA.B #((WW_BossOamRight>>8)&$FF)
    STA.B $09
    LDA.B #bank(WW_BossOamRight)
    STA.B $0A
    REP #$20
    LDA.B !PlayerX
    SEC
    SBC.L !WWBossX
    SEP #$20
    BPL .table
    LDA.B #(WW_BossOamLeft&$FF)
    STA.B $08
    LDA.B #((WW_BossOamLeft>>8)&$FF)
    STA.B $09
.table:
    ; Slots 52-87 follow the player (0-35); shots use 88-103.
    ; Never clear or reuse the player's OAM entries.
    LDA.B #$D0
    STA.B $0C
    LDA.B #$02
    STA.B $0D
    LDA.B #$54
    STA.B $0E
    LDA.B #$04
    STA.B $0F
    LDA.B #36
    STA.B $10
    JSR WW_DrawTiles
    JSR WW_DrawHUD
    JSR WW_RestoreStripe
.done:
    RTS

; Small 8x8 OBJs map the complete 48x48 action canvas.  The previous
; 16x16 table used the small-OAM size bit and dropped three quarters of Akuma.
; Projectile art is uploaded at $6800, which is tile 128 in the native OBJ page.
WW_FinaleShotOamRight:
    db 0,0,128,$00, 8,0,129,$00, 0,8,130,$00, 8,8,131,$00
WW_FinaleShotOamLeft:
    db 0,0,129,$28, 8,0,128,$28, 0,8,131,$28, 8,8,130,$28

WW_BossOamRight:
    db 0,0,192,$02, 8,0,193,$02, 16,0,194,$02, 24,0,195,$02, 32,0,196,$02, 40,0,197,$02
    db 0,8,198,$02, 8,8,199,$02, 16,8,200,$02, 24,8,201,$02, 32,8,202,$02, 40,8,203,$02
    db 0,16,204,$02, 8,16,205,$02, 16,16,206,$02, 24,16,207,$02, 32,16,208,$02, 40,16,209,$02
    db 0,24,210,$02, 8,24,211,$02, 16,24,212,$02, 24,24,213,$02, 32,24,214,$02, 40,24,215,$02
    db 0,32,216,$02, 8,32,217,$02, 16,32,218,$02, 24,32,219,$02, 32,32,220,$02, 40,32,221,$02
    db 0,40,222,$02, 8,40,223,$02, 16,40,224,$02, 24,40,225,$02, 32,40,226,$02, 40,40,227,$02
WW_BossOamLeft:
    db 0,0,197,$42, 8,0,196,$42, 16,0,195,$42, 24,0,194,$42, 32,0,193,$42, 40,0,192,$42
    db 0,8,203,$42, 8,8,202,$42, 16,8,201,$42, 24,8,200,$42, 32,8,199,$42, 40,8,198,$42
    db 0,16,209,$42, 8,16,208,$42, 16,16,207,$42, 24,16,206,$42, 32,16,205,$42, 40,16,204,$42
    db 0,24,215,$42, 8,24,214,$42, 16,24,213,$42, 24,24,212,$42, 32,24,211,$42, 40,24,210,$42
    db 0,32,221,$42, 8,32,220,$42, 16,32,219,$42, 24,32,218,$42, 32,32,217,$42, 40,32,216,$42
    db 0,40,227,$42, 8,40,226,$42, 16,40,225,$42, 24,40,224,$42, 32,40,223,$42, 40,40,222,$42
