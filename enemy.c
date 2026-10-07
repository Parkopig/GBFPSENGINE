#include <gb/gb.h>
#include <stdint.h>
#include <stdlib.h>

#include "overlay.h"
#include "enemy.h"
#include "fixmath.h"
#include "zombie_sheet.h"
#include "zombie_far.h"
#include "zombie_attack.h"

#include "health.h"


#define ZOMBIE_ATTACK_TILE_BASE (ZOMBIE_TILE_BASE + 64)

// Ammo variables from main.c
extern uint8_t player_ammo;
extern uint8_t player_ammo_max;
extern uint8_t ammo_reserve;


// NEAR zombie symbols from zombie_sheet.c
extern const metasprite_t* const zombie_near_metasprites[1];
extern const uint8_t zombie_near_tiles[];
extern const palette_color_t zombie_near_palettes[4];

extern const metasprite_t* const zombie_far_metasprites[1];
extern const uint8_t zombie_far_tiles[];
extern const palette_color_t zombie_far_palettes[4];

//#include "zombie_near.h"

// Map from main.c
extern const uint8_t map[];
#define MAP_W 16
#define MAP_H 16

Enemy enemies[ENEMY_MAX];

#define CHASE_RANGE   20
#define ATTACK_RANGE  4
#define WANDER_TIME   60

uint8_t wander_timer = 0;

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------

static uint16_t dist2(fix8 ax, fix8 ay, fix8 bx, fix8 by) {
    int16_t dx = FROM_FIX(ax) - FROM_FIX(bx);
    int16_t dy = FROM_FIX(ay) - FROM_FIX(by);
    return dx*dx + dy*dy;
}

static void enemy_face_player(Enemy* e, fix8 px, fix8 py) {
    int16_t dx = FROM_FIX(px) - FROM_FIX(e->x);
    int16_t dy = FROM_FIX(py) - FROM_FIX(e->y);

    if(abs(dx) > abs(dy))
        e->dir = (dx > 0) ? DIR_E : DIR_W;
    else
        e->dir = (dy > 0) ? DIR_S : DIR_N;
}

static void enemy_step(Enemy* e) {
    fix8 nx = e->x;
    fix8 ny = e->y;

    switch(e->dir) {
        case DIR_N: ny -= TO_FIX(1)/4; break;
        case DIR_S: ny += TO_FIX(1)/4; break;
        case DIR_E: nx += TO_FIX(1)/4; break;
        case DIR_W: nx -= TO_FIX(1)/4; break;
    }

    uint8_t mx = FROM_FIX(nx);
    uint8_t my = FROM_FIX(ny);

    if(!map[my * MAP_W + mx]) {
        e->x = nx;
        e->y = ny;
    }
}

// ---------------------------------------------------------
// Init
// ---------------------------------------------------------

void enemy_init() {
    for(uint8_t i = 0; i < ENEMY_MAX; i++) {
        enemies[i].active = 0;
        enemies[i].health = 30;
		enemies[i].stumble_timer = 0;
        enemies[i].blink = 0;
        enemies[i].sprite_id = 17 + (i * 24);
    }

    // Spawn enemy 0
    enemies[0].active = 1;
    enemies[0].x = TO_FIX(14);
    enemies[0].y = TO_FIX(14);

    // STORE SPAWN POSITION
    enemies[0].spawn_x = enemies[0].x;
    enemies[0].spawn_y = enemies[0].y;

    enemies[0].dir = DIR_S;
    enemies[0].state = ENEMY_IDLE;
}




// ---------------------------------------------------------
// Update
// ---------------------------------------------------------

void enemy_update(fix8 px, fix8 py, uint8_t pangle)
{
    wander_timer++;

    for(uint8_t i = 0; i < ENEMY_MAX; i++) {
        Enemy* e = &enemies[i];
        if(!e->active) continue;

        uint16_t d = dist2(e->x, e->y, px, py);

        int16_t dx = FROM_FIX(e->x) - FROM_FIX(px);
        int16_t dy = FROM_FIX(e->y) - FROM_FIX(py);



// --------------------------------------
// LINE OF SIGHT CHECK
// --------------------------------------
uint8_t los_blocked = 0;

fix8 rx = px;
fix8 ry = py;

for(uint8_t s = 0; s < 24; s++) {
    rx += (dx << 6) / 4;
    ry += (dy << 6) / 4;

    int8_t mx2 = FROM_FIX(rx);
    int8_t my2 = FROM_FIX(ry);

    if(mx2 < 0 || my2 < 0 || mx2 >= MAP_W || my2 >= MAP_H) {
        los_blocked = 1;
        break;
    }

    if(map[my2 * MAP_W + mx2] != 0) {
        los_blocked = 1;
        break;
    }
}

// --------------------------------------
// SHOOTING + INSTANT RELOAD WHEN EMPTY
// --------------------------------------
if(joypad() & J_A) {

    // If empty, reload immediately from reserve
    if(player_ammo == 0) {

        if(ammo_reserve > 0) {

            uint8_t need = player_ammo_max;

            if(ammo_reserve >= need) {
                player_ammo = need;
                ammo_reserve -= need;
            } else {
                player_ammo = ammo_reserve;
                ammo_reserve = 0;
            }

        } else {
            // no reserve → cannot shoot
            goto after_attack;
        }
    }

    // consume one bullet
    player_ammo--;


            int16_t fx = cos_lut[pangle & ANGLE_MASK];
            int16_t fy = sin_lut[pangle & ANGLE_MASK];

            int16_t front = dx * fx + dy * fy;
            int16_t side  = dx * fy - dy * fx;

            if(front > 0) {
                side = side * 8;
                int16_t sx = 80 - (side / 8);

                if(sx > 72 && sx < 88) {   // centered window

                    if(e->health > 0) {
                        uint8_t dmg = (rand() % 6) + 3;
                        e->health -= dmg;

                        e->blink = 6;
						e->stumble_timer = 12;   // half-second stumble


// --------------------------------------
// DEATH CHECK — RESPAWN AT SPAWN POINT
// --------------------------------------
if(e->health <= 0) {

    // reset health
    e->health = 30;

    // reset state
    e->attacking = 0;
    e->attack_timer = 0;
    e->attack_anim_toggle = 0;
    e->attack_anim_counter = 0;

    // teleport back to spawn
    e->x = e->spawn_x;
    e->y = e->spawn_y;

    // REACTIVATE ENEMY
    e->active = 1;

    // clear death flag
    e->death_pending = 0;

    continue;
}


                    }
                }
            }
        }

after_attack:

if(e->health == 0) {
    e->attacking = 0;
    continue;
}


        // --------------------------------------
        // PLAYER DAMAGE WHEN VERY CLOSE
        // --------------------------------------
        if(dx < 2 && dx > -2 && dy < 2 && dy > -2) {
            if(player_health > 0) player_health--;

            e->attacking = 1;
            e->attack_timer = 10;
        }

        // --------------------------------------
        // ATTACK STATE
        // --------------------------------------
        if(d < ATTACK_RANGE) {

            e->attack_anim_counter++;
            if(e->attack_anim_counter >= 4) {
                e->attack_anim_counter = 0;
                e->attack_anim_toggle ^= 1;
            }

            e->attacking = 1;
            e->attack_timer = 10;

            e->state = ENEMY_ATTACK;
            continue;
        }
// --------------------------------------
// STUMBLE BEHAVIOR
// --------------------------------------
if(e->stumble_timer > 0) {
    e->stumble_timer--;

    // stop attacking while stumbling
    e->attacking = 0;
    e->attack_timer = 0;

    // do NOT move
    continue;
}

        // --------------------------------------
        // LEAVING ATTACK RANGE
        // --------------------------------------
        e->attacking = 0;
        e->attack_anim_toggle = 0;
        e->attack_anim_counter = 0;

        // --------------------------------------
        // CHASE
        // --------------------------------------
        if(d < CHASE_RANGE) {
            e->state = ENEMY_CHASE;
            enemy_face_player(e, px, py);
            enemy_step(e);
            continue;
        }

        // --------------------------------------
        // WANDER
        // --------------------------------------
        e->state = ENEMY_WANDER;

        if(wander_timer > WANDER_TIME) {
            e->dir = (e->dir + 1) & 3;
            wander_timer = 0;
        }

        enemy_step(e);
    }
}



// ---------------------------------------------------------
// Render helpers
// ---------------------------------------------------------

// Hide 24 sprites (NEAR uses 24)
static void enemy_hide_sprites(Enemy* e) {
    for(uint8_t i = 0; i < 24; i++)
        move_sprite(e->sprite_id + i, 200, 200);
}



// BASE zombie (32×32 → 4×4 → 8 sprites)
void draw_zombie(uint8_t base_tile, uint8_t x, uint8_t y, uint8_t sprite_id) {

    uint8_t idx = 0;

    for(uint8_t row = 0; row < 4; row++) {
        for(uint8_t col = 0; col < 2; col++) {

            uint8_t tile = base_tile + zombie_sheet_map[idx++];

            move_sprite(sprite_id, x + col*8, y + row*8);
            set_sprite_tile(sprite_id, tile);

            sprite_id++;
        }
    }
}

// NEAR zombie (32×40 → 4×5 → 10 sprites)
void draw_zombie_near(uint8_t base_tile, uint8_t x, uint8_t y, uint8_t sprite_id) {
    move_metasprite(
        zombie_near_metasprites[0],
        base_tile,
        sprite_id,
        x,
        y
    );
}

// FAR zombie (16×16 → 2×2 → 4 sprites)
void draw_zombie_far(uint8_t base_tile, uint8_t x, uint8_t y, uint8_t sprite_id) {
    move_metasprite(
        zombie_far_metasprites[0],
        base_tile,
        sprite_id,
        x,
        y
    );
}




// ---------------------------------------------------------
// Angle tables
// ---------------------------------------------------------

#define ANGLES 32
#define ANGLE_MASK (ANGLES - 1)

extern const int8_t cos_lut[ANGLES];
extern const int8_t sin_lut[ANGLES];

// ---------------------------------------------------------
// Draw enemy
// ---------------------------------------------------------

void enemy_draw(fix8 px, fix8 py, uint8_t pangle)
{
    uint8_t ang = pangle & ANGLE_MASK;

    for(uint8_t i = 0; i < ENEMY_MAX; i++)
    {
        Enemy* e = &enemies[i];
        if(!e->active) continue;

        int16_t dx = FROM_FIX(e->x) - FROM_FIX(px);
        int16_t dy = FROM_FIX(e->y) - FROM_FIX(py);

        // --------------------------------------
        // WALL BLOCK CHECK (line-of-sight)
        // --------------------------------------
        fix8 rx2 = px;
        fix8 ry2 = py;

        uint8_t blocked = 0;

        for(uint8_t s = 0; s < 24; s++)
        {
            rx2 += (dx << 6) / 4;
            ry2 += (dy << 6) / 4;

            int8_t mx2 = FROM_FIX(rx2);
            int8_t my2 = FROM_FIX(ry2);

            if(mx2 < 0 || my2 < 0 || mx2 >= MAP_W || my2 >= MAP_H) {
                blocked = 1;
                break;
            }

            if(map[my2 * MAP_W + mx2] != 0) {
                blocked = 1;
                break;
            }
        }

        if(blocked) {
            enemy_hide_sprites(e);
            continue;
        }
		
		if(e->health == 0) {
		enemy_hide_sprites(e);
		continue;
		}

	
// --------------------------------------
// BLINK EFFECT WHEN HIT
// --------------------------------------
uint8_t blink_hide = 0;
if(e->blink > 0) {
    if(e->blink & 1) {
        blink_hide = 1;   // hide every other frame
    }
    e->blink--;
}

if(blink_hide) {
    enemy_hide_sprites(e);
    continue;
}


        // --------------------------------------
        // DISTANCE + PROJECTION
        // --------------------------------------
        int16_t adx = dx < 0 ? -dx : dx;
        int16_t ady = dy < 0 ? -dy : dy;
        uint16_t d  = (adx > ady) ? adx : ady;
        uint16_t d2 = d * d;

        if(d2 > 200) {
            enemy_hide_sprites(e);
            continue;
        }

        int16_t fx = cos_lut[ang];
        int16_t fy = sin_lut[ang];

        int16_t front = dx * fx + dy * fy;
        int16_t side  = dx * fy - dy * fx;

        if(front < 0) {
            enemy_hide_sprites(e);
            continue;
        }

        int16_t sy = 136 - (16 * (d2 / 4));

        side = side * 8;
        int16_t sx = 80 - (side / 8);

        if(sx < -16) sx = -16;
        if(sx > 176) sx = 176;
        if(sy < 0)   sy = 0;
        if(sy > 144) sy = 144;

        enemy_hide_sprites(e);

// --------------------------------------
// DEATH CHECK — RETURN TO SPAWN + RESPAWN
// --------------------------------------
if(e->health <= 0) {

    // reset health
    e->health = 30;

    // reset state
    e->attacking = 0;
    e->attack_timer = 0;
    e->attack_anim_toggle = 0;
    e->attack_anim_counter = 0;

    // teleport back to spawn
    e->x = e->spawn_x;
    e->y = e->spawn_y;

    // REACTIVATE ENEMY
    e->active = 1;

    // clear death flag
    e->death_pending = 0;

    continue;
}


// --------------------------------------
// ATTACK LOOP: NEAR ↔ ATTACK ↔ NEAR ↔ ATTACK
// --------------------------------------
if(e->attacking) {

    if(e->attack_anim_toggle == 0) {
        // draw NEAR frame
        draw_zombie_near(
            ZOMBIE_NEAR_TILE_BASE,
            (uint8_t)sx,
            (uint8_t)sy,
            e->sprite_id
        );
    }
    else {
        // draw ATTACK frame
        move_metasprite(
            zombie_attack_metasprites[0],
            ZOMBIE_ATTACK_TILE_BASE,
            e->sprite_id,
            (uint8_t)sx,
            (uint8_t)sy
        );
    }

    continue;   // skip MID/FAR
}

        // --------------------------------------
        // NEAR
        // --------------------------------------
        if(d2 <= 2) {
            draw_zombie_near(
                ZOMBIE_NEAR_TILE_BASE,
                (uint8_t)sx,
                (uint8_t)sy,
                e->sprite_id
            );
            continue;
        }

        // --------------------------------------
        // MID
        // --------------------------------------
        if(d2 <= 10) {
            draw_zombie(
                ZOMBIE_TILE_BASE,
                (uint8_t)sx,
                (uint8_t)sy,
                e->sprite_id
            );
            continue;
        }

        // --------------------------------------
        // FAR
        // --------------------------------------
        if (sy < 108) sy = 108;

        draw_zombie_far(
            ZOMBIE_FAR_TILE_BASE,
            (uint8_t)sx,
            (uint8_t)sy,
            e->sprite_id
        );
    }
}






