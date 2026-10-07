#ifndef ENEMY_H
#define ENEMY_H

#include <gb/gb.h>
#include <stdint.h>
#include "fixmath.h"   // your fix8, TO_FIX, FROM_FIX, cos_fix, sin_fix

#define ENEMY_MAX 4
#define ZOMBIE_TILE_BASE        overlay_TILE_COUNT   // MID starts after overlay
#define ZOMBIE_MID_TILE_COUNT   8                   // zombie_sheet_tiles = 8 tiles

#define ZOMBIE_NEAR_TILE_BASE   (ZOMBIE_TILE_BASE + ZOMBIE_MID_TILE_COUNT)
#define ZOMBIE_NEAR_TILE_COUNT  24                  // zombie_near_tiles = 24 tiles

#define ZOMBIE_FAR_TILE_BASE    (ZOMBIE_NEAR_TILE_BASE + ZOMBIE_NEAR_TILE_COUNT)
#define ZOMBIE_FAR_TILE_COUNT   4                   // zombie_far_tiles = 4 tiles




// Directions
#define DIR_N 0
#define DIR_E 1
#define DIR_S 2
#define DIR_W 3

// States
#define ENEMY_IDLE     0
#define ENEMY_WANDER   1
#define ENEMY_CHASE    2
#define ENEMY_ATTACK   3


typedef struct Enemy {
    fix8 x, y;
    uint8_t dir;
    uint8_t state;
    uint8_t sprite_id;
    uint8_t active;

    uint8_t health;
    uint8_t blink;
	uint8_t death_pending;
	uint8_t stumble_timer;

	
	fix8 spawn_x;
	fix8 spawn_y;


    uint8_t attacking;
    uint8_t attack_timer;
    uint8_t attack_anim_toggle;
    uint8_t attack_anim_counter;

} Enemy;

extern Enemy enemies[ENEMY_MAX];

void enemy_init(void);
void enemy_update(fix8 px, fix8 py, uint8_t pangle);
void enemy_draw(fix8 px, fix8 py, uint8_t pangle);

#endif
