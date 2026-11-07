; Native full-screen title/roster presentation and incremental BG uploads.
incsrc "rom/generated/menu_page_pointers.asm"

WW_MenuLateNMI:
    SEP #$30
    LDA.L !WWMenu
    CMP.B #!MenuTitle
    BEQ .custom
    CMP.B #!MenuSelect
    BEQ .custom
    LDA.B $1A
    STA.W $210D
    JML $00824B
.custom:
    LDA.B #$01
    STA.W $2105
    LDA.B #$78
    STA.W $2107
    LDA.B #$01
    STA.W $210B
    STZ.W $210D
    STZ.W $210D
    STZ.W $210E
    STZ.W $210E
    STZ.W $1A
    STZ.W $1C
    STZ.W $1E
    STZ.W $20
    STZ.W $2131
    LDA.B #$01
    STA.W $212C
    STZ.W $212D
    STZ.W $420C
    LDA.B #$0F
    STA.W $2100
    JML $00824B

WW_MenuStripe:
    SEP #$30
    LDA.L !WWMenu
    CMP.B #!MenuTitle
    BEQ .skip
    CMP.B #!MenuSelect
    BEQ .skip
    LDY.B $12
    LDA.W $84D0,Y
    STA.B $00
    LDA.W $84D1,Y
    STA.B $01
    JML $0085DC
.skip:
    STZ.B $12
    JML $009F6E

WW_SaveNativeScreen:
    SEP #$30
    LDA.W $0D9B
    STA.L !WWMenuNativeNMI
    LDA.W $0D9F
    STA.L !WWMenuNativeHDMA
    LDA.B #$00
    STA.L !WWMenuGfxState
    STA.L !WWMenuGfxRow
    LDA.B #$FF
    STA.L !WWMenuGfxPage
    LDA.B #$80
    STA.W $2100                   ; blank while the first tile rows upload
    RTS

WW_RestoreNativeScreen:
    SEP #$30
    LDA.B #$09                    ; verified native level PPU configuration
    STA.W $2105
    LDA.B #$03
    STA.W $2106
    LDA.B #$23
    STA.W $2107
    LDA.B #$33
    STA.W $2108
    LDA.B #$53
    STA.W $2109
    STZ.W $210A
    STZ.W $210B
    LDA.B #$04
    STA.W $210C
    LDA.B #$13                    ; custom HUD replaces native BG3/status art
    STA.W $212C
    STA.W $212E
    LDA.B #$02
    STA.W $212D
    STA.W $212F
    STA.W $2130
    LDA.B #$20
    STA.W $2131
    LDA.L !WWMenuNativeNMI
    STA.W $0D9B
    LDA.L !WWMenuNativeHDMA
    STA.W $420C
    LDA.B #$0F
    STA.W $2100
    LDA.B #$00
    STA.L !WWMenuGfxState
    RTS

WW_UploadMenu:
    SEP #$30
    LDA.L !WWMenu
    CMP.B #!MenuTitle
    BNE .selection_page
    LDA.B #$00
    BRA .desired_page
.selection_page:
    LDA.L !WWChoice
    ASL A
    INC A
    STA.B $04
    LDA.L !WWMenuAnim
    CMP.B #20
    BCC .desired_page
    INC.B $04
.desired_page:
    LDA.B $04
    STA.B $04
    LDA.L !WWMenuGfxPage
    CMP.B $04
    BEQ .state
    LDA.B $04
    STA.L !WWMenuGfxPage
    LDA.L !WWMenuGfxState
    CMP.B #$02
    BCC .state                    ; finish the shared tileset before the new map
    LDA.B #$04
    STA.L !WWMenuGfxState         ; palette+tilemap only on an already-loaded set
.state:
    LDA.L !WWMenuGfxState
    BEQ .begin
    DEC A
    BEQ .tiles
    DEC A
    BNE +
    JMP .tilemap
+   DEC A
    BNE +
    JMP .registers
+   JMP .palette
.begin:
    LDA.B #$80
    STA.W $2100
    LDA.B #$00
    STA.L !WWMenuGfxRow
    LDA.B #$01
    STA.L !WWMenuGfxState
    JMP .finish
.tiles:
    LDA.L !WWMenuGfxRow
    CMP.B #!WWMenuTileRows
    BCC .tile_row
    LDA.B #$04
    STA.L !WWMenuGfxState
    JMP .palette
.tile_row:
    STA.B $04
    REP #$20
    LDA.B $04
    ASL #10
    STA.B $06
    LDA.W #WW_MenuTiles
    CLC
    ADC.B $06
    STA.W $4322
    LDY.B #bank(WW_MenuTiles)
    STY.W $4324
    LDA.B $06
    LSR A
    CLC
    ADC.W #$1000
    STA.W $2116
    LDA.W #1024
    STA.W $4325
    LDX.B #$04
    STX.W $420B
    SEP #$20
    LDA.L !WWMenuGfxRow
    INC A
    STA.L !WWMenuGfxRow
    JMP .finish
.palette:
    LDA.L !WWMenuGfxPage
    ASL A
    ADC.L !WWMenuGfxPage
    TAX
    LDA.L WW_MenuPalettePointers,X
    STA.B $F7
    LDA.L WW_MenuPalettePointers+1,X
    STA.B $F8
    LDA.L WW_MenuPalettePointers+2,X
    STA.B $F9
    LDY.B #$00
    STY.W $2121
    JSL WW_UploadPaletteLong
    SEP #$20
    LDA.B #$02
    STA.L !WWMenuGfxState
    JMP .finish
.tilemap:
    LDA.L !WWMenuGfxPage
    ASL A
    ADC.L !WWMenuGfxPage
    TAX
    LDA.L WW_MenuTilemapPointers,X
    STA.B $F0
    LDA.L WW_MenuTilemapPointers+1,X
    STA.B $F1
    LDA.L WW_MenuTilemapPointers+2,X
    STA.B $F2
    REP #$20
    LDA.W #$7800
    STA.W $2116
    LDA.B $F0
    STA.W $4322
    LDA.B $F2
    AND.W #$00FF
    STA.W $4324
    LDA.W #1792
    STA.W $4325
    LDX.B #$04
    STX.W $420B
    SEP #$20
    LDA.B #$03
    STA.L !WWMenuGfxState
.registers:
    STZ.W $0D7C
    STZ.W $0D7E
    STZ.W $0D80
    STZ.W $143A
    STZ.W $1935
    STZ.W $1BE4
    STZ.W $1CE6
    STZ.W $0012
    LDA.B #$02
    STA.W $0D9B
    LDA.B #$10
    STA.W $13D9
    LDA.B #$06
    STA.W $1DE8
    LDA.B #$01
    STA.W $2105
    LDA.B #$78
    STA.W $2107
    LDA.B #$01
    STA.W $210B
    STZ.W $210D
    STZ.W $210D
    STZ.W $210E
    STZ.W $210E
    STZ.W $1A
    STZ.W $1C
    STZ.W $1E
    STZ.W $20
    STZ.W $2131
    LDA.B #$01
    STA.W $212C
    STZ.W $212D
    STZ.W $420C
    LDA.B #$0F
    STA.W $2100
.finish:
    REP #$20
    LDX.B #$0A
    LDY.B #$04
    LDA.W #$0040
    JML $00A38D

; Generated pointers are followed by the shared art and page maps.
pushpc
org $5F8000
WW_MenuTiles: incbin "rom/generated/menu_tiles.chr"
org $7A8000
WW_MenuTilemaps: incbin "rom/generated/menu_tilemaps.bin":$0000..$8000
org $7B8000
incbin "rom/generated/menu_tilemaps.bin":$8000..$B100
WW_MenuPalettes: incbin "rom/generated/menu_pages.pal"
pullpc
