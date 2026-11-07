; Build against the unmodified 512 KiB Super Mario World (USA) ROM.
; Asar expands the LoROM image and fixes its internal size/checksum fields.
lorom

incsrc "rom/smw.asm"

; Native power-up meter and Guard+Punch pickup support.
org $01C538
    JML WW_Powerup
org $01E6CE
    JML WW_PickupBoard          ; Replaces BIT/BVC without splitting the branch.
    NOP

; Initial SPC engine image: stock SMW engine plus the streamed fighter driver.
org $0080E8
    LDA.B #WW_SPCImage
    STA.W $0000
    LDA.B #WW_SPCImage>>8
    STA.W $0001
    LDA.B #WW_SPCImage>>16
    STA.W $0002

; Reapply custom menu registers after native NMI controller/register handling.
org $008246
    JML WW_MenuLateNMI
    NOP

; Custom title and selection own VRAM; skip only native StripeImage uploads.
org $0085D2
    JML WW_MenuStripe
    NOP #3

; Native title and post-file-menu fighter selection.
org $009C64
    JML WW_TitleMode
    NOP #2
org $009F6F
    JML WW_MapMode
    NOP

; GM14: run combat before vanilla player/sprite simulation.
org $00A1DA
    JML WW_Main

; Player renderer: native OAM replacement, with vanilla fallback outside levels.
org $00E2BD
    JML WW_Draw

; After player drawing, freeze the rest of gameplay while a custom menu is active.
org $00A2F3
    JML WW_AfterDraw

; Player damage: vitality/guard/parry replacement.
org $00F5B7
    JML WW_Hurt

; NMI: upload fighter/projectile/HUD tiles and the fighter palette.
org $00A300
    JML WW_DMA

; Replace Bowser's native battle AI while the custom finale is active.
org $03A279
    JML WW_BowserNative

; The first bank beyond the 512 KiB vanilla ROM is safe on real LoROM boards
; and emulators; no copier-specific features or enhancement chip are required.
org $108000
incsrc "rom/main.asm"
incsrc "rom/roster.asm"
incsrc "rom/menu.asm"
incsrc "rom/system.asm"
incsrc "rom/boss.asm"
incsrc "rom/render.asm"
incsrc "rom/graphics.asm"
incsrc "rom/boss_gfx.asm"
incsrc "rom/select_gfx.asm"
incsrc "rom/menu_gfx.asm"
incsrc "rom/perf.asm"
incsrc "rom/roster_assets.asm"
incsrc "rom/ending.asm"
incsrc "rom/audio.asm"

; Keep the patch self-identifying in the SNES header without changing the title.
org $00FFD5
    db $20                      ; SlowROM, LoROM (explicitly no SA-1/SuperFX).
org $00FFD7
    db $0C                      ; 4 MiB expanded ROM, not the 512 KiB base.
