; Full-screen native ending pages and their one-row-per-NMI uploader.

WW_UploadEnding:
    SEP #$20
    LDA.L !WWUploadSet
    AND.B #$04
    BNE .display
    LDA.L !WWUploadSet
    AND.B #$08
    BNE .row
    ; First ending frame: palette, row zero, and BG setup.
    LDA.L !WWCharacter
    ASL A
    ADC.L !WWCharacter
    TAX
    LDA.L WW_EndingPalettePointers,X
    STA.B $F7
    LDA.L WW_EndingPalettePointers+1,X
    STA.B $F8
    LDA.L WW_EndingPalettePointers+2,X
    STA.B $F9
    LDY.B #$00
    STY.W $2121
    JSL WW_UploadPaletteLong
    SEP #$20
    LDA.L !WWUploadSet
    ORA.B #$08
    STA.L !WWUploadSet
    LDA.B #$00
    STA.L !WWUploadChar
    BRA .row
.display:
    LDA.B #19
    STA.L !WWUploadChar
    BRA .upload                   ; native animation may otherwise reclaim VMADD $5600
.row:
    LDA.L !WWUploadChar
    CMP.B #28
    BCC .upload
    %WW_DMAWord(WW_EndingTilemap, $7800, 1792)
    SEP #$20
    LDA.L !WWUploadSet
    ORA.B #$04
    STA.L !WWUploadSet
    JMP .registers
.upload:
    ; Page base plus row*1024, in 24-bit address arithmetic.
    LDA.L !WWCharacter
    ASL A
    ADC.L !WWCharacter
    TAX
    LDA.L WW_EndingPagePointers,X
    STA.B $F0
    LDA.L WW_EndingPagePointers+1,X
    STA.B $F1
    LDA.L WW_EndingPagePointers+2,X
    STA.B $F2
    LDA.B $F1
    CMP.B #$80
    BCS .canonical_base
    LDA.B $F1
    CLC
    ADC.B #$80                  ; normalize raw label arithmetic below $8000
    STA.B $F1
.canonical_base:
    LDA.L !WWUploadChar
    STA.B $04
    REP #$20
    LDA.B $04
    ASL #10                     ; source row*1024 bytes.
    STA.B $06
    LDA.B $F0
    CLC
    ADC.B $06
    STA.B $F0
    BCC .no_bank_cross
    LDA.B $F2
    AND.W #$00FF
    INC A
    STA.B $F2
    LDA.B $F0
    CLC
    ADC.W #$8000                ; next LoROM bank starts at $8000
    STA.B $F0
.no_bank_cross:
    LSR.B $06                   ; VRAM destination row*512 words.
    LDA.B $06
    CLC
    ADC.W #$4000                ; ending characters avoid native low-VRAM animation
    STA.W $2116
    LDA.B $F0
    STA.W $4322
    LDA.B $F2
    AND.W #$00FF
    STA.W $4324
    LDA.W #1024
    STA.W $4325
    LDX.B #$04
    STX.W $420B
    SEP #$20
    LDA.L !WWUploadChar
    INC A
    STA.L !WWUploadChar
.registers:
    STZ.W $0D7C                 ; suppress native Gfx33/message tile DMA
    STZ.W $0D7E
    STZ.W $0D80
    STZ.W $143A
    STZ.W $1935
    STZ.W $1BE4                 ; suppress native Map16 strip uploads
    STZ.W $1CE6
    STZ.W $0012                 ; and StripeImage tilemap uploads
    LDA.B #$02
    STA.W $0D9B                 ; bypass the native status-bar NMI path
    LDA.B #$10
    STA.W $13D9
    LDA.B #$06
    STA.W $1DE8                 ; and overworld animated-tile uploads
    LDA.B #$01
    STA.W $2105                 ; Mode 1.
    LDA.B #$78
    STA.W $2107                 ; BG1 tilemap at $7800.
    LDA.B #$04
    STA.W $210B                 ; BG1 characters at $4000.
    STZ.W $210D
    LDA.B #$01
    STA.W $212C                 ; BG1 only.
    STZ.W $212D
    LDA.B #$0F
    STA.W $2100
    REP #$20
    LDX.B #$0A
    LDY.B #$04
    LDA.W #$0040
    JML $00A38D

WW_EndingPagePointers:
incsrc "rom/generated/ending_page_pointers.asm"
WW_EndingPalettePointers:
    dl WW_EndingPalettes+$00,WW_EndingPalettes+$20,WW_EndingPalettes+$40
    dl WW_EndingPalettes+$60,WW_EndingPalettes+$80,WW_EndingPalettes+$A0
    dl WW_EndingPalettes+$C0,WW_EndingPalettes+$E0,WW_EndingPalettes+$100
    dl WW_EndingPalettes+$120,WW_EndingPalettes+$140,WW_EndingPalettes+$160

WW_EndingTilemap: incbin "rom/generated/ending_tilemap.bin"
WW_EndingPalettes: incbin "rom/generated/ending_pages.pal"
org $608000
WW_EndingPages_000: incbin "rom/generated/ending_pages.chr":$00000..$08000
org $618000
WW_EndingPages_001: incbin "rom/generated/ending_pages.chr":$08000..$10000
org $628000
WW_EndingPages_002: incbin "rom/generated/ending_pages.chr":$10000..$18000
org $638000
WW_EndingPages_003: incbin "rom/generated/ending_pages.chr":$18000..$20000
org $648000
WW_EndingPages_004: incbin "rom/generated/ending_pages.chr":$20000..$28000
org $658000
WW_EndingPages_005: incbin "rom/generated/ending_pages.chr":$28000..$30000
org $668000
WW_EndingPages_006: incbin "rom/generated/ending_pages.chr":$30000..$38000
org $678000
WW_EndingPages_007: incbin "rom/generated/ending_pages.chr":$38000..$40000
org $688000
WW_EndingPages_008: incbin "rom/generated/ending_pages.chr":$40000..$48000
org $698000
WW_EndingPages_009: incbin "rom/generated/ending_pages.chr":$48000..$50000
org $6A8000
WW_EndingPages_010: incbin "rom/generated/ending_pages.chr":$50000..$54000
