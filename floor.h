#ifndef FLOOR_H
#define FLOOR_H

#include <gb/gb.h>
#include <stdint.h>
#include "fixmath.h"

void floor_init(void);

uint8_t floor_sample(
    fix8 posX, fix8 posY,
    fix8 rayDirX, fix8 rayDirY,
    uint8_t screenY
);

#endif
