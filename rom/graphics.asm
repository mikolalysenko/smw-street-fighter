; Imported roster OAM renderer, HUD and bank-safe NMI graphics DMA.

WW_Draw:
    PHB
    SEP #$30
    LDA.B #$00
    PHA
    PLB
    JSR WW_SaveStripe
    LDA.W !GameMode
    CMP.B #$14
    BEQ .in_level
    JMP .fallback
.in_level:
    LDA.L !WWInitialized
    CMP.B #$5A
    BEQ .draw_initialized
    JMP .fallback
.draw_initialized:
    LDX.B #54
    LDA.B #$FC
.clear_status:
    STA.W !StatusBar,X
    DEX
    BPL .clear_status
    LDA.L !WWMenu
    BEQ .gameplay
    JSR WW_DrawMenu
    LDX.B #$00
    LDY.B #$00
    JSR WW_RestoreStripe
    PLB
    RTL
.gameplay:
    LDA.B !PlayerHiddenTiles
    CMP.B #$FF
    BEQ .shots
    LDA.W !IFrameTimer
    AND.B #$01
    BNE .shots

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
    BEQ .player_table
    LDA.B #(WW_RosterActionOamLeft&$FF)
    STA.B $08
    LDA.B #((WW_RosterActionOamLeft>>8)&$FF)
    STA.B $09
.player_table:
    LDA.B #$00
    STA.B $0C
    LDA.B #$03
    STA.B $0D
    LDA.B #$60
    STA.B $0E
    LDA.B #$04
    STA.B $0F
    LDA.B #36
    STA.B $10
    JSR WW_DrawTiles

.shots:
    JSR WW_SetShotPointer
    LDA.B #$00
    STA.L !WWMoveArg
.shot_loop:
    LDA.L !WWMoveArg
    TAY
    LDA [$10],Y
    BNE .shot_active
    JMP .shot_next
.shot_active:
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
    LDA.B #(WW_RosterProjectileOamRight&$FF)
    STA.B $08
    LDA.B #((WW_RosterProjectileOamRight>>8)&$FF)
    STA.B $09
    LDA.B #bank(WW_RosterProjectileOamRight)
    STA.B $0A
    LDA.L !WWMoveArg
    TAY
    INY #7
    LDA [$10],Y
    BPL .shot_table
    LDA.B #(WW_RosterProjectileOamLeft&$FF)
    STA.B $08
    LDA.B #((WW_RosterProjectileOamLeft>>8)&$FF)
    STA.B $09
.shot_table:
    LDA.L !WWMoveArg
    STA.B $04
    CLC
    ADC.B #$90
    STA.B $0C                   ; Low OAM base $0390 + slot*16.
    LDA.B #$03
    STA.B $0D
    LDA.B $04
    LSR #2
    CLC
    ADC.B #$84
    STA.B $0E                   ; Expanded size $0484 + slot*4.
    LDA.B #$04
    STA.B $0F
    LDA.B #4
    STA.B $10
    JSR WW_DrawTiles
.shot_next:
    LDA.L !WWMoveArg
    CLC
    ADC.B #16
    STA.L !WWMoveArg
    CMP.B #64
    BEQ .shots_done
    JMP .shot_loop
.shots_done:
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BNE .no_boss
    JSR WW_DrawBoss
.no_boss:
    JSR WW_DrawHUD
    JSR WW_RestoreStripe
    PLB
    RTL
.fallback:
    JSR WW_RestoreStripe
    LDA.B !PlayerHiddenTiles
    JML $00E2C2

; Draw 16x16 OBJ tuples. $08-$0A table, $0C low OAM base,
; $0E expanded-size base, $10 tuple count.
WW_DrawTiles:
    SEP #$30
    LDY.B #$00
    LDX.B #$00
.loop:
    LDA [$08],Y
    STA.B $04
    STZ.B $05
    INY
    LDA [$08],Y
    STA.B $06
    STZ.B $07
    INY
    LDA [$08],Y
    STA.B $11
    INY
    LDA [$08],Y
    STA.B $13
    INY
    STY.B $12
    REP #$20
    LDA.L !WWDrawX
    CLC
    ADC.B $04
    STA.B $00
    LDA.L !WWDrawY
    CLC
    ADC.B $06
    STA.B $02
    CMP.W #$00E0
    BCS .hide
    LDA.B $00
    CMP.W #$0100
    BCC .visible
    CMP.W #$FFE0
    BCS .visible
    BRA .hide
.visible:
    SEP #$20
    LDY.B $12
    DEY #4
    LDA.B $00
    STA ($0C),Y
    INY
    LDA.B $02
    STA ($0C),Y
    INY
    LDA.B $11
    STA ($0C),Y
    INY
    LDA.B $13
    ORA.B #$30
    STA ($0C),Y
    TXY
    LDA.B $01
    AND.B #$01                  ; Ninth X bit; small OBJs keep size bit clear.
    STA ($0E),Y
    BRA .next
.hide:
    SEP #$20
    LDY.B $12
    DEY #3
    LDA.B #$F0
    STA ($0C),Y
    TXY
    LDA.B #$00
    STA ($0E),Y
.next:
    LDY.B $12
    INX
    CPX.B $10
    BEQ .done
    JMP .loop
.done:
    RTS

WW_DrawHUD:
    SEP #$30
    ; Compare health*10 against vitality*(cell+1), avoiding division.
    REP #$20
    LDA.L !WWHealth
    AND.W #$00FF
    STA.B $04
    ASL A
    ASL A
    CLC
    ADC.B $04
    STA.B $04
    LDA.W #$00
    STA.B $06
    LDA.L !WWCharacter
    TAX
    LDA.L WW_RosterVitality,X
    AND.W #$00FF
    STA.B $08
    SEP #$20
    LDX.B #$00
    LDY.B #$00
.health:
    LDA.L WW_HealthX,X
    STA.W $03D0,Y               ; OBJ slots 116-124.
    LDA.B #8
    STA.W $03D1,Y
    REP #$20
    LDA.B $06
    CLC
    ADC.B $08
    STA.B $06
    LDA.B $04
    CMP.B $06
    SEP #$20
    LDA.B #161
    BCC .health_tile
    LDA.B #160
.health_tile:
    STA.W $03D2,Y
    LDA.B #$32                  ; OBJ palette 9, shared HUD colors.
    STA.W $03D3,Y
    LDA.B #$00
    STA.W $0494,X
    INY #4
    INX
    CPX.B #9
    BNE .health
    LDX.B #$00
    LDY.B #$00
    STX.B $0E
.meter:
    LDA.L WW_MeterX,X
    STA.W $03F4,Y               ; OBJ slots 125-127.
    LDA.B #8
    STA.W $03F5,Y
    TXA
    ASL A
    TAX
    REP #$20
    LDA.L !WWMeter
    CMP.L WW_MeterThreshold,X
    SEP #$20
    LDX.B $0E
    LDA.B #163
    BCC .meter_tile
    LDA.B #162
.meter_tile:
    STA.W $03F6,Y
    LDA.B #$32
    STA.W $03F7,Y
    LDA.B #$00
    STA.W $049D,X
    INY #4
    INX
    STX.B $0E
    CPX.B #3
    BNE .meter
    RTS

WW_HealthX:
    db 8,16,24,32,40,48,56,64,72,80
WW_MeterX:
    db 200,208,216
WW_MeterThreshold:
    dw 100,200,300

; DMA channel 2. $14-$16 is a source pointer and WWUploadBase a VRAM base.
macro WW_DMAWord(source, destination, size)
    REP #$20
    LDA.W #<destination>
    STA.W $2116
    LDA.W #<source>
    STA.W $4322
    LDY.B #bank(<source>)
    STY.W $4324
    LDA.W #<size>
    STA.W $4325
    LDX.B #$04
    STX.W $420B
endmacro

WW_UploadActionFrame:
    REP #$20
    LDA.L !WWUploadBase
    STA.W $2116
    LDA.B $F0
    STA.W $4322
    LDA.B $F2
    AND.W #$00FF
    STA.W $4324
    LDA.W #1152
    STA.W $4325
    LDX.B #$04
    STX.W $420B
    SEP #$20
    RTS

WW_UploadProjectileFrame:
    REP #$20
    LDA.W #$6800
    STA.W $2116
    LDA.B $F0
    STA.W $4322
    LDA.B $F2
    AND.W #$00FF
    STA.W $4324
    LDA.W #512
    STA.W $4325
    LDX.B #$04
    STX.W $420B
    SEP #$20
    RTS

WW_UploadPaletteLong:
    JSR WW_UploadPalette
    RTL

WW_UploadPalette:
    ; $17-$19 = source palette, $2121 has already been set.
    REP #$20
    LDA.W #$2200
    STA.W $4320
    LDA.B $F7
    STA.W $4322
    LDA.B $F9
    AND.W #$00FF
    STA.W $4324
    LDA.W #32
    STA.W $4325
    LDX.B #$04
    STX.W $420B
    LDA.W #$1801
    STA.W $4320
    RTS

WW_DMA:
    SEP #$10
    LDA.W !GameMode
    CMP.B #$14
    BEQ .in_level
    JMP .fallback
.in_level:
    LDA.L !WWInitialized
    CMP.B #$5A
    BEQ .initialized
    JMP .fallback
.initialized:
    LDA.L !WWAudioBootDelay
    BEQ .native_upload_done
    JMP .fallback                 ; native music upload must not share this NMI
.native_upload_done:
    REP #$20
    LDY.B #$80
    STY.W $2115
    LDA.W #$1801
    STA.W $4320
    LDX.B #$04
    LDA.L !WWMenu
    AND.W #$00FF
    BNE .selection
    JMP .gameplay
.selection:
    LDA.L !WWMenu
    AND.W #$00FF
    CMP.W #!MenuEnding
    BNE .roster_menu
    JML WW_UploadEnding
.roster_menu:
    JMP WW_UploadSelection
.gameplay:
    ; Action frame pointer is resolved outside NMI.
    LDA.L !WWLastGfxPointer
    CMP.L !WWGfxPointer
    BNE .action
    LDA.L !WWLastGfxPointer+1
    CMP.L !WWGfxPointer+1
    BNE .action
    LDA.L !WWLastGfxPointer+2
    CMP.L !WWGfxPointer+2
    BEQ .palette
.action:
    LDA.L !WWGfxPointer
    STA.B $F0
    LDA.L !WWGfxPointer+1
    STA.B $F1
    LDA.L !WWGfxPointer+2
    STA.B $F2
    LDA.W #$6000
    STA.L !WWUploadBase
    JSR WW_UploadActionFrame
    LDA.L !WWGfxPointer
    STA.L !WWLastGfxPointer
    LDA.L !WWGfxPointer+1
    STA.L !WWLastGfxPointer+1
    LDA.L !WWGfxPointer+2
    STA.L !WWLastGfxPointer+2
.palette:
    LDA.L !WWLastPalette
    CMP.L !WWCharacter
    BEQ .shot
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
    LDA.L !WWCharacter
    STA.L !WWLastPalette
.shot:
    LDA.L !WWLastShotPointer
    CMP.L !WWShotGfxPointer
    BNE .shot_upload
    LDA.L !WWLastShotPointer+1
    CMP.L !WWShotGfxPointer+1
    BNE .shot_upload
    LDA.L !WWLastShotPointer+2
    CMP.L !WWShotGfxPointer+2
    BEQ .hud
.shot_upload:
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
.hud:
    SEP #$20
    LDA.L !WWMenu
    CMP.B #!MenuFinale
    BNE .no_boss_upload
    JSR WW_UploadBossFrame
.no_boss_upload:
    SEP #$20
    LDA.L !WWUploadSet
    AND.B #$01
    BNE .done
    LDY.B #$90
    STY.W $2121
    LDA.B #(WW_Palette&$FF)
    STA.B $F7
    LDA.B #((WW_Palette>>8)&$FF)
    STA.B $F8
    LDA.B #bank(WW_Palette)
    STA.B $F9
    JSR WW_UploadPalette
    %WW_DMAWord(WW_HUDGraphics, $7400, 128)
    SEP #$20
    LDA.L !WWUploadSet
    ORA.B #$01
    STA.L !WWUploadSet
.done:
    REP #$20
    LDX.B #$0A
    LDY.B #$04
    LDA.W #$0040
    JML $00A38D
.fallback:
    REP #$20
    LDX.B #$04
    LDY.W $0D84
    JML $00A306

; Compact selection art stays inside the native OBJ page.
WW_UploadSelection:
    SEP #$20
    LDA.L !WWUploadSet
    AND.B #$02
    BNE .done
    LDA.B #$00
    STA.L !WWUploadChar
.palette_loop:
    LDA.L !WWUploadChar
    ASL A
    ADC.L !WWUploadChar
    TAX
    LDA.L WW_RosterPalettePointers,X
    STA.B $F7
    LDA.L WW_RosterPalettePointers+1,X
    STA.B $F8
    LDA.L WW_RosterPalettePointers+2,X
    STA.B $F9
    LDA.L !WWUploadChar
    ASL #4
    ORA.B #$80
    STA.W $2121
    JSR WW_UploadPalette
    SEP #$20
    LDA.L !WWUploadChar
    INC A
    STA.L !WWUploadChar
    CMP.B #8
    BNE .palette_loop
    %WW_DMAWord(WW_SelectGraphics, $6000, 1536)
    SEP #$20
    LDA.L !WWUploadSet
    ORA.B #$02
    STA.L !WWUploadSet
.done:
    REP #$20
    LDX.B #$0A
    LDY.B #$04
    LDA.W #$0040
    JML $00A38D

WW_SelectGraphics: incbin "rom/generated/roster_select.chr"
WW_HUDGraphics: incbin "rom/generated/hud.chr"
WW_Palette: incbin "rom/generated/palette.bin"
