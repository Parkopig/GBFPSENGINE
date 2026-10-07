#include <gb/gb.h>
#include <stdint.h>
#include "health.h"

// player health starts at 100
uint8_t player_health = 100;

// tile indices for health bar
#define TILE_HEALTH_EMPTY  8
#define TILE_HEALTH_FULL   9

void health_init(void) {
    // load 2 tiles into VRAM (empty + full)
    // make sure tiledata[] in main.c includes these tiles
    // and set_bkg_data loads enough tiles to include them
}

void health_draw(void) {
    uint8_t bar_len = 10;          // half screen width
    uint8_t filled  = player_health / 10; // 0–10 tiles

    for(uint8_t x = 0; x < bar_len; x++) {
        uint8_t t = (x < filled) ? TILE_HEALTH_FULL : TILE_HEALTH_EMPTY;
        set_bkg_tiles(x, 0, 1, 1, &t);
    }
}

void health_damage(uint8_t amount) {
    if(player_health > amount)
        player_health -= amount;
    else
        player_health = 0;
}
