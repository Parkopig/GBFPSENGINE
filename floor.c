#include "floor.h"
#include "map.h"   // your map + floor_map + floor_texture_base

#define SCREEN_H 18
#define PROJ_CONST 90

static fix8 inv_y[SCREEN_H];

void floor_init(void)
{
    const int16_t center = SCREEN_H >> 1;

    for (uint8_t y = 0; y < SCREEN_H; y++)
    {
        int16_t dy = y - center;
        inv_y[y] = (dy <= 0) ? 0 : fix_div(TO_FIX(1), TO_FIX(dy));
    }
}

uint8_t floor_sample(
    fix8 posX, fix8 posY,
    fix8 rayDirX, fix8 rayDirY,
    uint8_t screenY
){
    fix8 inv = inv_y[screenY];
    if (!inv) return floor_texture_base[0];

    fix8 scale = fix_mul(TO_FIX(PROJ_CONST), inv);

    fix8 worldX = posX + fix_mul(rayDirX, scale);
    fix8 worldY = posY + fix_mul(rayDirY, scale);

    int16_t mx = FROM_FIX(worldX);
    int16_t my = FROM_FIX(worldY);

    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H)
        return floor_texture_base[0];

    uint8_t ftype = floor_map[my * MAP_W + mx];
    return floor_texture_base[ftype];
}
