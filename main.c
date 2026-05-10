#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef _Bool bool;
#define false 0
#define true 1

static inline bool key_available(void) {
  volatile const u8 *key_avail = (void *)0x8000;
  return *key_avail;
}
static inline u8 get_key(void) {
  volatile const u8 *key = (void *)0x8001;
  return *key;
}
static inline void write_pixel(u8 color) {
  volatile u8 *pixel = (void *)0x8000;
  *pixel = color;
}
static inline void wait_for_next_frame(void) {
  volatile u8 *key = (void *)0x8001;
  *key = 0;
}
static inline void debug(u8 c) {
  volatile u8 *dbg = (void *)0x8007;
  *dbg = c;
}

u16 random_seed = 0x9f63;
static u16 random_u16(void) {
  u16 x = random_seed;
  x ^= x << 7;
  x ^= x >> 9;
  x ^= x << 3;
  return random_seed = x;
}

#define Y_SHIFT 4
#define COORD(x, y) ((y) << Y_SHIFT | (x))
#define COORD_X(coord) (((u8)(coord)) & (1 << Y_SHIFT) - 1)
#define COORD_Y(coord) (((u8)(coord)) >> Y_SHIFT)

enum Color {
  COLOR_BLACK = 0,
  COLOR_WHITE = 1,
  COLOR_GREEN = 2,
  COLOR_RED = 3,
};

#define MAX_NUM_APPLES 4
u8 num_apples = 2;
u8 apple_coords[MAX_NUM_APPLES];

bool dead = false;

const i8 DIR[4] = {-COORD(1, 0), COORD(0, 1), COORD(1, 0), -COORD(0, 1)};
i8 snake_dir = COORD(1, 0); // right

#define INPUT_QUEUE_SIZE 2
i8 input_queue[INPUT_QUEUE_SIZE];
u8 input_queue_pos = 0;

const u8 SNAKE_MOVEMENT_FRAMES = 3;
u8 movement_counter = 0;

// deque
u8 snake_head = 0;
u16 snake_len = 3;
u8 snake_coords[256] = {COORD(3, 1), COORD(2, 1), COORD(1, 1)};
u8 grid_colors[256];

static inline void halt(void) { __asm__("halt"); }

static void render_frame(void) {
  memset(grid_colors, COLOR_BLACK, sizeof(grid_colors));

  // this loop is very inefficient and visibly slows the game down
  // but i can't be bothered to fix it right now (or ever, actually)
  u8 snake_color = COLOR_GREEN;
  for (int i = snake_head, end = snake_head + snake_len; i < end; i++) {
    u8 idx = snake_coords[(u8)i];
    if (grid_colors[idx] != COLOR_BLACK) {
      // snake collided with itself
      dead = true;
    }
    grid_colors[idx] = snake_color;
  }
  for (u8 i = 0; i < num_apples; i++)
    grid_colors[apple_coords[i]] = COLOR_RED;

  if (dead) {
    const u16 PIXELS[5] = {
        0b1100111011000001, 0b1010100010101010, 0b1010110010100010,
        0b1010100010101010, 0b1100111011000001,
    };
    for (int y = 0; y < 5; y++) {
      u16 row = PIXELS[y];
      for (int x = 0; x < 16; x++) {
        if (row >> (15 - x) & 1)
          grid_colors[COORD(x, y + 6)] = COLOR_WHITE;
      }
    }
  }

  for (u16 i = 0; i < 256; i++)
    write_pixel(grid_colors[i]);
}

static void handle_key_events(void) {
  if (!key_available())
    return;
  // insert new_dir at position 0
  if (input_queue_pos == INPUT_QUEUE_SIZE)
    input_queue_pos--;
  for (int i = input_queue_pos; i > 0; i--)
    input_queue[i] = input_queue[i - 1];
  input_queue_pos++;
  i8 direction = DIR[get_key() & 3];
  input_queue[0] = direction;
  // seed rng
  random_seed ^= direction;
  random_u16();
  random_u16();
  random_u16();
}

static void tick_snake(void) {
  if (input_queue_pos) {
    i8 new_dir = input_queue[--input_queue_pos];
    if (new_dir != -snake_dir)
      snake_dir = new_dir;
  }

  u8 head_coord = snake_coords[snake_head];
  // sign-extend snake_dir
  i16 direction = snake_dir;
  u16 new_coord = head_coord + direction;
  // bounds checks
  if ((snake_dir & 1) != 0) {
    // direction in x, if y values are different then we overflowed
    if (COORD_Y(new_coord) != COORD_Y(head_coord))
      dead = true;
  } else {
    // direction in y, straightforward overflow check
    if (new_coord & 0xff00)
      dead = true;
  }
  if (dead)
    return;

  // since everything is a u8, we get wrapping arithmetic for free
  snake_head--;
  snake_coords[snake_head] = new_coord;

  for (int i = 0; i < num_apples; i++) {
    if (apple_coords[i] == new_coord) {
      // ate apple!
      snake_len++;
      // use previous frame's grid to place new apple; this is not perfect
      // but it works well enough
      u8 new_apple_coord;
      do {
        new_apple_coord = random_u16();
      } while (grid_colors[new_apple_coord] != COLOR_BLACK);
      apple_coords[i] = new_apple_coord;
    }
  }
}

int main(void) {
  memset(grid_colors, COLOR_BLACK, sizeof(grid_colors));
  for (int i = 0; i < num_apples; i++)
    apple_coords[i] = random_u16();

  while (1) {
    handle_key_events();
    if (!dead) {
      if (++movement_counter == SNAKE_MOVEMENT_FRAMES) {
        movement_counter = 0;
        tick_snake();
      }
    }
    render_frame();
    wait_for_next_frame();
  }
}
