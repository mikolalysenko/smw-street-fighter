; Roster data and shared move metadata. Character order matches the desktop game.
; Ryu, Honda, Chun-Li, Guile, Ken, Blanka, Zangief, Dhalsim,
; Balrog, Vega, Sagat, M. Bison.

WW_RosterVitality:
    db 100,125,90,110,100,115,145,95,125,95,120,125
WW_RosterSpeed:
    db 23,18,27,22,25,21,18,19,25,29,21,25
WW_RosterSpecial:
    db !MoveHadouken,!MoveRush,!MoveHadouken,!MoveHadouken
    db !MoveHadouken,!MoveRush,!MoveHurricane,!MoveHadouken
    db !MoveRush,!MoveRush,!MoveHadouken,!MoveRush
WW_RosterUp:
    db !MoveShoryuken,!MoveFlurry,!MoveHurricane,!MoveShoryuken
    db !MoveShoryuken,!MoveShoryuken,!MoveThrow,!MoveFlame
    db !MoveShoryuken,!MoveDive,!MoveShoryuken,!MoveDive
WW_RosterDown:
    db !MoveHurricane,!MoveFlurry,!MoveFlurry,!MoveShoryuken
    db !MoveHurricane,!MoveElectric,!MoveHurricane,!MoveTeleport
    db !MoveRush,!MoveDive,!MoveHadouken,!MoveDive

WW_MoveDurations:
    db 0,18,26,28,64,32,36,46,64,48,26,60,38,44
WW_MovePoses:
    db !PoseIdle,!PosePunch,!PoseKick,!PoseHadouken,!PoseHadouken
    db !PoseShoryuken,!PoseHurricane,!PoseRush,!PoseFlurry,!PoseThrow
    db !PoseTeleport,!PoseElectric,!PoseFlame,!PoseDive

; Frames at which projectile-like moves emit their first shot.
WW_MoveSpawnFrame:
    db 0,0,0,12,16,0,0,0,0,0,0,0,10,0

; Native super presentation and broad behavior. 0 projectile, 1 rush,
; 2 flurry, 3 grapple, 4 electricity, 5 multi-projectile.
WW_RosterSuperKind:
    db 5,1,2,5,5,1,3,5,1,1,5,1

WW_RosterNames:
    db "RYU     "
    db "E.HONDA "
    db "CHUN-LI "
    db "GUILE   "
    db "KEN     "
    db "BLANKA  "
    db "ZANGIEF "
    db "DHALSIM "
    db "BALROG  "
    db "VEGA    "
    db "SAGAT   "
    db "M.BISON "

WW_ComboAwards:
    dw 200,400,800,1000,2000,4000,8000,8000,8000,8000,8000,8000

; Native sound IDs for neutral, up/down and super actions.
WW_SoundSpecial:
    db 8,11,31,12,8,20,22,23,25,25,26,28
WW_SoundUp:
    db 9,11,13,9,9,20,22,24,9,25,27,28
WW_SoundDown:
    db 10,11,29,9,10,21,22,24,25,25,26,28
WW_SoundSuper:
    db 37,5,5,5,37,20,22,24,5,5,27,36
