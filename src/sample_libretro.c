/*
 * sample_libretro.c - Minimal libretro core meant as a starting point for
 * porting emulators to libretro.
 *
 * Based on libretro_hello_world_core by Lightnet (MIT License):
 * https://github.com/Lightnet/libretro_hello_world_core
 * See LICENSE and THIRD_PARTY_NOTICES.md.
 *
 * What it shows:
 *  - Logging through the frontend (with a stderr fallback).
 *  - Running without content (RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME).
 *  - Software rendering to an RGB565 framebuffer.
 *  - Audio output once per frame (silence or a test tone).
 *  - Joypad input with input bitmasks and input descriptors.
 *  - Core options (v2 API with a fallback to the legacy API).
 *  - Save states (versioned, endian-independent).
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <libretro.h>

#include "font.h"

/* Set by the Makefile from `git rev-parse --short HEAD`. */
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

#define CORE_NAME    "Sample"
#define CORE_VERSION "1.0"

/* Video */
#define WIDTH  320
#define HEIGHT 240
#define FPS    60.0

/* Audio: 48000 Hz / 60 fps = 800 stereo frames per video frame. */
#define SAMPLE_RATE             48000
#define AUDIO_FRAMES_PER_FRAME  (SAMPLE_RATE / 60)
#define TEST_TONE_HZ            440
#define TEST_TONE_AMPLITUDE     3000

/* Sprite */
#define SQUARE_SIZE 20

/* Colors (RGB565) */
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF
#define COLOR_RED    0xF800
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_GRAY   0x8410

/* Save state layout: 5 little-endian uint32 values. */
#define STATE_MAGIC   0x4C504D53u /* "SMPL" */
#define STATE_VERSION 1u
#define STATE_SIZE    (5 * 4)

/* ------------------------------------------------------------------------ */
/* Frontend callbacks                                                        */
/* ------------------------------------------------------------------------ */

static void RETRO_CALLCONV fallback_log(enum retro_log_level level, const char *fmt, ...);

static retro_environment_t        environ_cb;
static retro_video_refresh_t      video_cb;
static retro_audio_sample_t       audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t         input_poll_cb;
static retro_input_state_t        input_state_cb;
static retro_log_printf_t         log_cb = fallback_log;

static bool libretro_supports_bitmasks = false;

/* ------------------------------------------------------------------------ */
/* Core state                                                                */
/* ------------------------------------------------------------------------ */

static uint16_t framebuffer[WIDTH * HEIGHT];
static int16_t  audio_buffer[AUDIO_FRAMES_PER_FRAME * 2];

/* Emulated state: everything here goes into save states. */
static int32_t  square_x;
static int32_t  square_y;
static uint32_t frame_count;

/* Settings: they come from core options, so they are not saved in states. */
static uint16_t square_color = COLOR_RED;
static int      square_speed = 1;
static bool     test_tone    = false;
static unsigned tone_phase;

/* Used when the frontend has no log interface. Messages already end in '\n'. */
static void RETRO_CALLCONV fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   va_list va;
   (void)level;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

static void reset_state(void)
{
   square_x    = (WIDTH  - SQUARE_SIZE) / 2;
   square_y    = (HEIGHT - SQUARE_SIZE) / 2;
   frame_count = 0;
   tone_phase  = 0;
}

/* ------------------------------------------------------------------------ */
/* Core options                                                              */
/* ------------------------------------------------------------------------ */

static struct retro_core_option_v2_category option_cats[] = {
   { NULL, NULL, NULL },
};

static struct retro_core_option_v2_definition option_defs[] = {
   {
      "sample_square_color",
      "Square Color",
      NULL,
      "Color of the square controlled with the D-Pad.",
      NULL,
      NULL,
      {
         { "red",    "Red" },
         { "green",  "Green" },
         { "blue",   "Blue" },
         { "yellow", "Yellow" },
         { "white",  "White" },
         { NULL, NULL },
      },
      "red"
   },
   {
      "sample_square_speed",
      "Square Speed",
      NULL,
      "Pixels the square moves per frame.",
      NULL,
      NULL,
      {
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { NULL, NULL },
      },
      "1"
   },
   {
      "sample_test_tone",
      "Test Tone",
      NULL,
      "Play a 440 Hz tone to check audio output. When disabled, the core outputs silence.",
      NULL,
      NULL,
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   { NULL, NULL, NULL, NULL, NULL, NULL, { { NULL, NULL } }, NULL },
};

static struct retro_core_options_v2 options_v2 = {
   option_cats,
   option_defs
};

/* For frontends that only support the legacy API. The first value is the default. */
static const struct retro_variable options_legacy[] = {
   { "sample_square_color", "Square Color; red|green|blue|yellow|white" },
   { "sample_square_speed", "Square Speed; 1|2|3|4" },
   { "sample_test_tone",    "Test Tone; disabled|enabled" },
   { NULL, NULL },
};

static void set_core_options(void)
{
   unsigned version = 0;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version >= 2)
   {
      if (environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_v2))
         return;
   }

   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)options_legacy);
}

static const char *get_option(const char *key)
{
   struct retro_variable var;
   var.key   = key;
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var))
      return var.value;
   return NULL;
}

static void check_variables(void)
{
   const char *value;

   value = get_option("sample_square_color");
   if (value)
   {
      if      (!strcmp(value, "green"))  square_color = COLOR_GREEN;
      else if (!strcmp(value, "blue"))   square_color = COLOR_BLUE;
      else if (!strcmp(value, "yellow")) square_color = COLOR_YELLOW;
      else if (!strcmp(value, "white"))  square_color = COLOR_WHITE;
      else                               square_color = COLOR_RED;
   }

   value = get_option("sample_square_speed");
   if (value)
   {
      square_speed = value[0] - '0';
      if (square_speed < 1 || square_speed > 4)
         square_speed = 1;
   }

   value = get_option("sample_test_tone");
   if (value)
      test_tone = !strcmp(value, "enabled");
}

/* ------------------------------------------------------------------------ */
/* Rendering                                                                 */
/* ------------------------------------------------------------------------ */

static void clear_framebuffer(void)
{
   memset(framebuffer, 0, sizeof(framebuffer));
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
   int px, py;
   for (py = y; py < y + h; py++)
   {
      if (py < 0 || py >= HEIGHT)
         continue;
      for (px = x; px < x + w; px++)
      {
         if (px >= 0 && px < WIDTH)
            framebuffer[py * WIDTH + px] = color;
      }
   }
}

/* Draw one 8x8 character. Characters outside ASCII 32-126 are drawn as '?'. */
static void draw_char(int x, int y, unsigned char c, uint16_t color)
{
   const uint8_t *glyph;
   int gx, gy;

   if (c < 32 || c > 126)
      c = '?';

   glyph = font_8x8[c - 32];
   for (gy = 0; gy < 8; gy++)
   {
      for (gx = 0; gx < 8; gx++)
      {
         int px = x + gx;
         int py = y + gy;
         if ((glyph[gy] & (0x80 >> gx)) && px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
            framebuffer[py * WIDTH + px] = color;
      }
   }
}

static void draw_string(int x, int y, const char *str, uint16_t color)
{
   for (; *str; str++, x += 8)
      draw_char(x, y, (unsigned char)*str, color);
}

static void render_frame(void)
{
   char line[41];

   clear_framebuffer();
   fill_rect(square_x, square_y, SQUARE_SIZE, SQUARE_SIZE, square_color);

   draw_string(8, 8, "Hello World", COLOR_WHITE);
   draw_string(8, HEIGHT - 24, "D-Pad: move   Start: center", COLOR_GRAY);
   snprintf(line, sizeof(line), "Frame %lu", (unsigned long)frame_count);
   draw_string(8, HEIGHT - 12, line, COLOR_GRAY);
}

/* ------------------------------------------------------------------------ */
/* Audio                                                                     */
/* ------------------------------------------------------------------------ */

/* Fill one video frame of audio: a square wave when the test tone is on, else silence. */
static void render_audio(void)
{
   int i;

   for (i = 0; i < AUDIO_FRAMES_PER_FRAME; i++)
   {
      int16_t sample = 0;
      if (test_tone)
      {
         sample = (tone_phase < SAMPLE_RATE / 2) ? TEST_TONE_AMPLITUDE : -TEST_TONE_AMPLITUDE;
         tone_phase += TEST_TONE_HZ;
         if (tone_phase >= SAMPLE_RATE)
            tone_phase -= SAMPLE_RATE;
      }
      audio_buffer[i * 2 + 0] = sample; /* left */
      audio_buffer[i * 2 + 1] = sample; /* right */
   }

   audio_batch_cb(audio_buffer, AUDIO_FRAMES_PER_FRAME);
}

/* ------------------------------------------------------------------------ */
/* Input                                                                     */
/* ------------------------------------------------------------------------ */

static uint16_t read_joypad(unsigned port)
{
   uint16_t buttons = 0;
   unsigned id;

   if (libretro_supports_bitmasks)
      return (uint16_t)input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);

   for (id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
   {
      if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id))
         buttons |= (uint16_t)(1 << id);
   }
   return buttons;
}

static void set_input_descriptors(void)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Center square" },
      { 0, 0, 0, 0, NULL },
   };
   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

#define PRESSED(mask, id) ((mask) & (1 << (id)))

static void update_state(uint16_t buttons)
{
   if (PRESSED(buttons, RETRO_DEVICE_ID_JOYPAD_START))
   {
      square_x = (WIDTH  - SQUARE_SIZE) / 2;
      square_y = (HEIGHT - SQUARE_SIZE) / 2;
   }

   if (PRESSED(buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT)) square_x += square_speed;
   if (PRESSED(buttons, RETRO_DEVICE_ID_JOYPAD_LEFT))  square_x -= square_speed;
   if (PRESSED(buttons, RETRO_DEVICE_ID_JOYPAD_DOWN))  square_y += square_speed;
   if (PRESSED(buttons, RETRO_DEVICE_ID_JOYPAD_UP))    square_y -= square_speed;

   if (square_x < 0) square_x = 0;
   if (square_y < 0) square_y = 0;
   if (square_x > WIDTH  - SQUARE_SIZE) square_x = WIDTH  - SQUARE_SIZE;
   if (square_y > HEIGHT - SQUARE_SIZE) square_y = HEIGHT - SQUARE_SIZE;

   frame_count++;
}

/* ------------------------------------------------------------------------ */
/* libretro API: setup                                                       */
/* ------------------------------------------------------------------------ */

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

/* May be called before retro_init and more than once. Core options must be set here. */
void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback logging;
   bool no_game = true;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging) && logging.log)
      log_cb = logging.log;
   else
      log_cb = fallback_log;

   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
   set_core_options();
}

void retro_set_video_refresh(retro_video_refresh_t cb)          { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)            { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb)                { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb)              { input_state_cb = cb; }

/* May be called without retro_set_environment (for example, when the frontend
 * scans its cores). It must not use callbacks or have side effects. */
void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = CORE_NAME;
   info->library_version  = CORE_VERSION GIT_VERSION;
   info->valid_extensions = "";
   info->need_fullpath    = false;
   info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width   = WIDTH;
   info->geometry.base_height  = HEIGHT;
   info->geometry.max_width    = WIDTH;
   info->geometry.max_height   = HEIGHT;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
   info->timing.fps            = FPS;
   info->timing.sample_rate    = SAMPLE_RATE;
}

void retro_init(void)
{
   libretro_supports_bitmasks = environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL);
   reset_state();
   log_cb(RETRO_LOG_DEBUG, "[" CORE_NAME "] Initialized\n");
}

void retro_deinit(void)
{
   libretro_supports_bitmasks = false;
   log_cb(RETRO_LOG_DEBUG, "[" CORE_NAME "] Deinitialized\n");
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   log_cb(RETRO_LOG_DEBUG, "[" CORE_NAME "] Port %u: device %u\n", port, device);
}

/* ------------------------------------------------------------------------ */
/* libretro API: content                                                     */
/* ------------------------------------------------------------------------ */

/* `game` is NULL when started without content. An emulator would load the
 * ROM here from game->data/game->size (need_fullpath = false) or from
 * game->path (need_fullpath = true). */
bool retro_load_game(const struct retro_game_info *game)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   (void)game;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_ERROR, "[" CORE_NAME "] RGB565 is not supported by the frontend\n");
      return false;
   }

   set_input_descriptors();
   check_variables();
   reset_state();

   log_cb(RETRO_LOG_INFO, "[" CORE_NAME "] Started %s\n", game ? "with content" : "without content");
   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

/* ------------------------------------------------------------------------ */
/* libretro API: execution                                                   */
/* ------------------------------------------------------------------------ */

void retro_reset(void)
{
   reset_state();
}

void retro_run(void)
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();

   input_poll_cb();
   update_state(read_joypad(0));

   render_frame();
   render_audio();
   video_cb(framebuffer, WIDTH, HEIGHT, WIDTH * sizeof(uint16_t));
}

/* ------------------------------------------------------------------------ */
/* libretro API: save states                                                 */
/* ------------------------------------------------------------------------ */

static void write_u32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v);
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static uint32_t read_u32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Must return the same size for the whole session, even before content is loaded. */
size_t retro_serialize_size(void)
{
   return STATE_SIZE;
}

bool retro_serialize(void *data, size_t size)
{
   uint8_t *p = (uint8_t *)data;

   if (size < STATE_SIZE)
      return false;

   write_u32(p +  0, STATE_MAGIC);
   write_u32(p +  4, STATE_VERSION);
   write_u32(p +  8, (uint32_t)square_x);
   write_u32(p + 12, (uint32_t)square_y);
   write_u32(p + 16, frame_count);
   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   const uint8_t *p = (const uint8_t *)data;
   int32_t x, y;

   if (size < STATE_SIZE || read_u32(p) != STATE_MAGIC || read_u32(p + 4) != STATE_VERSION)
      return false;

   x = (int32_t)read_u32(p + 8);
   y = (int32_t)read_u32(p + 12);
   if (x < 0 || x > WIDTH - SQUARE_SIZE || y < 0 || y > HEIGHT - SQUARE_SIZE)
      return false;

   square_x    = x;
   square_y    = y;
   frame_count = read_u32(p + 16);
   return true;
}

/* ------------------------------------------------------------------------ */
/* libretro API: cheats and memory                                           */
/* ------------------------------------------------------------------------ */

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

/* An emulator returns its battery-backed RAM here (RETRO_MEMORY_SAVE_RAM) so
 * the frontend saves the .srm file, and its main RAM (RETRO_MEMORY_SYSTEM_RAM)
 * for cheats and achievements. */
void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}
