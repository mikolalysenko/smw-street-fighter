; Native SMW + Street Fighter audio integration, Asar 1.91 / 65c816.
; Run rom/make_audio.py before assembling. This file can be assembled standalone
; against the verified USA ROM, or incsrc'd after the other expanded-ROM code.

lorom

; The initial SPC image pointer is patched by world_warrior.asm.

; Keep audio independent of the current size of the combat/graphics expansion.
org $118000

WW_SPCImage:
    dw $0E3E, $0500                    ; stock engine, one CALL hook at ARAM $0549
    incbin "local/sf2/native-audio/spc_engine.bin"
    dw $0A6B, $5570                    ; stock sound-effect programs
    incbin "local/sf2/native-audio/spc_sfx.bin"
    dw WW_SPC700_End-WW_SPC700_Poll, $FC00
    incsrc "rom/spc700.asm"
    dw $161D, $1360                    ; stock overworld music, as the native image
    incbin "local/sf2/native-audio/spc_music1.bin"
    dw $0000, $0500

incsrc "rom/sound_ids.asm"

!WWAudioState      = $7F0E00
!WWAudioID         = $7F0E01
!WWAudioPtr        = $7F0E02             ; 24-bit ROM read pointer
!WWAudioRemaining  = $7F0E05             ; word, BRR blocks not uploaded
!WWAudioVolume     = $7F0E07
!WWAudioWork       = $7F0E08             ; prime/update budget
!WWAudioLastStatus = $7F0E09             ; last transport result, for diagnostics
!WWAudioNMISave    = $7F0E0A             ; $4200 during one bounded transaction
!WWAudioScratch    = $7F0E0B             ; saved $00-$02 during one block copy
!WWAudioNew        = $7F0E0F             ; request primed this frame; defer update

; Request/update return A: 0 = accepted/OK, 1 = invalid ID or ring full (not an
; error), 2 = transport timeout/protocol error. WW_AudioUpdate treats full as OK.
; All public routines preserve X/Y and incoming M/X/I flags. $00-$02 are saved
; and restored around each block copy; $4200 is saved/masked per transaction.

WW_AudioRequest:
    PHP
    REP #$10
    PHX
    PHY
    SEP #$20                           ; keep X/Y 16-bit for descriptor indexing
if defined("WW_PERFORMANCE")
    PHA
    JSL WW_PerfAudioStart
    PLA
endif
    CMP.B #!WW_SND_COUNT
    BCC +
    JMP .invalid
+   STA.L !WWAudioID
    REP #$20
    AND.W #$00FF                       ; zero-extend the 8-bit ID before 16-bit TAX
    ASL A
    ASL A
    ASL A                              ; eight-byte descriptors
    TAX
    SEP #$20
    LDA.L WW_AudioDescriptors,X
    STA.L !WWAudioPtr
    INX
    LDA.L WW_AudioDescriptors,X
    STA.L !WWAudioPtr+1
    INX
    LDA.L WW_AudioDescriptors,X
    STA.L !WWAudioPtr+2
    INX
    REP #$20
    LDA.L WW_AudioDescriptors,X       ; X is descriptor base + 3 here
    STA.L !WWAudioRemaining
    SEP #$20
    LDA.L WW_AudioDescriptors+2,X     ; volume at descriptor base + 5
    STA.L !WWAudioVolume
    LDA.B #$01
    STA.L !WWAudioState
    JSR WW_AudioBegin
    CMP.B #$00
    BNE .fault
    LDA.B #1                           ; validate transport; updates handle the stream
    STA.L !WWAudioWork
.prime:
    LDA.L !WWAudioRemaining
    ORA.L !WWAudioRemaining+1
    BEQ .accepted
    JSR WW_AudioSendBlock
    CMP.B #$00
    BEQ .primed
    CMP.B #$01
    BEQ .accepted                      ; unusual, but update can safely retry
    BRA .fault
.primed:
    LDA.L !WWAudioWork
    DEC A
    STA.L !WWAudioWork
    BNE .prime
.accepted:
    LDA.B #$01
    STA.L !WWAudioNew
    LDA.B #$00
    BRA .done
.invalid:
    LDA.B #$01
    BRA .done
.fault:
    LDA.B #$00
    STA.L !WWAudioState
    LDA.B #$02
.done:
    STA.L !WWAudioLastStatus
if defined("WW_PERFORMANCE")
    PHA
    JSL WW_PerfAudioEnd
    PLA
endif
    REP #$20
    AND.W #$00FF                       ; valid status for 8- or 16-bit callers
    REP #$10
    PLY
    PLX
    PLP
    RTL

; Call once per gameplay/main-loop frame, never from NMI. At most nine blocks
; (81 source bytes) are transferred; interrupts are restored between blocks.
WW_AudioUpdate:
    PHP
    REP #$10
    PHX
    PHY
    SEP #$30
if defined("WW_PERFORMANCE")
    JSL WW_PerfAudioStart
endif
    LDA.L !WWAudioNew
    BEQ .regular_update
    LDA.B #$00
    STA.L !WWAudioNew
    BRA .idle                         ; request already offered its initial block
.regular_update:
    LDA.L !WWAudioState
    BEQ .idle
    LDA.L !WWAudioRemaining
    ORA.L !WWAudioRemaining+1
    BEQ .idle
    LDA.L !WWMenu
    BNE .input_budget
    LDA.B !Held1
    ORA.B !Held2
    BNE .input_budget
    LDA.B #9                           ; idle: 18 ms of audio per 60 Hz update
    BRA .set_budget
.input_budget:
    LDA.B #1                           ; input stays responsive during button holds
.set_budget:
    STA.L !WWAudioWork
.more:
    JSR WW_AudioSendBlock
    CMP.B #$00
    BEQ .sent
    CMP.B #$01
    BEQ .idle                          ; ring full: normal flow control
    BRA .fault
.sent:
    LDA.L !WWAudioRemaining
    ORA.L !WWAudioRemaining+1
    BEQ .idle
    LDA.L !WWAudioWork
    DEC A
    STA.L !WWAudioWork
    BNE .more
.idle:
    LDA.B #$00
    BRA .done
.fault:
    LDA.B #$00
    STA.L !WWAudioState
    LDA.B #$02
.done:
    STA.L !WWAudioLastStatus
if defined("WW_PERFORMANCE")
    PHA
    JSL WW_PerfAudioEnd
    PLA
endif
    REP #$20
    AND.W #$00FF                       ; valid status for 8- or 16-bit callers
    REP #$10
    PLY
    PLX
    PLP
    RTL

; Wait for the SPC to release its reply. X/Y are preserved; A=0 released, 2 timeout.
; Every caller has already saved X/Y, but preserving X here also keeps retry counters.
WW_AudioWaitRelease:
    PHX
    LDX.B #$00
.wait:
    LDA.W $2143
    BEQ .released
    DEX
    BNE .wait
    LDA.B #$02
    BRA .done
.released:
    LDA.B #$00
.done:
    PLX
    RTS

; Complete the input half of a four-phase handshake and bound the output release.
; Enter with the desired return status in A; a release timeout overrides it with 2.
WW_AudioReleaseCommand:
    PHA
    STZ.W $2143
    JSR WW_AudioWaitRelease
    CMP.B #$00
    BNE .timeout
    PLA
    RTS
.timeout:
    PLA
    LDA.B #$02
    RTS

; Stop only fighter voice 4. Native music and stock SFX channels are untouched.
WW_AudioStop:
    PHP
    REP #$10
    PHX
    PHY
    SEP #$30
    SEI
    LDA.W $4200
    STA.L !WWAudioNMISave
    AND.B #$7F                         ; mask NMI only; keep automatic joypad reads
    STA.W $4200                        ; SEI does not mask NMI on the 65c816
    LDA.B #$00                         ; never resume a stream after a failed stop
    STA.L !WWAudioState
    STA.L !WWAudioRemaining
    STA.L !WWAudioRemaining+1
    STA.L !WWAudioNew
    LDA.B #$A8
    STA.W $2143
    LDX.B #$04
.retry:
    LDY.B #$00
.wait:
    LDA.W $2143
    CMP.B #$5A
    BEQ .ack
    DEY
    BNE .wait
    STZ.W $2143                        ; finish one poll window before retrying
    JSR WW_AudioWaitRelease
    CMP.B #$00
    BNE .done
    DEX
    BEQ .timeout
    LDA.B #$A8
    STA.W $2143
    BRA .retry
.timeout:
    LDA.B #$02
    BRA .done
.ack:
    LDA.B #$00
.done:
    JSR WW_AudioReleaseCommand
    STA.L !WWAudioLastStatus
    PHA
    LDA.L !WWAudioNMISave
    ORA.B #$81                         ; game/menu execution requires NMI and joypad reads
    STA.W $4200
    PLA
    REP #$20
    AND.W #$00FF                       ; valid status for 8- or 16-bit callers
    REP #$10
    PLY
    PLX
    PLP
    RTL

; Internal transaction: configure directory/voice and prime a new ring.
WW_AudioBegin:
    PHP
    REP #$10
    PHX
    PHY
    SEP #$30
    SEI
    LDA.W $4200
    STA.L !WWAudioNMISave
    AND.B #$7F                         ; mask NMI only; keep automatic joypad reads
    STA.W $4200
    LDA.L !WWAudioVolume
    STA.W $2140
    LDA.B #$A5
    STA.W $2143
    LDX.B #$10
.retry:
    LDY.B #$00
.wait:
    LDA.W $2143
    CMP.B #$5A
    BEQ .ack
    DEY
    BNE .wait
    STZ.W $2143                        ; finish one poll window before retrying
    JSR WW_AudioWaitRelease
    CMP.B #$00
    BNE .done
    DEX
    BEQ .timeout
    LDA.B #$A5
    STA.W $2143
    BRA .retry
.timeout:
    LDA.B #$02
    BRA .done
.ack:
    LDA.B #$00
.done:
    JSR WW_AudioReleaseCommand
    STZ.W $2140
    STZ.W $2141
    PHA
    LDA.L !WWAudioNMISave
    ORA.B #$81                         ; game/menu execution requires NMI and joypad reads
    STA.W $4200
    PLA
    REP #$10
    PLY
    PLX
    PLP
    RTS

; Internal transaction: send one 9-byte BRR block. A=0 sent, 1 full, 2 error.
WW_AudioSendBlock:
    PHP
    REP #$10
    PHX
    PHY
    SEP #$30
    LDA.B $00
    STA.L !WWAudioScratch
    LDA.B $01
    STA.L !WWAudioScratch+1
    LDA.B $02
    STA.L !WWAudioScratch+2
    LDA.L !WWAudioPtr
    STA.B $00
    LDA.L !WWAudioPtr+1
    STA.B $01
    LDA.L !WWAudioPtr+2
    STA.B $02
    SEI
    LDA.W $4200
    STA.L !WWAudioNMISave
    AND.B #$7F                         ; mask NMI only; keep automatic joypad reads
    STA.W $4200
    LDA.B #$A6
    STA.W $2143
    LDY.B #$00                         ; ring-full/busy is retried next frame, not spun
.ready_wait:
    LDA.W $2143
    CMP.B #$5B
    BEQ .ready
    CMP.B #$80
    BEQ .full
    CMP.B #$81
    BEQ .error
    DEY
    BNE .ready_wait
    BRA .full                          ; no ready reply: ring full/native busy, retry
.ready:
    STZ.W $2143                        ; release block-ready before offering byte zero
    JSR WW_AudioWaitRelease
    CMP.B #$00
    BNE .done
    LDY.B #$00
.byte:
    STY.W $2140
    LDA.B [$00]
    STA.W $2141
    INC.B $00                          ; full LoROM increment: $xxFFFF -> $xx+1:8000
    BNE +
    INC.B $01
    BNE +
    INC.B $02
    LDA.B #$80
    STA.B $01
+   LDA.B #$A7
    STA.W $2143
    LDX.B #$00
.byte_wait:
    TYA
    CLC
    ADC.B #$60
    CMP.W $2143                        ; $60+index, never a stale prior-byte ack
    BNE .byte_not_ack
    BRA .byte_ack
.byte_not_ack:
    LDA.W $2143
    CMP.B #$81
    BEQ .error
    DEX
    BNE .byte_wait
    BRA .error
.byte_ack:
    STZ.W $2143                        ; release every edge before offering another byte
    JSR WW_AudioWaitRelease
    CMP.B #$00
    BNE .done
    INY
    CPY.B #9
    BCC .byte

    LDA.B $00                          ; save the canonical pointer advanced above
    STA.L !WWAudioPtr
    LDA.B $01
    STA.L !WWAudioPtr+1
    LDA.B $02
    STA.L !WWAudioPtr+2
    REP #$20
    LDA.L !WWAudioRemaining
    DEC A
    STA.L !WWAudioRemaining
    SEP #$20
    LDA.B #$00
    BRA .done
.full:
    LDA.B #$01
    BRA .done
.error:
    LDA.B #$02
.done:
    JSR WW_AudioReleaseCommand
    PHA
    LDA.L !WWAudioScratch
    STA.B $00
    LDA.L !WWAudioScratch+1
    STA.B $01
    LDA.L !WWAudioScratch+2
    STA.B $02
    LDA.L !WWAudioNMISave
    ORA.B #$81                         ; game/menu execution requires NMI and joypad reads
    STA.W $4200
    PLA
    REP #$10
    PLY
    PLX
    PLP
    RTS

; Eight bytes each: 24-bit BRR address, 16-bit block count, volume, reserved.
WW_AudioDescriptors:
incsrc "local/sf2/native-audio/audio_descriptors.asm"

; Generated labels split incbin directives at LoROM bank borders without gaps.
org $6B0000
incsrc "local/sf2/native-audio/audio_data.asm"
