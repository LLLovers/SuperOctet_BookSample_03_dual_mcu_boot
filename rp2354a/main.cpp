#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware_config.h"

#include "board/k210_boot.hpp"

static void blink(uint pin, uint half_period_ms) {
  bool level = false;
  for (;;) {
    gpio_put(pin, level = !level);
    busy_wait_ms(half_period_ms);
  }
}

static void core1_main() {
  blink(PIN_ACTIVITY_CORE1, 650);
}

int main() {
  gpio_init(PIN_ACTIVITY_CORE0);
  gpio_init(PIN_ACTIVITY_CORE1);

  gpio_set_dir(PIN_ACTIVITY_CORE0, true);
  gpio_set_dir(PIN_ACTIVITY_CORE1, true);

  if (!UniBootK210())
    for (;;) __wfe();

  multicore_launch_core1(core1_main);
  blink(PIN_ACTIVITY_CORE0, 500);
}
