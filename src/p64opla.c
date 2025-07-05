// p64oPLA - microcontroller-based C64 PLA
// https://github.com/c1570/p64opla

#include "pico/stdlib.h"
#include "pico/sync.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/structs/pio.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/syscfg.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/dma.h"
#include "hardware/pio.h"

#include "pla.h"

uint SYS_CLOCK_KHZ = 40000; //TODO

const uint LED_PIN = 25;
const uint BUTTON_PIN = 24;
const uint MODE_PIN = 26;

static void selftest(const uint pin_start, const uint pin_end) {
  // short/stuck detection: sets all pins to pullup, shifts one LOW through pins,
  // blinks if more than one pin is low during that
  void signal_pin_broken(const uint pin) {
    for(uint i = 0; i <= pin; i++) { // blink 1 time for GPIO 0, etc.
      gpio_put(LED_PIN, 1);
      busy_wait_us(500000);
      gpio_put(LED_PIN, 0);
      busy_wait_us(200000);
    }
    busy_wait_us(800000);
  }
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
    for(uint pin = pin_start; pin <= pin_end; pin++) {
      gpio_set_dir(pin, GPIO_OUT); // set GPIO "pin" low (value has been set above)
      busy_wait_us(1000);
      uint32_t gpios_in = gpio_get_all() & mask_our_pins;
      if(gpios_in != (mask_our_pins - (1 << pin))) {
        // we expected only one GPIO low but don't see that: there's some short
        signal_pin_broken(pin); // start signaling with output pin number
        for(uint test = 0; test <= pin_end; test++) {
          if(test == pin) continue;
          if(!(gpios_in & 1)) signal_pin_broken(test);
          gpios_in >>= 1;
        }
        busy_wait_us(2000000);
      }
      gpio_set_dir(pin, GPIO_IN);
    }
    gpio_put(LED_PIN, 1);
    busy_wait_us(2000000);
    gpio_put(LED_PIN, 0);
    busy_wait_us(1000000);
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

static void platest_offline() {
  // external PLA test: connect p64opla parallel to another PLA standalone,
  // run this test, and LED will light if the other PLA behaves incorrectly.
  // one successful test run takes a few milliseconds.
  gpio_put(LED_PIN, 0);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  for(uint pin = 0; pin <= 23; pin++) {
    gpio_init(pin);
    if(pin < 16) {
      gpio_pull_up(pin);
      gpio_put(pin, 0);
    }
    gpio_set_dir(pin, GPIO_IN);
  }
  while(1) {
    for(uint i = 0; i < 0x10000; i++) {
      gpio_set_dir_masked(0xffff, i ^ 0xffff);
      uint8_t expected_out = PLA_OUTPUT[i];
      busy_wait_us(2);
      bool correct = ((gpio_get_all() >> 16) & 0xff) == expected_out;
      if(!correct) {
        gpio_put(LED_PIN, 1);
        busy_wait_us(1000000);
        gpio_put(LED_PIN, 0);
      }
    }
  }
}

void __attribute__ ((noinline)) __scratch_x("idle_loop") idle_loop() {
  while(1) __wfe();
}

const PIO pio = pio0;
const uint pinread_sm = 0;
const uint pinwrite_sm = 1;
const uint addrfetch1_dma_chan = 0;
const uint addrfetch2_dma_chan = 1;
const uint lookup_dma_chan = 2;

int main() {
  // init pins
  for(int pin=0; pin<=23; pin++) {
    pio_gpio_init(pio, pin);
    if(pin <= 15) {
      gpio_pull_up(pin);
    } else if(pin <= 23) {
      gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
      gpio_set_slew_rate(pin, GPIO_SLEW_RATE_SLOW);
    }
  }
  pio_gpio_init(pio, 27);

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

  // disable GPIO input synchronizers, reduces latency
  syscfg_hw->proc_in_sync_bypass = 0x3fffffff;
  pio0_hw->input_sync_bypass = 0x3fffffff;

  // enable fastperi perfsel counters
  bus_ctrl_hw->counter[0].sel = arbiter_fastperi_perf_event_access_contested;
  bus_ctrl_hw->counter[0].value = 0;
  bus_ctrl_hw->counter[1].sel = arbiter_fastperi_perf_event_access;
  bus_ctrl_hw->counter[1].value = 0;

  // add PIO0 programs - unfortunately DMA cannot access GPIO input/output SIO registers so we have to go through PIOs
  // 1. program used by pinread SM
  pio->instr_mem[0] = pio_encode_in(pio_pins, 30) | pio_encode_sideset(1, 0);
  pio->instr_mem[1] = pio_encode_in(pio_pins, 30) | pio_encode_sideset(1, 1);
  const uint pinread_sm_start = 0;
  const uint pinread_sm_end = 1;
  // 2. program used by pinwrite SM
  pio->instr_mem[2] = pio_encode_out(pio_pins, 8);
  const uint pinwrite_sm_start = 2;
  const uint pinwrite_sm_end = 2;

  // configure PIO0 pindirs
  pio_sm_set_pindirs_with_mask(pio, pinread_sm, ~0u, 0b1000111111110000000000000000u); // GPIO0-15 are input, 16-23 are output

  // **** init pinread PIO program (reading input pins, pushing data which is picked up by DMA)
  pio_sm_config pinread_sm_config = pio_get_default_sm_config();
  sm_config_set_wrap(&pinread_sm_config, pinread_sm_start, pinread_sm_end); // PIO programs have been set up before
  sm_config_set_in_pins(&pinread_sm_config, 0 /* SM pins start at GPIO 0 */);
  sm_config_set_in_shift(&pinread_sm_config, false /* true: shift ISR right */,
                         true /* autopush */, 28 /* autopush threshold */);
  sm_config_set_sideset_pins(&pinread_sm_config, 27);
  sm_config_set_sideset(&pinread_sm_config, 1, false, false);
  sm_config_set_clkdiv(&pinread_sm_config, 3);
  pio_sm_init(pio, pinread_sm, pinread_sm_start, &pinread_sm_config);

  // **** init addrfetch1 DMA (get address that PIO0 SM0 has output, send and trigger lookup DMA)
  dma_channel_config addrfetch1_dma_config = dma_channel_get_default_config(addrfetch1_dma_chan);
  channel_config_set_transfer_data_size(&addrfetch1_dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&addrfetch1_dma_config, false);
  channel_config_set_write_increment(&addrfetch1_dma_config, false);
  channel_config_set_chain_to(&addrfetch1_dma_config, addrfetch2_dma_chan); // chain to addrfetch2 which will chain back to us
  channel_config_set_dreq(&addrfetch1_dma_config,
                          pio_get_dreq(pio, pinread_sm, false /* receive data from pinread PIO program */));
  dma_channel_configure(addrfetch1_dma_chan, &addrfetch1_dma_config,
                        &dma_hw->ch[lookup_dma_chan].al3_read_addr_trig /* write addr: write to lookup DMA read addr and trigger its transfer */,
                        &pio->rxf[pinread_sm] /* read addr: read from pinread rx FIFO */,
                        1000 /* number of transfers, doesn't really matter for us */,
                        false /* started later */);

  // **** init addrfetch2 DMA (bit of a hack to get a DMA transfer that runs continuously - RP2350 supports this natively, RP2040 doesn't)
  dma_channel_config addrfetch2_dma_config = dma_channel_get_default_config(addrfetch2_dma_chan);
  channel_config_set_transfer_data_size(&addrfetch2_dma_config, DMA_SIZE_32);
  channel_config_set_read_increment(&addrfetch2_dma_config, false);
  channel_config_set_write_increment(&addrfetch2_dma_config, false);
  channel_config_set_chain_to(&addrfetch2_dma_config, addrfetch1_dma_chan); // chain to addrfetch1
  channel_config_set_dreq(&addrfetch2_dma_config,
                          pio_get_dreq(pio, pinread_sm, false /* receive data from pinread PIO program */));
  dma_channel_configure(addrfetch2_dma_chan, &addrfetch2_dma_config,
                        &dma_hw->ch[lookup_dma_chan].al3_read_addr_trig /* write addr: write to lookup DMA read addr and trigger its transfer */,
                        &pio->rxf[pinread_sm] /* read addr: read from pinread rx FIFO */,
                        1000 /* number of transfers */,
                        false /* don't start, will get triggered/chained when addrfetch1 ends */);

  // **** init lookup DMA (lookup byte from PLA_OUTPUT array, send to GPIO output register)
  dma_channel_config lookup_dma_config = dma_channel_get_default_config(lookup_dma_chan);
  channel_config_set_transfer_data_size(&lookup_dma_config, DMA_SIZE_8);
  channel_config_set_read_increment(&lookup_dma_config, false);
  channel_config_set_write_increment(&lookup_dma_config, false);
  dma_channel_configure(lookup_dma_chan, &lookup_dma_config,
                        &pio->txf[pinwrite_sm] /* write addr */,
                        0 /* read addr - will be set by addrfetch DMAs */,
                        1 /* number of transfers */, false /* don't start - will get triggered by addrfetch DMA */);

  // **** init pinwrite PIO program (getting data from lookup DMA, writing to output pins)
  pio_sm_config pinwrite_sm_config = pio_get_default_sm_config();
  sm_config_set_wrap(&pinwrite_sm_config, pinwrite_sm_start, pinwrite_sm_end); // PIO programs have been set up before
  sm_config_set_out_pins(&pinwrite_sm_config, 16, 8); // we use 8 output pins starting at GPIO 16
  sm_config_set_out_shift(&pinwrite_sm_config, true /* true: shift OSR right */,
                          true /* autopull */, 7 /* autopull threshold */);
  pio_sm_init(pio, pinwrite_sm, pinwrite_sm_start, &pinwrite_sm_config);
  pio_sm_set_clkdiv(pio, pinwrite_sm, 1);

  // start DMA
  dma_channel_start(addrfetch1_dma_chan);

  // start PIOs
  pio_set_sm_mask_enabled(pio, 0b11 /* SM mask */, true /* enable */);

  //pio_sm_clear_fifos(pio, pinread_sm);
  //pio_sm_clear_fifos(pio, pinwrite_sm);

  idle_loop();
}
