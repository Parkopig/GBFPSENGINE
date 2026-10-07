#ifndef FIXMATH_H
#define FIXMATH_H
#define ANGLES 32
#define ANGLE_MASK (ANGLES - 1)

extern const int8_t cos_lut[ANGLES];
extern const int8_t sin_lut[ANGLES];


#include <stdint.h>

typedef int16_t fix8;

#define FIX_SHIFT   8
#define TO_FIX(x)   ((fix8)((x) << FIX_SHIFT))
#define FROM_FIX(x) ((x) >> FIX_SHIFT)

extern const int8_t cos_lut[32];
extern const int8_t sin_lut[32];

static inline fix8 cos_fix(uint8_t a) { return (fix8)cos_lut[a] << 2; }
static inline fix8 sin_fix(uint8_t a) { return (fix8)sin_lut[a] << 2; }

#endif
