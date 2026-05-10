    org 0

    ld hl, $0
start:
    ld a, (hl)
    inc hl
    jp start
