#include <gb/gb.h>
#include <stdint.h>

#include "overlay.h"
#include "enemy.h"
#include "fixmath.h"
#include "zombie_sheet.h"
#include "zombie_far.h"
#include "zombie_attack.h"

static uint8_t g_dist = 0;

static inline fix8 fix_mul(fix8 a, fix8 b) {
    return (fix8)(((int32_t)a * (int32_t)b) >> FIX_SHIFT);
}

static inline fix8 fix_div(fix8 a, fix8 b) {
    return (fix8)(((int32_t)a << FIX_SHIFT) / b);
}

// returns 1 if cell is solid wall
static inline uint8_t is_wall(uint8_t cell)
{
    return (cell != 0);
}



// --------------------------------------
// PICKUP SYSTEM
// --------------------------------------

#define PICKUP_AMMO 1

typedef struct Pickup {
    uint8_t active;
    uint8_t type;
    fix8 x;
    fix8 y;
} Pickup;

#define PICKUP_MAX 16
Pickup pickups[PICKUP_MAX];


//PLAYER HEALTH  
#include "health.h"

#define BULLET_TILE_FULL   50
#define BULLET_TILE_HALF   51
#define BULLET_TILE_EMPTY  0   // white tile

#define ZOMBIE_TILE_BASE overlay_TILE_COUNT
#define ZOMBIE_ATTACK_TILE_BASE (ZOMBIE_TILE_BASE + 64)

#define OBJ_TILE_BASE (ZOMBIE_ATTACK_TILE_BASE + zombie_attack_TILE_COUNT)
#define AMMO_TILE_BASE (OBJ_TILE_BASE + 4)

#define PICKUP_ICON_TILE 52

#define DOOR_TILE_BASE 60   // pick a safe unused BG tile index




#define PICKUP_SPRITE_ID_BASE  24  // 12,13,14,15 will be the pickup metasprite



uint8_t player_ammo = 8;      // 6 bullets = 3 bars (2 bullets per bar)
uint8_t player_ammo_max = 8;
uint8_t ammo_reserve = 0;     // NEW: stored ammo
uint8_t ammo_reserve_max = 99;
uint8_t auto_reload_timer = 0;

fix8 pz = 0;   // player vertical position (Q8.8)




const palette_color_t bullets_palettes[4] = {
    RGB8(224,248,207), RGB8(134,192,108), RGB8(  7, 24, 33), RGB8(  0,  0,  0)
};

const uint8_t bullets_tiles[16] = {
	0x00,0x99,0x66,0x00,0x66,0x00,0x00,0x00,
	0x00,0x66,0x00,0x66,0x66,0x00,0x00,0x66
};

const palette_color_t bullets2_palettes[4] = {
    RGB8(224,248,207), RGB8(134,192,108), RGB8(  7, 24, 33), RGB8(  0,  0,  0)
};

const uint8_t bullets2_tiles[16] = {
    0x00,0xf9,0x06,0xf0,0x06,0xf0,0x00,0xf0,
	0x00,0xf6,0x00,0xf6,0x06,0xf0,0x00,0xf6
};

// Generic far LOD object (4 tiles)
const uint8_t obj_tiles[64] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x03,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x00,0xc0,0x00,
    0x03,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0x00,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

// ammo 
const uint8_t ammo_tiles[16] = {
	0x02,0xfd,0x73,0x80,0x54,0x88,0x00,0x80,0x40,0x81,0x40,0x81,0x00,0x87,0x00,0xff
	};

void pickup_spawn(uint8_t type, uint8_t mx, uint8_t my)
{
    for(uint8_t i = 0; i < PICKUP_MAX; i++) {
        if(!pickups[i].active) {
            pickups[i].active = 1;
            pickups[i].type = type;
            pickups[i].x = TO_FIX(mx);
            pickups[i].y = TO_FIX(my);
            return;
        }
    }
}

void pickup_init() {
    for(uint8_t i = 0; i < PICKUP_MAX; i++) {
        pickups[i].active = 0;
    }
}

 

void draw_zombie(uint8_t base_tile, uint8_t x, uint8_t y, uint8_t sprite_id);

static int8_t iabs(int8_t v) {
    return (v < 0) ? -v : v;
}


//HEALTH 

//uint8_t player_health = 100;



// --------------------------------------
// Angle + trig tables (32 steps)
// --------------------------------------

#define ANGLES 32
#define ANGLE_MASK (ANGLES - 1)

const int8_t cos_lut[ANGLES] = {
    64, 59, 45, 24,  0, -24, -45, -59,
   -64,-59,-45,-24,  0,  24,  45,  59,
    64, 59, 45, 24,  0, -24, -45, -59,
   -64,-59,-45,-24,  0,  24,  45,  59
};

const int8_t sin_lut[ANGLES] = {
     0, 24, 45, 59, 64, 59, 45, 24,
     0,-24,-45,-59,-64,-59,-45,-24,
     0, 24, 45, 59, 64, 59, 45, 24,
     0,-24,-45,-59,-64,-59,-45,-24
};

// --------------------------------------
// Map
// --------------------------------------

#define MAP_W 32
#define MAP_H 32

// map key
// 1 brick
// 2 rock
// 3 tree
// 4 window
// 5 door1
// 6 slope (special up/down tile)

// example: in height_map, use -1 for pit, 0 for normal, +1 for raised,
// and maybe -2 for a deeper pit if you want.




uint8_t map[MAP_W * MAP_H] = {
    // Row 0
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,3,3,3,3,3,0,3,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,1,1,1,1,1,0,0,3,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,1,0,0,0,1,0,0,3,3,3,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,0,3,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,3,0,3,3,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
};

// --------------------------------------
// Wall height table
// --------------------------------------

const uint8_t wall_height[32] = {
    16,16,15,14,13,12,11,10,
     9, 8, 7, 6, 5, 4, 3, 2,
     2, 2, 2, 2, 1, 1, 1, 1,
     1, 1, 1, 1, 0, 0, 0, 0
};

const uint8_t wall_height_mul[] = { 1,1,1,1,1 };

const uint8_t wall_texture_base[] = { 0,2,7,10,7 };

const uint8_t floor_texture_base[] = {
    1,   // type 0 floor
    42,  // type 1 floor
    60,  // type 2 floor (example)
    // add more
};

uint8_t floor_map[MAP_W * MAP_H] = {
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3
};


int8_t height_map[MAP_W * MAP_H] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};



// --------------------------------------
// Tile data
// --------------------------------------

const uint8_t tiledata[] = {
    // 0: ceiling
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    // 1: floor
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,

// brick dark
0x77,0x00,0x00,0x00,
0xdd,0x00,0x00,0x00,
0x77,0x00,0x00,0x00,
0xdd,0x00,0x00,0x00,


// brick medium
0x6a,0x15,0x55,0x2a,
0x7f,0x00,0x00,0x00,
0xa6,0x51,0x55,0xa2,
0xf7,0x00,0x00,0x00,


// brick light
0x6a,0x15,0x55,0x2a,
0x7f,0x00,0x00,0x00,
0xa6,0x51,0x55,0xa2,
0xf7,0x00,0x00,0x00,


    // 5: minimap wall
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,

    // 6: minimap player / enemy dot
    0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,
    0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,

    // 7–21: your existing wall textures...
    // (unchanged, same as your file)
    0x91,0xFF,0x0A,0xFF,0x4C,0xFF,0xE0,0xFF,
    0x44,0xFF,0xE1,0xFF,0x9E,0xFF,0x18,0xFF,

    0x6E,0x91,0xF5,0x0A,0xB3,0x4C,0x1F,0xE0,
    0xBB,0x44,0x1E,0xE1,0x61,0x9E,0xE7,0x18,

    0x91,0x00,0x0A,0x00,0x4C,0x00,0xE0,0x00,
    0x44,0x00,0xE1,0x00,0x9E,0x00,0x18,0x00,

    0x18,0x07,0x1A,0x05,0x1A,0xC5,0x12,0x6D,
    0x0A,0x35,0x2A,0x15,0x1A,0x05,0x18,0x07,

    0x80,0x60,0xA0,0x40,0x20,0xC0,0xA0,0x40,
    0x80,0x64,0xA0,0x58,0x80,0x70,0x00,0xE0,

    0x0A,0x15,0x02,0x1D,0x00,0x1F,0x24,0x1B,
    0x24,0x1B,0x15,0x0A,0x11,0x0E,0x15,0x0A,

    0x80,0x60,0x90,0x60,0xD0,0x20,0xC0,0x20,
    0x40,0xA0,0x00,0xE0,0x40,0xA0,0x50,0xA0,

    0x8C,0x73,0x6C,0x93,0x2E,0xD1,0xBB,0x44,
    0xCE,0x31,0xDB,0x24,0x55,0xAA,0x52,0xAD,

    0x94,0x6B,0xAC,0x53,0xA5,0x5A,0x76,0x89,
    0x54,0xAB,0x4E,0xB1,0x6A,0x95,0xA9,0x56,

    0x77,0x88,0xDA,0x25,0xDA,0x25,0x56,0xA9,
    0x77,0x88,0x3D,0xC2,0xA9,0x56,0xA9,0x56,

    0x99,0x66,0x9B,0x64,0xAC,0x53,0xEC,0x13,
    0xCA,0x35,0x53,0xAC,0x57,0xA8,0x3A,0xC5,

    0x3A,0x04,0x1A,0x24,0x3A,0x84,0x38,0x46,
    0x1A,0x64,0x50,0x2F,0x18,0x26,0x1A,0x24,

    0x3A,0x04,0x1A,0x24,0x3A,0x84,0x38,0x46,
    0x1A,0x64,0x50,0x2F,0x18,0x26,0x1A,0x24,

    0x0A,0x34,0x0A,0x34,0x26,0x18,0x76,0x08,
    0x74,0x0A,0x38,0x06,0x2E,0x10,0x3E,0x00,

    0x0A,0x34,0x0A,0x34,0x26,0x18,0x76,0x08,
    0x74,0x0A,0x38,0x06,0x2E,0x10,0x3E,0x00,
};

// 32x32 bark LIGHT (tiles 26–41), loaded at VRAM tile 22
const uint8_t bark32_light[] = {
    // (unchanged, same as your file)
    0x00,0x00,0x00,0x00,0x20,0x40,0x30,0xC0,
    0x58,0x20,0x20,0x1C,0x12,0x0C,0x09,0x06,

    0x2E,0xD1,0x0E,0xF1,0x5F,0xA0,0x53,0xAC,
    0x51,0xAE,0xD4,0x2B,0xC5,0x3A,0xD5,0x2A,

    0xE2,0x1D,0xEA,0x15,0xFE,0x01,0xBA,0x45,
    0x3A,0xC5,0x2A,0xD5,0xAA,0x55,0xA0,0x5F,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0x04,0x03,0x00,0x03,0x02,0x01,0x01,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0xE2,0x1D,0xEA,0x15,0xFE,0x01,0xBA,0x45,
    0x3A,0xC5,0x2A,0xD5,0xAA,0x55,0xA0,0x5F,

    0x2E,0xD1,0x0E,0xF1,0x5F,0xA0,0x53,0xAC,
    0x51,0xAE,0xD4,0x2B,0xC5,0x3A,0xD5,0x2A,

    0x04,0x02,0x02,0x0C,0x04,0x18,0xC8,0x30,
    0x90,0x60,0x20,0xC0,0x40,0x80,0x80,0x00,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0x2E,0xD1,0x0E,0xF1,0x5F,0xA0,0x53,0xAC,
    0x51,0xAE,0xD4,0x2B,0xC5,0x3A,0xD5,0x2A,

    0xE2,0x1D,0xEA,0x15,0xFE,0x01,0xBA,0x45,
    0x3A,0xC5,0x2A,0xD5,0xAA,0x55,0xA0,0x5F,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,

    0xE2,0x1D,0xEA,0x15,0xFE,0x01,0xBA,0x45,
    0x3A,0xC5,0x2A,0xD5,0xAA,0x55,0xA0,0x5F,

    0x2E,0xD1,0x0E,0xF1,0x5F,0xA0,0x53,0xAC,
    0x51,0xAE,0xD4,0x2B,0xC5,0x3A,0xD5,0x2A,

    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

// 16×16 BRICK LIGHT (tiles 42–45)
const uint8_t brickl16[] = {
	0x40,0x3f,0x40,0x3f,0x7f,0x00,0x00,0x00,
	0x04,0xf3,0x04,0xf3,0xf7,0x00,0x00,0x00,
	
	0x40,0x3f,0x40,0x3f,0x7f,0x00,0x00,0x00,
	0x04,0xf3,0x04,0xf3,0xf7,0x00,0x00,0x00,

	0x40,0x3f,0x40,0x3f,0x7f,0x00,0x00,0x00,
	0x04,0xf3,0x04,0xf3,0xf7,0x00,0x00,0x00,

	0x40,0x3f,0x40,0x3f,0x7f,0x00,0x00,0x00,
	0x04,0xf3,0x04,0xf3,0xf7,0x00,0x00,0x00,
};

// DOOR TILESET (4 tiles)
const uint8_t door_tiles[32] = {
	0x55,0x2a,0x51,0x2e,0x53,0x2c,0x43,0x3c,0x6b,0x14,0x49,0x36,0x51,0x2e,0x55,0x2a,
	0x55,0x2a,0x51,0x2e,0x53,0x2c,0x43,0x3c,0x6b,0x14,0x49,0x36,0x51,0x2e,0x55,0x2a
	};


// --------------------------------------
// Render settings
// --------------------------------------

#define SCREEN_W 20
#define SCREEN_H 18
#define RENDER_COLS 20
#define PROJ_CONST 90
#define STEP_LEN (TO_FIX(1) / 2)
#define FOV 3

fix8 px = TO_FIX(4);
fix8 py = TO_FIX(4);
uint8_t pangle = 0;

uint8_t show_map = 0;


// --------------------------------------
// Minimap
// --------------------------------------

void draw_minimap(void)
{
    // player tile coords
    uint8_t pxm = FROM_FIX(px);
    uint8_t pym = FROM_FIX(py);

    // viewport size on BG (we’ll use 16x16 inside the 20x18 screen)
    const uint8_t VIEW_W = 16;
    const uint8_t VIEW_H = 16;

    // center player in the viewport
    int8_t origin_x = (int8_t)pxm - (VIEW_W / 2);
    int8_t origin_y = (int8_t)pym - (VIEW_H / 2);

    // clamp so we don’t read outside map
    if (origin_x < 0) origin_x = 0;
    if (origin_y < 0) origin_y = 0;
    if (origin_x > MAP_W - VIEW_W) origin_x = MAP_W - VIEW_W;
    if (origin_y > MAP_H - VIEW_H) origin_y = MAP_H - VIEW_H;

    // draw base map + pits as walls
    for (uint8_t vy = 0; vy < VIEW_H; vy++) {
        for (uint8_t vx = 0; vx < VIEW_W; vx++) {

            uint8_t mx = origin_x + vx;
            uint8_t my = origin_y + vy;

            uint8_t cell = map[my * MAP_W + mx];
            int8_t  h    = height_map[my * MAP_W + mx];

            // treat pits (h < 0) like solid walls on minimap
            uint8_t t = (cell != 0 || h < 0) ? 5 : 0;

            // draw at BG coords (e.g. top-left corner of screen)
            set_bkg_tiles(vx, vy, 1, 1, &t);
        }
    }

    // enemies as dots (only if inside viewport)
    for (uint8_t i = 0; i < ENEMY_MAX; i++) {
        if (!enemies[i].active) continue;

        uint8_t ex = FROM_FIX(enemies[i].x);
        uint8_t ey = FROM_FIX(enemies[i].y);

        if (ex >= origin_x && ex < origin_x + VIEW_W &&
            ey >= origin_y && ey < origin_y + VIEW_H) {

            uint8_t vx = ex - origin_x;
            uint8_t vy = ey - origin_y;
            uint8_t e_tile = 7; // enemy dot
            set_bkg_tiles(vx, vy, 1, 1, &e_tile);
        }
    }

    // pickups as icons (only if inside viewport)
    for (uint8_t i = 0; i < PICKUP_MAX; i++) {
        if (!pickups[i].active) continue;

        uint8_t ix = FROM_FIX(pickups[i].x);
        uint8_t iy = FROM_FIX(pickups[i].y);

        if (ix >= origin_x && ix < origin_x + VIEW_W &&
            iy >= origin_y && iy < origin_y + VIEW_H) {

            uint8_t vx = ix - origin_x;
            uint8_t vy = iy - origin_y;
            uint8_t tile = 8; // ammo pickup icon
            set_bkg_tiles(vx, vy, 1, 1, &tile);
        }
    }

    // player dot (always centered if possible)
    uint8_t p_vx = pxm - origin_x;
    uint8_t p_vy = pym - origin_y;
    uint8_t p_tile = 6;

    if (p_vx < VIEW_W && p_vy < VIEW_H) {
        set_bkg_tiles(p_vx, p_vy, 1, 1, &p_tile);
    }
}

// --------------------------------------
// Shading
// --------------------------------------

uint8_t shade_for_dist(uint8_t steps) {
    if (steps < 2)  return 2;   // bright
    if (steps < 5)  return 3;   // mid
    if (steps < 6) return 4;   // dark
    return 4;                   // far = dark (NOT tile 5)
}

uint8_t edge_shade(uint8_t ax, uint8_t ay) {
    // ax = horizontal hit offset (0–15)
    // ay = vertical hit offset (0–15)

    uint8_t edge = (ax < ay) ? ax : ay;

    //if (edge < 1) return 0;   // bright edge
    if (edge < 4) return 1;   // medium bright
    if (edge < 8) return 2;  // slight bright
    return 3;                 // no highlight
}



// --------------------------------------
// Column renderer 
// --------------------------------------

void draw_column(
    uint8_t col,
    uint8_t h,
    uint8_t tile,
    uint8_t wall_type,
    uint8_t dist,
    uint8_t ax,
    int8_t height_offset
)
{
    if (h > SCREEN_H) h = SCREEN_H;
    if (h < 1)        h = 1;

    int16_t top = ((SCREEN_H - h) >> 1) - height_offset;
    if (top < 0)            top = 0;
    if (top > SCREEN_H - 1) top = SCREEN_H - 1;

    uint8_t half = h >> 1;
    uint8_t col4 = col & 3;
    uint8_t col2 = col & 1;

    uint8_t near = (dist < 6);
    uint8_t mid  = (dist < 14);

    uint8_t base32 = 22;

    for (uint8_t y = 0; y < SCREEN_H; y++) {

        uint8_t t;

        if (y < top) {
            t = 0;   // ceiling
        }
        else if (y >= top + h) {
            t = 1;   // placeholder floor (we will overwrite with sampled floor tiles)
        }
        else {
            uint8_t local = y - top;

            if (wall_type == 4) {
                uint8_t ly = (local * 16) / h;
                uint8_t tx = ax;

                if (dist < 6) {
                    if (ly >= 4 && ly < 12) {
                        uint8_t base = wall_texture_base[2];
                        uint8_t row  = (ly - 4) >> 3;
                        uint8_t colb = tx & 1;
                        t = base + row * 2 + colb;
                    } else {
                        uint8_t row = ly >> 3;
                        t = 42 + row * 2 + (tx & 1);
                    }
                }
                else if (dist < 14) {
                    t = (ly >= 6 && ly < 10) ? 8 : 4;
                }
                else {
                    t = (ly >= 7 && ly < 9) ? 8 : 2;
                }
            }
            else if (wall_type == 5) {
                uint8_t ly     = (local * 16) / h;
                uint8_t row    = ly >> 3;
                uint8_t colbit = col & 1;
                uint8_t index  = (row * 2 + colbit) & 1;
                t = DOOR_TILE_BASE + index;
            }
            else if (wall_type == 1) {
                if (dist < 6) {
                    uint8_t ly     = (local * 16) / h;
                    uint8_t row    = ly >> 3;
                    uint8_t colbit = col & 1;
                    t = 42 + row * 2 + colbit;
                }
                else if (dist < 14) {
                    t = 4;
                }
                else {
                    t = 2;
                }
            }
            else if (wall_type != 3) {
                uint8_t base       = wall_texture_base[wall_type];
                uint8_t brightness = shade_for_dist(dist);

                if (brightness > 4) brightness = 4;
                if (brightness < 2) brightness = 2;

                t = base + (brightness - 2);
            }
            else {
                uint8_t local2 = y - top;

                if (local2 < half) {
                    uint8_t ly  = (local2 * 16) / half;
                    uint8_t row = ly >> 3;
                    t = 14 + (row << 1) + col2;
                }
                else {
                    uint8_t by = local2 - half;

                    if (near) {
                        uint8_t tex_y    = (by * 16) / half;
                        uint8_t tile_row = 1 + (tex_y >> 3);
                        t = base32 + (tile_row << 2) + col4;
                    }
                    else if (mid) {
                        uint8_t ly  = (by * 16) / half;
                        uint8_t row = (ly < 8) ? 0 : 1;
                        t = 10 + (row << 1) + col2;
                    }
                    else {
                        t = 18 + col2;
                    }
                }
            }
        }

        set_bkg_tiles(col, y + 1, 1, 1, &t);
    }

    // NOTE: floor tiles are drawn per-scanline in render_frame so we don't draw them here.
}

// --------------------------------------
// Main Renderer 
// --------------------------------------


void render_frame(void) {
    // Direction / plane
    fix8 dirX = TO_FIX(cos_lut[pangle]) / 64;
    fix8 dirY = TO_FIX(sin_lut[pangle]) / 64;

    fix8 FOV_SCALE = TO_FIX(1) >> 1;
    fix8 planeX = -fix_mul(dirY, FOV_SCALE);
    fix8 planeY =  fix_mul(dirX, FOV_SCALE);

    // Player tile and height
    uint8_t pxm = FROM_FIX(px);
    uint8_t pym = FROM_FIX(py);
    int8_t player_h = height_map[pym * MAP_W + pxm];

    // floor texture base count
    const uint8_t floor_base_count =
        (uint8_t)(sizeof(floor_texture_base) / sizeof(floor_texture_base[0]));

    // screen center for row distance formula
    const int16_t screenCenter = (SCREEN_H >> 1);

    // Precompute cameraX for each column (avoid fix_div per column)
    static fix8 cameraX_table[RENDER_COLS];
    for (uint8_t c = 0; c < RENDER_COLS; c++) {
        cameraX_table[c] = fix_div(TO_FIX(c) * 2, TO_FIX(RENDER_COLS)) - TO_FIX(1);
    }

    // Precompute rowDist for each screen row once per frame (avoid repeated fix_div)
    // rowDist[y] approximates distance for screen row y using your original formula:
    // rowDist = SCREEN_H / (y - screenCenter)
    static fix8 rowDist_lut[SCREEN_H];
    for (int16_t y = 0; y < SCREEN_H; y++) {
        int16_t dy = y - screenCenter;
        if (dy <= 0) dy = 1; // avoid division by zero or negative
        rowDist_lut[y] = fix_div(TO_FIX(SCREEN_H), TO_FIX(dy));
    }

    // Column buffer for a vertical block of floor tiles
    uint8_t colbuf[SCREEN_H];

    // Main column loop
    for (uint8_t col = 0; col < RENDER_COLS; col++) {

        fix8 cameraX = cameraX_table[col];

        // ray direction for this column
        fix8 rayDirX = dirX + fix_mul(planeX, cameraX);
        fix8 rayDirY = dirY + fix_mul(planeY, cameraX);

        // clamp tiny values
        const fix8 EPS = TO_FIX(1) >> 4;
        if (rayDirX > -EPS && rayDirX < EPS) rayDirX = (rayDirX < 0) ? -EPS : EPS;
        if (rayDirY > -EPS && rayDirY < EPS) rayDirY = (rayDirY < 0) ? -EPS : EPS;

        fix8 posX = px;
        fix8 posY = py;

        int8_t mapX = FROM_FIX(posX);
        int8_t mapY = FROM_FIX(posY);

        fix8 absDirX = (rayDirX < 0) ? -rayDirX : rayDirX;
        fix8 absDirY = (rayDirY < 0) ? -rayDirY : rayDirY;

        fix8 deltaDistX = absDirX ? fix_div(TO_FIX(1), absDirX) : TO_FIX(64);
        fix8 deltaDistY = absDirY ? fix_div(TO_FIX(1), absDirY) : TO_FIX(64);

        int8_t stepX = (rayDirX < 0) ? -1 : 1;
        int8_t stepY = (rayDirY < 0) ? -1 : 1;

        fix8 sideDistX = (rayDirX < 0)
            ? fix_mul(posX - TO_FIX(mapX), deltaDistX)
            : fix_mul(TO_FIX(mapX + 1) - posX, deltaDistX);

        fix8 sideDistY = (rayDirY < 0)
            ? fix_mul(posY - TO_FIX(mapY), deltaDistY)
            : fix_mul(TO_FIX(mapY + 1) - posY, deltaDistY);

        uint8_t hit = 0;
        uint8_t side = 0;
        uint8_t wall_type = 1;
        int8_t cell_h = 0;
        uint8_t is_step = 0;

        // DDA to find wall hit
        while (!hit) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX < 0 || mapX >= MAP_W || mapY < 0 || mapY >= MAP_H) {
                hit = 1;
                wall_type = 2;
                cell_h = 0;
                break;
            }

            uint8_t cell = map[mapY * MAP_W + mapX];
            cell_h = height_map[mapY * MAP_W + mapX];

            if (cell != 0) {
                hit = 1;
                wall_type = cell;
                is_step = 0;
            } else if (cell_h != player_h && cell_h > player_h) {
                hit = 1;
                wall_type = 1;
                is_step = 1;
            }
        }

        // true distance to wall hit
        fix8 trueDist = (side == 0)
            ? (sideDistX - deltaDistX)
            : (sideDistY - deltaDistY);

        if (trueDist < TO_FIX(1)) trueDist = TO_FIX(1);

        // projection distance and wall height
        fix8 projDist = trueDist >> 1;
        if (projDist < TO_FIX(1)) projDist = TO_FIX(1);

        uint8_t h = (uint8_t)((SCREEN_H << FIX_SHIFT) / projDist);
        if (h > SCREEN_H) h = SCREEN_H;
        if (h < 1) h = 1;

        int8_t height_offset = 0;
        if (is_step) {
            int8_t rel = cell_h - player_h;
            height_offset = rel * 3;
        }

        // compute hitPos for texture X
        fix8 hitPos = (side == 0)
            ? posY + fix_mul(trueDist, rayDirY)
            : posX + fix_mul(trueDist, rayDirX);

        uint8_t ax = ((hitPos >> 8) & 0x0F);

        uint8_t dist = FROM_FIX(trueDist);
        if (dist > 31) dist = 31;
        if (dist < 1) dist = 1;

        uint8_t base = wall_texture_base[wall_type];
        uint8_t brightness = shade_for_dist(dist);

        if (side && brightness > 2) brightness--;
        if (brightness > 4) brightness = 4;
        if (brightness < 2) brightness = 2;

        uint8_t tile = base + (brightness - 2);
        if (wall_type == 3) tile = base;

        // draw wall/ceiling/placeholder floor rows as before
        draw_column(col, h, tile, wall_type, dist, ax, height_offset);

        // compute wall bottom and floor block
        int16_t wall_bottom = ((SCREEN_H - h) >> 1) - height_offset;
        wall_bottom += h - 1;
        if (wall_bottom < -1) wall_bottom = -1;
        if (wall_bottom >= SCREEN_H) wall_bottom = SCREEN_H - 1;

        int16_t floor_start_row = wall_bottom + 1;
        int16_t floor_rows = SCREEN_H - floor_start_row;
        if (floor_rows <= 0) continue;

        // Fill column buffer using precomputed rowDist_lut for accurate sampling
        for (int16_t r = 0; r < floor_rows; r++) {
            int16_t screen_y = floor_start_row + r;
            fix8 rowDist = rowDist_lut[screen_y];

            // world sample point = pos + rayDir * rowDist
            fix8 worldX = posX + fix_mul(rayDirX, rowDist);
            fix8 worldY = posY + fix_mul(rayDirY, rowDist);

            int16_t sample_mx = FROM_FIX(worldX);
            int16_t sample_my = FROM_FIX(worldY);

            // clamp
            if (sample_mx < 0) sample_mx = 0;
            if (sample_my < 0) sample_my = 0;
            if (sample_mx >= MAP_W) sample_mx = MAP_W - 1;
            if (sample_my >= MAP_H) sample_my = MAP_H - 1;

            uint8_t fval = floor_map[sample_my * MAP_W + sample_mx];

            uint8_t floor_tex = (fval < floor_base_count)
                ? floor_texture_base[fval]
                : fval;

            colbuf[r] = floor_tex;
        }

        // Single VRAM write for the vertical block (BG y is 1-based in your code)
        set_bkg_tiles(col, (uint8_t)(floor_start_row + 1), 1, (uint8_t)floor_rows, colbuf);
    }
}




// --------------------------------------
// Overlay HUD (bottom-right)
// --------------------------------------

void overlay_pin_bottom_right() {
    uint8_t x = 160 - 24 - 16;
    uint8_t y = 144 - 8;

    move_metasprite(overlay_metasprites[0], 0, 0, x, y);
}



// DRAW AMMO BAR
void draw_ammo_bar()
{
    uint8_t bars = player_ammo_max / 2;
    uint8_t ammo = player_ammo;

    // bottom-left BG tile position
    uint8_t x = 16;
    uint8_t y = 0;   // BG row near bottom

    for(uint8_t i = 0; i < bars; i++)
    {
        uint8_t tile;
        uint8_t r = (bars - 1) - i;

        if(ammo >= (r*2 + 2)) {
            tile = BULLET_TILE_FULL;
        }
        else if(ammo >= (r*2 + 1)) {
            tile = BULLET_TILE_HALF;
        }
        else {
            tile = BULLET_TILE_EMPTY;
        }

        set_bkg_tiles(x + i, y, 1, 1, &tile);
    }
}



void draw_pickup_bg(uint8_t x, uint8_t y)
{
    // 2×2 tile block for 16×16 pickup
    uint8_t top_row[2] = {
        AMMO_TILE_BASE + 0,
        AMMO_TILE_BASE + 1
    };

    uint8_t bottom_row[2] = {
        AMMO_TILE_BASE + 2,
        AMMO_TILE_BASE + 3
    };

    // top row
    set_bkg_tiles(x,     y,     2, 1, top_row);

    // bottom row
    set_bkg_tiles(x,     y + 1, 2, 1, bottom_row);
}


void draw_items(void)
{
    uint8_t center_col = RENDER_COLS / 2;
    uint8_t floor_y    = SCREEN_H - 1;

    // clear slot
    uint8_t empty = 1;
    set_bkg_tiles(center_col, floor_y,     1, 1, &empty);
    set_bkg_tiles(center_col, floor_y - 1, 1, 1, &empty);

    uint8_t pxm = FROM_FIX(px);
    uint8_t pym = FROM_FIX(py);

    // 8-way direction from angle
int16_t fx = cos_lut[pangle];
int16_t fy = sin_lut[pangle];

// normalize to -1, 0, +1
int8_t sx = (fx > 32) ? 1 : (fx < -32) ? -1 : 0;
int8_t sy = (fy > 32) ? 1 : (fy < -32) ? -1 : 0;

// tile ahead
uint8_t tx1 = pxm + sx;
uint8_t ty1 = pym + sy;

// tile 2 ahead
uint8_t tx2 = pxm + sx * 2;
uint8_t ty2 = pym + sy * 2;

for(uint8_t i = 0; i < PICKUP_MAX; i++) {

    if(!pickups[i].active) continue;

    uint8_t ix = FROM_FIX(pickups[i].x);
    uint8_t iy = FROM_FIX(pickups[i].y);

    // 1 tile away → draw 16x16 BG pickup
    if(ix == tx1 && iy == ty1) {
        draw_pickup_bg(center_col - 1, floor_y - 2);
        return;
    }

    // 2 tiles away → draw small icon
    if(ix == tx2 && iy == ty2) {
        uint8_t tile = PICKUP_ICON_TILE;
        set_bkg_tiles(center_col, floor_y - 1, 1, 1, &tile);
        return;
		}
    }
}





// --------------------------------------
// MAIN
// --------------------------------------

void main() {

    DISPLAY_OFF;
    BGP_REG  = 0b00011011;
    OBP0_REG = 0b11100100;
    OBP1_REG = 0b00011011;

    enemy_init();

    set_bkg_data(0, 22, tiledata);
    set_bkg_data(22, 16, bark32_light);
    set_bkg_data(42, 4, brickl16);
    health_init();
    enemy_init();
    pickup_init();

    pickup_spawn(PICKUP_AMMO, 5, 5);
    pickup_spawn(PICKUP_AMMO, 10, 12);
    pickup_spawn(PICKUP_AMMO, 3, 8);

    set_sprite_data(0, overlay_TILE_COUNT, overlay_tiles);
    set_sprite_palette(0, 1, overlay_palettes);

    set_sprite_data(ZOMBIE_TILE_BASE, ZOMBIE_MID_TILE_COUNT, zombie_sheet_tiles);
    set_sprite_palette(1, 1, zombie_sheet_palettes);

    set_sprite_data(ZOMBIE_NEAR_TILE_BASE, ZOMBIE_NEAR_TILE_COUNT, zombie_near_tiles);
    set_sprite_data(ZOMBIE_FAR_TILE_BASE, ZOMBIE_FAR_TILE_COUNT, zombie_far_tiles);

    set_sprite_palette(2, zombie_attack_PALETTE_COUNT, zombie_attack_palettes);
    set_sprite_data(ZOMBIE_ATTACK_TILE_BASE, zombie_attack_TILE_COUNT, zombie_attack_tiles);

    set_bkg_data(BULLET_TILE_FULL, 1, bullets_tiles);
    set_bkg_data(BULLET_TILE_HALF, 1, bullets2_tiles);

    set_sprite_data(OBJ_TILE_BASE, 4, obj_tiles);
    set_sprite_data(AMMO_TILE_BASE, 4, ammo_tiles);

    set_bkg_data(PICKUP_ICON_TILE, 1, ammo_tiles);
    set_bkg_data(DOOR_TILE_BASE, 4, door_tiles);
	
	//set_bkg_data(FLOOR_TILE_BASE, 4, floor_tiles);


    SHOW_SPRITES;
    SPRITES_8x8;
    SHOW_BKG;
    DISPLAY_ON;

    while (1) {

        // ---------------- LOGIC (NO VRAM) ----------------
        uint8_t k = joypad();

        if (k & J_SELECT) {
            show_map = !show_map;
            waitpadup();
        }

        if (!show_map) {

            if (k & J_LEFT)  pangle = (pangle - 1) & ANGLE_MASK;
            if (k & J_RIGHT) pangle = (pangle + 1) & ANGLE_MASK;

            if (k & J_UP) {
                fix8 nx = px + cos_fix(pangle);
                fix8 ny = py + sin_fix(pangle);
                if (!map[FROM_FIX(ny) * MAP_W + FROM_FIX(nx)]) {
                    px = nx; py = ny;
                }
            }

            if (k & J_DOWN) {
                fix8 nx = px - cos_fix(pangle);
                fix8 ny = py - sin_fix(pangle);
                if (!map[FROM_FIX(ny) * MAP_W + FROM_FIX(nx)]) {
                    px = nx; py = ny;
                }
            }
        }

// --------------------------------------
// OPEN DOOR: remove wall and make walkable
// --------------------------------------
if (k & J_B)
{
    int8_t fx = FROM_FIX(px + cos_fix(pangle));
    int8_t fy = FROM_FIX(py + sin_fix(pangle));

    uint8_t idx = fy * MAP_W + fx;

    // If it's a door tile, delete it
    if (map[idx] == 5)
    {
        map[idx] = 0;   // door becomes empty space
    }
}


        static uint8_t fire_anim = 0;

        if (k & J_A) {
            fire_anim = 3;
        }

        if (fire_anim > 0) {
            fire_anim--;
            set_sprite_tile(0, 1);
        } else {
            set_sprite_tile(0, 0);
        }

        // pickup logic (no VRAM)
        for (uint8_t i = 0; i < PICKUP_MAX; i++) {
            if (!pickups[i].active) continue;

            uint8_t pxm = FROM_FIX(px);
            uint8_t pym = FROM_FIX(py);
			
			pz = TO_FIX(height_map[pym * MAP_W + pxm]);

            uint8_t ix = FROM_FIX(pickups[i].x);
            uint8_t iy = FROM_FIX(pickups[i].y);

            if (pxm == ix && pym == iy) {

                if (pickups[i].type == PICKUP_AMMO) {

                    ammo_reserve += 32;
                    if (ammo_reserve > ammo_reserve_max)
                        ammo_reserve = ammo_reserve_max;

                    if (player_ammo == 0 && ammo_reserve > 0) {
                        uint8_t need = player_ammo_max;
                        if (ammo_reserve >= need) {
                            player_ammo = need;
                            ammo_reserve -= need;
                        } else {
                            player_ammo = ammo_reserve;
                            ammo_reserve = 8;
                        }
                    }
                }

                pickups[i].active = 0;
            }
        }

        enemy_update(px, py, pangle);

        // ---------------- VBLANK + VRAM WRITES ----------------
        wait_vbl_done();

        if (show_map)
            draw_minimap();
        else
            render_frame();          // BG columns

        enemy_draw(px, py, pangle);  // sprites
        draw_ammo_bar();             // HUD BG
        health_draw();               // HUD BG / sprites
        overlay_pin_bottom_right();  // HUD sprites



// AUTO RELOAD
if(player_ammo == 0 && ammo_reserve > 0 && auto_reload_timer == 0) {
    auto_reload_timer = 120;   // 2 seconds
}

if(auto_reload_timer > 0) {
    auto_reload_timer--;

    if(auto_reload_timer == 0) {

        uint8_t need = player_ammo_max;

        if(ammo_reserve >= need) {
            player_ammo = need;
            ammo_reserve -= need;
        } else {
            player_ammo = ammo_reserve;
            ammo_reserve = 0;
        }
    }
}


	draw_items();	
	
    // Reset scene if dead
    if(player_health == 0) {
        px = TO_FIX(4);
        py = TO_FIX(4);
        pangle = 0;
        enemy_init();
        player_health = 100;
		}
    }
}
