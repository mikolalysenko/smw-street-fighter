; Compact imported roster selection: 12 portraits x 4 small OBJs.

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
    LDA.L !WWMenu
    CMP.B #!MenuSelect
    BEQ .select
    JMP .done
.select:
    LDA.B #$00
    STA.B $04                   ; Character.
    STA.B $06                   ; OBJ slot.
    LDY.B #$00
.character:
    LDA.B #$00
    STA.B $05                   ; Tile within portrait.
.tiles:
    LDA.B $04
    AND.B #$03
    ASL #6
    STA.B $0C
    LDA.B $04
    LSR #2
    ASL #6
    CLC
    ADC.B #16
    STA.B $0E
    LDA.B $05
    AND.B #$01
    BEQ .left
    LDA.B #$08
    BRA .add_x
.left:
    LDA.B #$00
.add_x:
    CLC
    ADC.B $0C
    STA.W $0200,Y
    LDA.B $05
    CMP.B #2
    BCS .bottom
    LDA.B $0E
    BRA .store_y
.bottom:
    LDA.B $0E
    CLC
    ADC.B #8
.store_y:
    STA.W $0201,Y
    LDA.B $04
    ASL #2
    CLC
    ADC.B $05
    STA.W $0202,Y
    LDA.B $04
    CMP.B #8
    BCC .palette
    LDA.B #7
.palette:
    ASL A
    ORA.B #$30
    STA.W $0203,Y
    LDX.B $06
    LDA.B #$00
    STA.W $0420,X
    INC.B $06
    INC.B $05
    INY #4
    LDA.B $05
    CMP.B #4
    BNE .tiles
    INC.B $04
    LDA.B $04
    CMP.B #12
    BEQ .highlight
    JMP .character
.highlight:
    LDA.L !WWChoice
    AND.B #$03
    ASL #6
    SEC
    SBC.B #4
    STA.B $0C
    LDA.L !WWChoice
    LSR #2
    ASL #6
    CLC
    ADC.B #12
    STA.B $0E
    LDA.B #$00
    STA.B $05
.markers:
    LDA.B $05
    AND.B #$01
    BEQ .marker_left
    LDA.B $0C
    CLC
    ADC.B #20
    BRA .marker_x
.marker_left:
    LDA.B $0C
.marker_x:
    STA.W $0200,Y
    LDA.B $05
    CMP.B #2
    BCS .marker_bottom
    LDA.B $0E
    BRA .marker_y
.marker_bottom:
    LDA.B $0E
    CLC
    ADC.B #20
.marker_y:
    STA.W $0201,Y
    LDA.B #162
    STA.W $0202,Y
    LDA.B #$32
    STA.W $0203,Y
    LDX.B $06
    LDA.B #$00
    STA.W $0420,X
    INC.B $06
    INC.B $05
    INY #4
    LDA.B $05
    CMP.B #4
    BNE .markers
.done:
    RTS
