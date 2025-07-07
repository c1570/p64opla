// p64oPLA - microcontroller-based C64 PLA
// https://github.com/c1570/p64opla
// AGPLv3 licensed

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/structs/sio.h"

#include "pla.h"

uint SYS_CLOCK_KHZ = 400000;

const uint LED_PIN = 25;
const uint BUTTON_PIN = 24;
const uint MODE_PIN = 26;

void blink_led(const uint flashes, const uint on_ms, const uint off_ms, const uint after_ms) {
  for(uint i = 1; i <= flashes; i++) {
    gpio_put(LED_PIN, 1);
    busy_wait_us(on_ms * 1000);
    gpio_put(LED_PIN, 0);
    busy_wait_us(off_ms * 1000);
  }
  busy_wait_us(after_ms * 1000);
}

static void selftest(const uint pin_start, const uint pin_end) {
  // short/stuck detection: sets all pins to pullup, shifts one LOW through pins,
  // blinks if more than one pin is low during that
  gpio_put(LED_PIN, 0);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  for(uint pin = pin_start; pin <= pin_end; pin++) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    gpio_put(pin, false);
  }
  const uint32_t mask_our_pins = ((1 << (pin_end + 1)) - 1) - ((1 << pin_start) - 1);
  while(1) {
    bool found_error = false;
    for(uint pin = pin_start; pin <= pin_end; pin++) {
      gpio_set_dir(pin, GPIO_OUT); // set GPIO "pin" low (value has been set above)
      busy_wait_us(1000);
      uint32_t gpios_in = gpio_get_all() & mask_our_pins;
      if(gpios_in != (mask_our_pins - (1 << pin))) {
        // we expected only one GPIO low but don't see that: there's some short
        blink_led(pin + 1, 500, 200, 800); // start signaling with output pin number
        for(uint test = 0; test <= pin_end; test++) {
          if(test != pin) {
            if(!(gpios_in & 1)) blink_led(test + 1, 500, 200, 800); // blink 1 time for GPIO 0, etc.
            found_error = true;
          }
          gpios_in >>= 1;
        }
        busy_wait_us(2000000);
      }
      gpio_set_dir(pin, GPIO_IN);
    }
    if(!found_error) {
      blink_led(1, 5000, 100, 0);
    } else {
      gpio_put(LED_PIN, 0);
      busy_wait_us(4000000);
    }
  }
}

static void platest_live() {
  // live external PLA test: connect p64opla parallel to another PLA in the C64,
  // run this test, and LED will light if the other PLA behaves incorrectly.
  gpio_put(LED_PIN, 0);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  for(uint pin = 0; pin <= 23; pin++) {
    gpio_init(pin);
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_IN);
  }
  uint32_t steady = 0;
  uint32_t gpios_prev = 0;
  while(1) {
    uint32_t gpios = gpio_get_all() & 0x00ffffff;
    if((gpios & 0xffff) != (gpios_prev & 0xffff)) {
      steady = 0;
      gpios_prev = gpios;
      continue;
    }
    if(steady > 2) {
      // if input lines have been steady for a bit, we can assume
      // outputs have settled as well
      uint8_t expected_out = PLA_OUTPUT[gpios & 0xffff];
      if(((gpios >> 16) & 0xff) != expected_out) {
        gpio_put(LED_PIN, 1);
        busy_wait_us(5000000);
      }
      gpio_put(LED_PIN, 0);
    } else {
      steady++;
    }
  }
}

static inline int __attribute__((always_inline)) get_signal_latency(uint trigger_pin, uint measure_pin, bool trigger_on_posedge, bool wait_for_posedge) {
  // set up pin states in PIO before switching GPIO functions to PIO
  pio_sm_set_consecutive_pindirs(pio0, 0, trigger_pin, 1, true);
  pio_sm_set_pins(pio0, 0, (!trigger_on_posedge) << trigger_pin);
  pio_sm_set_consecutive_pindirs(pio0, 0, measure_pin, 1, false);
  // save previous pin functions, then switch functions to PIO
  enum gpio_function func_trigger = gpio_get_function(trigger_pin);
  gpio_set_function(trigger_pin, GPIO_FUNC_PIO0);
  enum gpio_function func_measure = gpio_get_function(measure_pin);
  gpio_set_function(measure_pin, GPIO_FUNC_PIO0);
  busy_wait_us(10); // wait for things to settle

  pio0->instr_mem[0] = pio_encode_set(pio_pins, trigger_on_posedge);
  pio0->instr_mem[1] = pio_encode_in(pio_pins, 1);
  pio_sm_config measure_sm_config = pio_get_default_sm_config();
  sm_config_set_wrap(&measure_sm_config, 1, 1); // loop on IN instruction
  sm_config_set_in_pins(&measure_sm_config, measure_pin);
  sm_config_set_in_shift(&measure_sm_config, false /* true: shift ISR right */,
                         true /* autopush */, 32 /* autopush threshold */);
  sm_config_set_set_pins(&measure_sm_config, trigger_pin, 1);
  sm_config_set_clkdiv(&measure_sm_config, 1);
  pio_sm_init(pio0, 0, 0 /* SM start instruction addr */, &measure_sm_config);
  pio_sm_set_enabled(pio0, 0, true);

  int cycles = 0;
  while(1) {
    uint32_t p = pio_sm_get_blocking(pio0, 0);
    if(!wait_for_posedge) p = ~p;
    if(__builtin_expect(!p, 1)) { // fast path has to be faster than 32 cycles
      cycles += 32;
      if(cycles >> 22) { // timeout after about 10ms
        cycles = -1;
        break;
      }
      continue;
    } else {
      cycles += __builtin_clz(p); // count leading zero bits
      break;
    }
  }

  pio_sm_set_enabled(pio0, 0, false);
  pio_sm_clear_fifos(pio0, 0);
  // restore previous GPIO functions (SIO in this case)
  gpio_set_function(trigger_pin, func_trigger);
  gpio_set_function(measure_pin, func_measure);
  busy_wait_us(2);

  return cycles;
}

static void platest_offline() {
  // external PLA test: connect p64opla parallel to another PLA standalone,
  // run this test, and LED will light if the other PLA behaves incorrectly.
  // one successful test run takes a few milliseconds.
  gpio_put(LED_PIN, 0);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  for(uint pin = 0; pin <= 23; pin++) {
    gpio_init(pin);
    if(pin < 16) {
      gpio_put(pin, 0);
      gpio_set_dir(pin, GPIO_OUT);
      gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
    } else {
      gpio_set_dir(pin, GPIO_IN);
    }
  }
  stdio_init_all();
  while(1) {
    gpio_put_masked(0xffff, 0 | (1<<7) | (1<<14)); // $0000 RAM read access, CAS inactive, BA active
    float lat_min = -1;
    float lat_max = -1;
    for(uint i = 0; i < 10; i++) {
      int latency = get_signal_latency(7, 23, false, false); // enable CAS, wait for CASRAM to go active
      if(latency < lat_min || lat_min == -1) lat_min = latency;
      if(latency > lat_max || lat_max == -1) lat_max = latency;
    }
    // at 400MHz, RP2040 pin to pin latency is 5 cycles
    lat_min = lat_min >= 5 ? ((float) lat_min - 5) * (1000000.0 / SYS_CLOCK_KHZ) : -1;
    lat_max = lat_max >= 5 ? ((float) lat_max - 5) * (1000000.0 / SYS_CLOCK_KHZ) : -1;
    if(lat_min >= 0) {
      printf("CASRAM latency: %.1f to %.1f nanoseconds\n", lat_min, lat_max);
    } else {
      printf("Could not measure CASRAM latency\n");
    }

    bool failure = false;
    for(uint i = 0; i < 0x10000; i++) {
      gpio_put_masked(0xffff, i);
      uint8_t expected_out = PLA_OUTPUT[i];
      busy_wait_us(2);
      uint8_t actual_out = (gpio_get_all() >> 16) & 0xff;
      bool correct = actual_out == expected_out;
      if(!correct) {
        printf("Test pattern 0b%016b, expected 0b%08b, got 0b%08b\n", i, expected_out, actual_out);
        failure = true;
        blink_led(5, 150, 150, 200);
      }
    }
    if(!failure) {
      printf("Test passed successfully\n");
      blink_led(1, 5000, 100, 0);
    }
  }
}

void __attribute__((noinline)) __scratch_x("c0") core0() {
  #include "p64opla_loop.inc"
}

void __attribute__((noinline)) __scratch_y("c1") core1() {
  #include "p64opla_loop.inc"
}

int main() {
  // init pins
  for(int pin=0; pin<=29; pin++) {
    gpio_init(pin);
    if(pin <= 15) {
      gpio_set_dir(pin, GPIO_IN);
      // gpio_pull_up(pin); // debugging only
    } else if(pin <= 23) {
      gpio_set_dir(pin, GPIO_OUT);
      gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
      gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
    }
  }

  // init LED
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 1);

  // init button
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);

  // init mode pin
  gpio_init(MODE_PIN);
  gpio_set_dir(MODE_PIN, GPIO_IN);
  gpio_pull_up(MODE_PIN);

  // set system clock
  vreg_set_voltage(VREG_VOLTAGE_1_30);
  busy_wait_us(10000);
  set_sys_clock_khz(SYS_CLOCK_KHZ, true);
  gpio_put(LED_PIN, 0);
  gpio_set_dir(LED_PIN, GPIO_IN);
  busy_wait_us(10000); // core0 likes to crash on RP2350 otherwise

  // if mode pin is grounded, allow tests that set input lines
  if(!gpio_get(MODE_PIN)) {
    if(gpio_get(BUTTON_PIN)) {
      selftest(0, 23); // only mode pin grounded: do selftest
    } else {
      platest_offline(); // mode pin grounded and button pressed: external PLA offline test
    }
  } else {
    // do live external PLA test if (only) button pressed on startup
    if(!gpio_get(BUTTON_PIN)) platest_live();
  }

  // set upper GPIO input overrides so that the PIO reading the GPIOs
  // just produces the appropriate address of the PLA_OUTPUT entry
  for(int pin=16; pin<=29; pin++) {
    gpio_set_input_enabled(pin, false);
    gpio_set_inover(pin, ((((uint32_t) PLA_OUTPUT) >> pin) & 1) ? GPIO_OVERRIDE_HIGH : GPIO_OVERRIDE_LOW);
  }

  multicore_launch_core1(core1);
  core0();
}
