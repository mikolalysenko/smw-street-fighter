; Optional hardware raster-time instrumentation. Excluded from normal builds.
; The red CGRAM-zero band spans GM14 through native/player drawing. VCounter
; snapshots include interrupt time and wrap across the 262-line NTSC frame.
if defined("WW_PERFORMANCE")
WW_PerfStart:
    SEP #$30
    LDA.W !GameMode
    CMP.B #$14
    BEQ .in_level
    RTL
.in_level:
    LDA.L !WWInitialized
    CMP.B #$5A
    BEQ .initialized
    LDA.B #$00
    STA.L !WWPerfMax
    STA.L !WWPerfMax+1
    STA.L !WWPerfAudioMax
    STA.L !WWPerfAudioMax+1
.initialized:
    LDA.W $2137                   ; Latch H/V counters.
    LDA.W $213D
    STA.L !WWPerfStart
    LDA.W $213D
    AND.B #$01
    STA.L !WWPerfStart+1
    STZ.W $2121                   ; CGRAM zero: bright red during measured work.
    STZ.W $2122
    LDA.B #$7C
    STA.W $2122
    RTL

WW_PerfEnd:
    SEP #$30
    LDA.W !GameMode
    CMP.B #$14
    BEQ .in_level
    RTL
.in_level:
    LDA.W $2137
    LDA.W $213D
    STA.L !WWPerfEnd
    LDA.W $213D
    AND.B #$01
    STA.L !WWPerfEnd+1
    REP #$20
    LDA.L !WWPerfEnd
    SEC
    SBC.L !WWPerfStart
    BPL .elapsed
    CLC
    ADC.W #262                    ; NTSC, non-interlaced frame length.
.elapsed:
    CMP.L !WWPerfMax
    BCC .restore
    STA.L !WWPerfMax
.restore:
    SEP #$20
    STZ.W $2121                   ; Diagnostic backdrop is black outside the band.
    STZ.W $2122
    STZ.W $2122
    RTL

WW_PerfAudioStart:
    SEP #$30
    LDA.W $2137
    LDA.W $213D
    STA.L !WWPerfAudioStart
    LDA.W $213D
    AND.B #$01
    STA.L !WWPerfAudioStart+1
    RTL

WW_PerfAudioEnd:
    SEP #$30
    LDA.W $2137
    LDA.W $213D
    STA.L !WWPerfAudioEnd
    LDA.W $213D
    AND.B #$01
    STA.L !WWPerfAudioEnd+1
    REP #$20
    LDA.L !WWPerfAudioEnd
    SEC
    SBC.L !WWPerfAudioStart
    BPL .elapsed
    CLC
    ADC.W #262
.elapsed:
    CMP.L !WWPerfAudioMax
    BCC .done
    STA.L !WWPerfAudioMax
.done:
    SEP #$20
    RTL
endif
