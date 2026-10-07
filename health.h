#ifndef HEALTH_H
#define HEALTH_H
extern uint8_t player_ammo;
extern uint8_t player_ammo_max;

#include <stdint.h>

// global player health
extern uint8_t player_health;

// init health system (load tiles)
void health_init(void);

// draw the health bar each frame
void health_draw(void);

// damage the player
void health_damage(uint8_t amount);

#endif
