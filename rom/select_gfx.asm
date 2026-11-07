; Custom menus are full-screen BG pages; keep every native OBJ off screen.
WW_DrawMenu:
    SEP #$30
    LDX.B #$00
    LDY.B #$00
    STZ.B $06
.clear:
    LDA.B #$F0
    STA.W $0201,X
    LDA.B #$00
    STA.W $0420,Y
    INX #4
    INY
    INC.B $06
    LDA.B $06
    CMP.B #128
    BNE .clear
    RTS
