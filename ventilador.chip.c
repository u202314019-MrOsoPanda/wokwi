// Ventilador de FreshSense.
// Es una pieza propia de Wokwi: el circulo de aspas se dibuja en pantalla.
// El pin IN llega del mismo GPIO 26 que el rele. Si IN esta en alto,
// las aspas giran. En la placa fisica el motor es un ventilador de verdad
// y el rele es el que le da corriente.

#include "wokwi-api.h"
#include <math.h>
#include <stdlib.h>

#define TWO_PI 6.2831853f

typedef struct {
  pin_t in_pin;
  buffer_t framebuffer;
  uint32_t width;
  uint32_t height;
  uint8_t *pixels;
  float angle;
} chip_state_t;

static void paint(chip_state_t *chip) {
  uint32_t width = chip->width;
  uint32_t height = chip->height;
  float center_x = (width - 1) / 2.0f;
  float center_y = (height - 1) / 2.0f;
  float radius = (width < height ? width : height) * 0.46f;
  float hub = radius * 0.18f;

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      float dx = (float)x - center_x;
      float dy = (float)y - center_y;
      float distance = sqrtf(dx * dx + dy * dy);
      uint8_t red = 0;
      uint8_t green = 0;
      uint8_t blue = 0;
      uint8_t alpha = 0;

      if (distance <= radius) {
        alpha = 255;
        red = 36;
        green = 44;
        blue = 54;

        float angle = atan2f(dy, dx) - chip->angle;
        float sector = fmodf(angle + TWO_PI * 2.0f, TWO_PI / 3.0f);
        if (distance > hub && sector < TWO_PI / 16.0f) {
          red = 226;
          green = 234;
          blue = 242;
        }
        if (distance <= hub) {
          red = 24;
          green = 92;
          blue = 176;
        }
        if (distance > radius - 3.0f) {
          red = 190;
          green = 198;
          blue = 206;
        }
      }

      uint32_t index = (y * width + x) * 4;
      chip->pixels[index] = red;
      chip->pixels[index + 1] = green;
      chip->pixels[index + 2] = blue;
      chip->pixels[index + 3] = alpha;
    }
  }

  buffer_write(chip->framebuffer, 0, chip->pixels, width * height * 4);
}

static void on_timer(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (pin_read(chip->in_pin) == HIGH) {
    chip->angle += 0.5f;
    if (chip->angle > TWO_PI) {
      chip->angle -= TWO_PI;
    }
  }
  paint(chip);
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->in_pin = pin_init("IN", INPUT_PULLDOWN);
  chip->angle = 0.4f;
  chip->framebuffer = framebuffer_init(&chip->width, &chip->height);
  chip->pixels = malloc(chip->width * chip->height * 4);

  timer_config_t timer_config = {
      .callback = on_timer,
      .user_data = chip,
  };
  timer_t timer = timer_init(&timer_config);
  timer_start(timer, 40000, true);
  paint(chip);
}
