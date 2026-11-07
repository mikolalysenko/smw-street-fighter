WW_SaveInput:
    LDA.B $F1
    STA.L !WWInputBackup
    LDA.B $F2
    STA.L !WWInputBackup+1
    LDA.B $F7
    STA.L !WWInputBackup+2
    LDA.B $F8
    STA.L !WWInputBackup+3
    RTS
WW_RestoreInput:
    LDA.L !WWInputBackup
    STA.B $F1
    LDA.L !WWInputBackup+1
    STA.B $F2
    LDA.L !WWInputBackup+2
    STA.B $F7
    LDA.L !WWInputBackup+3
    STA.B $F8
    RTS

; Resolve imported animation descriptors into current graphics pointers/origins.

; A = FC_POSE_* animation ID. Returns $08-$0A = pose table and
; $0B-$0D = 13-byte animation descriptor, or zero in $0B.
WW_GetDescriptor:
    STA.B $04
    LDA.L !WWResolveCharacter
    ASL A
    ADC.L !WWResolveCharacter
    TAX
    LDA.L WW_RosterPoseTablePointers,X
    STA.B $08
    LDA.L WW_RosterPoseTablePointers+1,X
    STA.B $09
    LDA.L WW_RosterPoseTablePointers+2,X
    STA.B $0A
    LDA.B $04
    ASL A
    ADC.B $04
    TAY
    LDA [$08],Y
    STA.B $0B
    INY
    LDA [$08],Y
    STA.B $0C
    INY
    LDA [$08],Y
    STA.B $0D
    LDA.B $0B
    ORA.B $0C
    ORA.B $0D
    RTS

; Descriptor in $0B. Tick in $04. Resolves a frame into $14-$16 and
; origin bytes $17/$18. The descriptor format is documented in GRAPHICS.md.
WW_ResolveFrame:
    LDY.B #$04
    LDA [$0B],Y
    STA.B $0E
    INY
    LDA [$0B],Y
    STA.B $0F
    INY
    LDA [$0B],Y
    STA.B $10
    LDY.B #$07
    LDA [$0B],Y
    STA.B $11
    INY
    LDA [$0B],Y
    STA.B $12
    INY
    LDA [$0B],Y
    STA.B $13
    LDA.B #$00
    STA.B $05                   ; Frame index.
    STA.B $06                   ; Accumulated duration.
.loop:
    LDA [$0B]
    CMP.B $05
    BEQ .last
    ; Metadata records are 16 bytes; compute the current record in $19.
    LDA.B $05
    ASL #4
    TAY
    LDA [$11],Y
    STA.B $07
    LDA.B $04
    CMP.B $07
    BCC .chosen
    SEC
    SBC.B $07
    STA.B $04
    INC.B $05
    BRA .loop
.last:
    LDA.B $05
    DEC A
    STA.B $05
.chosen:
    LDA.B $05
    ASL A
    ADC.B $05
    TAY
    LDA [$0E],Y
    STA.B $F0
    INY
    LDA [$0E],Y
    STA.B $F1
    INY
    LDA [$0E],Y
    STA.B $F2
    LDA.B $05
    ASL #4
    TAY
    INY #4
    LDA [$11],Y
    STA.B $F7
    INY
    LDA [$11],Y
    STA.B $F8
    RTS

WW_ResolveAction:
    JSR WW_SaveStripe
    LDA.L !WWCharacter
    STA.L !WWResolveCharacter
    LDA.L !WWPose
    TAX
    LDA.L WW_NativePoseMap,X
    JSR WW_GetDescriptor
    BEQ .fallback
    BRA .resolve
.fallback:
    LDA.B #$00
    JSR WW_GetDescriptor
.resolve:
    LDA.L !WWMove
    BEQ .global_tick
    LDA.L !WWMoveTimer
    BRA .tick
.global_tick:
    LDA.L !WWFrameCounter
.tick:
    STA.B $04
    JSR WW_ResolveFrame
    LDA.B $F0
    STA.L !WWGfxPointer
    LDA.B $F1
    STA.L !WWGfxPointer+1
    LDA.B $F2
    STA.L !WWGfxPointer+2
    LDA.B $F7
    STA.L !WWGfxOriginX
    LDA.B $F8
    STA.L !WWGfxOriginY
    JSR WW_RestoreStripe
    RTS

WW_ResolveShot:
    JSR WW_SaveStripe
    JSR WW_SetShotPointer
    LDY.B #$00
.find:
    LDA [$10],Y
    BNE .found
    INY #16
    CPY.B #64
    BNE .find
    JSR WW_RestoreStripe
    RTS
.found:
    LDA.L !WWCharacter
    STA.L !WWResolveCharacter
    LDA.B #14                   ; FC_POSE_SHOT.
    JSR WW_GetDescriptor
    BEQ .done
    LDA.L !WWFrameCounter
    STA.B $04
    JSR WW_ResolveFrame
    LDA.B $F0
    STA.L !WWShotGfxPointer
    LDA.B $F1
    STA.L !WWShotGfxPointer+1
    LDA.B $F2
    STA.L !WWShotGfxPointer+2
    LDA.B $F7
    STA.L !WWShotOriginX
    LDA.B $F8
    STA.L !WWShotOriginY
.done:
    JSR WW_RestoreStripe
    RTS

WW_NativePoseMap:
    db 0,1,2,4,5,6,7,8,13,3,9,10,37,38,10,10,39,11,20,21,22

; A = character. Resolve the first idle frame into $14-$16 and its palette
; into $17-$19. Used by the roster's one-character-per-NMI selection upload.
WW_GetIdleGraphics:
    STA.L !WWResolveCharacter
    LDA.B #$00
    JSR WW_GetDescriptor
    LDY.B #$04
    LDA [$0B],Y
    STA.B $0E
    INY
    LDA [$0B],Y
    STA.B $0F
    INY
    LDA [$0B],Y
    STA.B $10
    LDY.B #$00
    LDA [$0E],Y
    STA.B $F0
    INY
    LDA [$0E],Y
    STA.B $F1
    INY
    LDA [$0E],Y
    STA.B $F2
    LDY.B #$0A
    LDA [$0B],Y
    STA.B $F7
    INY
    LDA [$0B],Y
    STA.B $F8
    INY
    LDA [$0B],Y
    STA.B $F9
    RTS
