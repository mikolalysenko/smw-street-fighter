; World Warrior native-SPC streaming extension.
; Asar syntax, SPC700 architecture. Included by audio.asm inside an upload block.
;
; The stock SMW main-loop instruction at ARAM $0549 (MOV Y,$00FD) is replaced
; by CALL $FC00. WW_SPC700_Poll returns the same counter value in Y, so native
; tempo and all native SFX sequencers retain their normal timing.

!WW_SPC_Ring       = $F400
!WW_SPC_RingBlocks = 227                 ; floor(2048 / 9), five padding bytes
!WW_SPC_RingLast   = !WW_SPC_Ring+(!WW_SPC_RingBlocks-1)*9
!WW_SPC_RingAfter  = !WW_SPC_Ring+!WW_SPC_RingBlocks*9
!WW_SPC_Prime      = 64                  ; 128 ms before initial key-on
!WW_Final          = $E6                 ; source end block has been uploaded

; Direct page $E8-$EF is unused by the stock engine (which clears through $E7).
!WW_State          = $E8                 ; 0 idle, 1 accepting blocks, 2 receiving
!WW_Write          = $E9                 ; word, next ring byte
!WW_Free           = $EB                 ; blocks safe for the CPU to overwrite
!WW_Queued         = $EC                 ; uploaded but not consumed by the 2 ms clock
!WW_Playing        = $ED
!WW_Expected       = $EE                 ; byte index within the current BRR block

arch spc700
base $FC00

WW_SPC700_Poll:
    JMP WW_SPC700_PollMain
WW_SPC700_Native3:
    JMP WW_SPC700_Native3Main
WW_SPC700_Native0:
    JMP WW_SPC700_Native0Main
WW_SPC700_Native1:
    JMP WW_SPC700_Native1Main

; Fixed $FC03 relay for the native CALL at $056B. Custom commands stay in
; SPCInEdge+3 until the next poll; do not let the native $1DFC sequencer eat them.
WW_SPC700_Native3Main:
    MOV A,$0003                        ; SPCInEdge+3
    CMP A,#$A5
    BEQ .done
    CMP A,#$A6
    BEQ .done
    CMP A,#$A7
    BEQ .done
    CMP A,#$A8
    BEQ .done
    JMP $0816                          ; original native $1DFC handler
.done:
    RET

WW_SPC700_Native0Main:
    JMP $06AE                          ; native SFX and music-upload protocol

WW_SPC700_Native1Main:
    JMP $09E5                          ; native SFX and music-upload protocol

WW_SPC700_PollMain:
    ; This read replaces the hooked native MOV Y,$00FD instruction.
    MOV Y,$00FD
    PUSH Y                             ; shared direct page is used by native uploads
    PUSH X                             ; bounded waits must not clobber the native loop
    BEQ .poll_command

    ; At 8 kHz, one 16-sample BRR block takes exactly one native timer tick.
    ; Credit is returned only after consumption, preventing ring overwrite.
    MOV A,!WW_Playing
    BEQ .poll_command
.tick:
    MOV A,!WW_Queued
    BNE .consume
    MOV !WW_State,#$00                 ; underrun: never repeat stale ring data
    MOV !WW_Playing,#$00
    CLR4 $1D                           ; release native ChannelsMuted voice 4
    MOV A,#$10
    MOV Y,#$5C
    CALL WW_SPC700_WriteDSP
    BRA .poll_command
.consume:
    DEC !WW_Queued
    INC !WW_Free
    DEC Y
    BNE .tick

.poll_command:
    MOV A,$0003                        ; CPU input edge, retained by the native relay
    BNE .custom_command
    JMP WW_SPC700_Return
.custom_command:
    CMP A,#$A5
    BNE +
    JMP WW_SPC700_Begin
+   CMP A,#$A6
    BNE +
    JMP WW_SPC700_Block
+   CMP A,#$A7
    BNE +
    JMP WW_SPC700_Block_not_next_block
+   CMP A,#$A8
    BNE +
    JMP WW_SPC700_Stop
+   JMP WW_SPC700_Return

WW_SPC700_Begin:
    ; APU0 volume, APU3 $A5. Pitch is fixed; native APU2 is never touched.
    MOV A,#$00
    MOV $0000,A                        ; clear stale native edge/input for $1DF9
    MOV $0008,A
    MOV $0004,A                        ; retire any previous native $1DF9 phrase
    MOV $0382,A
    MOV A,#$10
    MOV Y,#$5C                         ; KOF voice 4 before replacing its stream
    CALL WW_SPC700_WriteDSP
    SET4 $1D                           ; music sequences, but cannot write voice 4 DSP

    MOV A,#$00
    MOV $8050,A                        ; sample directory entry 20: start
    MOV $8052,A                        ; loop used only for physical ring wrap
    MOV A,#$F4
    MOV $8051,A
    MOV $8053,A

    MOV A,$00F4
    MOV Y,#$40                         ; VOLL
    CALL WW_SPC700_WriteDSP
    MOV Y,#$41                         ; VOLR, preserve centered mono
    CALL WW_SPC700_WriteDSP
    MOV A,#$00
    MOV Y,#$42                         ; PITCHL ($0400 = 8 kHz)
    CALL WW_SPC700_WriteDSP
    MOV A,#$04
    MOV Y,#$43                         ; PITCHH
    CALL WW_SPC700_WriteDSP
    MOV A,#$14
    MOV Y,#$44                         ; SRCN 20
    CALL WW_SPC700_WriteDSP
    MOV A,#$7F
    MOV Y,#$45                         ; GAIN (ignored while ADSR is enabled)
    CALL WW_SPC700_WriteDSP
    MOV A,#$DF
    MOV Y,#$46                         ; ADSR1: native-style fast voice envelope
    CALL WW_SPC700_WriteDSP
    MOV A,#$E0
    MOV Y,#$47                         ; ADSR2
    CALL WW_SPC700_WriteDSP

    MOV !WW_Write,#$00
    MOV !WW_Write+1,#$F4
    MOV !WW_Free,#!WW_SPC_RingBlocks
    MOV !WW_Queued,#$00
    MOV !WW_Playing,#$00
    MOV !WW_Expected,#$00
    MOV !WW_Final,#$00
    MOV !WW_State,#$01
    JMP WW_SPC700_AckAndRelease

WW_SPC700_Block:
    MOV A,!WW_Free
    BNE +
    JMP .full
+   MOV !WW_State,#$02
    MOV !WW_Expected,#$00
    MOV $00F7,#$5B                     ; ready for nine $A7 byte transactions
    MOV X,#$00
.ready_release:
    MOV A,$00F7
    BEQ .ready_released
    DEC X
    BNE .ready_release
    MOV !WW_State,#$01                 ; CPU vanished: resume the native sequencer
    MOV $00F7,#$00
    JMP WW_SPC700_ClearAndReturn
.ready_released:
    MOV $00F7,#$00
.next_byte:
    MOV X,#$00                         ; no partial block can stall native audio forever
.byte_loop:
    MOV A,$00F7
    BEQ .countdown                     ; zero is the normal inter-byte release gap
    CMP A,#$A5
    BNE .not_begin
    JMP WW_SPC700_Begin                ; a new request is also valid after an error
.not_begin:
    CMP A,#$A8
    BNE .not_stop
    JMP WW_SPC700_Stop
.not_stop:
    CMP A,#$A6
    BNE .not_next_block
    MOV A,!WW_Expected                 ; repeated ready is valid only at a block boundary
    BEQ +
    JMP .bad_byte
+   JMP WW_SPC700_Block
.not_next_block:
    CMP A,#$A7
    BEQ .receive_byte
.countdown:
    DEC X
    BNE +
    JMP .byte_timeout
+   JMP .byte_loop
.receive_byte:
    MOV A,!WW_State
    CMP A,#$02
    BEQ +
    JMP .bad_byte
+   MOV A,$00F4
    CMP A,!WW_Expected
    BEQ +
    JMP .bad_byte
+   MOV A,$00F5
    MOV Y,!WW_Expected
    BNE .header_done
    MOV Y,A
    AND A,#$01
    BEQ .header_restore
    MOV !WW_Final,#$01
.header_restore:
    MOV A,Y
.header_done:
    ; The final physical ring block needs end+loop unless it is the source's
    ; own final block. Its loop target is the ring base in directory entry 20.
    MOV Y,!WW_Expected
    BNE .store
    MOV Y,!WW_Write+1
    CMP Y,#$F7
    BNE .store
    MOV Y,!WW_Write
    CMP Y,#$F2
    BNE .store
    MOV Y,A
    AND A,#$01
    BEQ .set_loop
    MOV A,Y
    BRA .store
.set_loop:
    MOV A,Y
    OR A,#$02
.store:
    MOV Y,#$00
    MOV (!WW_Write)+Y,A
    INC !WW_Write
    BNE .no_carry
    INC !WW_Write+1
.no_carry:
    INC !WW_Expected
    MOV A,!WW_Expected
    CMP A,#$09
    BNE .ack_byte
    CALL WW_SPC700_CommitBlock         ; commit at byte 8, not at a later zero
.ack_byte:
    MOV A,$00F4
    CLRC
    ADC A,#$60
    MOV $00F7,A                        ; $60+index is the byte ack; APU2 is untouched
    MOV X,#$00
.wait_byte_release:
    MOV A,$00F7
    BEQ .byte_released
    DEC X
    BNE .wait_byte_release
    MOV $00F7,#$00                     ; drop a stuck ack and rewind partial data
    MOV A,!WW_State
    CMP A,#$02
    BNE .release_done
    MOV !WW_State,#$01
    CALL WW_SPC700_RewindBlock
.release_done:
    JMP WW_SPC700_ClearAndReturn
.byte_released:
    MOV $00F7,#$00                     ; only the SPC can clear its output ack
    MOV A,!WW_State
    CMP A,#$02
    BNE .committed
    JMP .next_byte                     ; partial block: wait for its next byte
.committed:
    JMP WW_SPC700_ClearAndReturn       ; committed block: resume native music/timer
.byte_timeout:
    JMP .bad_byte
.bad_byte:
    MOV !WW_State,#$01
    CALL WW_SPC700_RewindBlock         ; retry/new request cannot append to a partial block
    MOV $00F7,#$81
    BRA WW_SPC700_WaitRelease
.full:
    MOV $00F7,#$80                     ; explicit flow control avoids a CPU timeout stall
    JMP WW_SPC700_WaitRelease
WW_SPC700_RewindBlock:
    MOV A,!WW_Write
    SETC
    SBC A,!WW_Expected
    MOV !WW_Write,A
    BCS .done
    DEC !WW_Write+1
.done:
    MOV !WW_Expected,#$00
    RET
WW_SPC700_CommitBlock:
    MOV !WW_State,#$01
    MOV A,!WW_Write+1
    CMP A,#$F8
    BCC .no_wrap                       ; tolerate landing in the five pad bytes
    MOV !WW_Write,#$00
    MOV !WW_Write+1,#$F4
.no_wrap:
    DEC !WW_Free
    INC !WW_Queued
    MOV A,!WW_Playing
    BNE .done
    MOV A,!WW_Final
    BNE .final_start
    MOV A,!WW_Queued
    CMP A,#!WW_SPC_Prime
    BCC .done
    BRA .start
.final_start:
    MOV A,!WW_Queued
    CMP A,#12                          ; short clips start once fully uploaded
    BCC .done
.start:
    MOV !WW_Playing,#$01
    MOV A,#$10
    MOV Y,#$4C                         ; DSP KON, voice 4 only
    CALL WW_SPC700_WriteDSP
.done:
    RET

WW_SPC700_Stop:
    MOV A,!WW_State
    BEQ .inactive                      ; native SFX may already own voice 4
    MOV A,#$10
    MOV Y,#$5C                         ; KOF voice 4 only
    CALL WW_SPC700_WriteDSP
    CLR4 $1D
.inactive:
    MOV !WW_State,#$00
    MOV !WW_Playing,#$00
    MOV !WW_Queued,#$00
    MOV !WW_Free,#!WW_SPC_RingBlocks
WW_SPC700_AckAndRelease:
    MOV $00F7,#$5A
WW_SPC700_WaitRelease:
    MOV X,#$00
.wait:
    MOV A,$00F7
    BEQ .released
    DEC X
    BNE .wait
.released:
    MOV $00F7,#$00                     ; acknowledge release, or recover from a stuck CPU
WW_SPC700_ClearAndReturn:
    MOV A,#$00
    MOV $0000,A                        ; custom payload must not become native edges
    MOV $0001,A
    MOV $0003,A
    MOV $0004,A
    MOV $0005,A
    ; APU2 edge/input/output and $0006 music output are deliberately preserved.
    MOV $0007,A
    MOV $0008,A
    MOV $0009,A
    MOV $000B,A
    MOV $00F4,A
    MOV $00F5,A
WW_SPC700_Return:
    POP X
    POP Y                              ; restore native counter without using direct page
    MOV A,Y                            ; native BEQ needs the counter's zero flag
    RET

WW_SPC700_WriteDSP:
    MOV $00F2,Y
    MOV $00F3,A
    RET

WW_SPC700_End:

assert WW_SPC700_End <= $FFC0, "World Warrior SPC driver overlaps the IPL ROM"

arch 65816
base off
