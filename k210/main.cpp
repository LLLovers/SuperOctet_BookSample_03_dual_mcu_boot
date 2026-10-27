#include <atomic>

#include "bsp.h"
#include "fpioa.h"
#include "gpiohs.h"
#include "sleep.h"
#include "sysctl.h"

#include "board/hardware_config.h"

#include "uarths.h"

alignas(64) static spinlock_t led_lock = SPINLOCK_INIT;
alignas(64) static std::atomic<bool> core1_started{false};

static void blink(uint32_t pin, uint32_t half_period_ms) {
  bool level = false;
  for (;;) {
    level = !level;
    // GPIOHS output_val is shared: even different bits need one lock.
    spinlock_lock(&led_lock);
    gpiohs_set_pin(pin, level ? GPIO_PV_HIGH : GPIO_PV_LOW);
    spinlock_unlock(&led_lock);
    msleep(half_period_ms);
  }
}

static int core1_main(void *) {
  core1_started.store(true, std::memory_order_relaxed);
  blink(ACTIVITY_GPIOHS_CORE1, 350);
  return 0;
}

int main() {
  fpioa_set_function(PIN_ACTIVITY_CORE0, FUNC_GPIOHS2);
  fpioa_set_function(PIN_ACTIVITY_CORE1, FUNC_GPIOHS3);

  for (uint32_t pin = 2; pin <= 3; ++pin) {
    gpiohs_set_pin(pin, GPIO_PV_LOW);
    gpiohs_set_drive_mode(pin, GPIO_DM_OUTPUT);
  }

  mb();

  register_core1(core1_main, nullptr);
  while (!core1_started.load(std::memory_order_relaxed)) {}

  fpioa_set_function(PIN_MIDI_TX , FUNC_UARTHS_TX);
  fpioa_set_function(PIN_MIDI_RX, FUNC_UARTHS_RX);
  uarths_init();
  uarths_config(115200, UARTHS_STOP_1);
  const uint8_t started = 0xc3;
  uarths_send_data(&started, 1);
  usleep(2000);
  uarths_config(3000000, UARTHS_STOP_1);

  blink(ACTIVITY_GPIOHS_CORE0, 250);
}
