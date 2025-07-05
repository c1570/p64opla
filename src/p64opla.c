#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/syscfg.h"
#include "hardware/riscv_platform_timer.h"

/*
  This reads GPIO0-15, interprets that as a 16 bits offset
  into an array, and writes the result byte to GPIO16-23.
*/

uint SYS_CLOCK_KHZ = 125000;

const uint LED_PIN = 25;

//#define SIMPLE 1

#ifndef SIMPLE
// align array on 64k boundary AND in second SRAM bank (SRAM4_BASE)
// so no contention with core0() code happens
__attribute__ ((aligned(0x40000))) uint8_t xlate_array[0x10000];

void __attribute__((noinline)) core0() {
  // read 16 bits, do a lookup from those to 8 bits, then write 8 bits.
  uint8_t *in1, *in2;
  uint8_t out_value1, out_value2;
  out_value1 = out_value2 = 0xff;
  in1 = in2 = (uint8_t *) xlate_array;
  __asm__ __volatile__ ("add x0,x0,x0\n\t"); // just for proper word alignment of the loop
  while(1) {
    uint64_t t_start, t_end;
    for(uint64_t i = 0; i < 0x100000; i++) {
      __compiler_memory_barrier();
      t_start = riscv_timer_get_mtime();
      __compiler_memory_barrier();
      #pragma GCC unroll 128
      for(uint i = 0; i < 128; i++) {
        // basically, this is a read-translate-write pipeline,
        // two threads interleaved (so no dependencies between consecutive ops),
        // two loops unrolled
        ((uint8_t *) &sio_hw->gpio_out)[0] = out_value2; // narrow 8 bit write gets replicated over bus width so shows up on GPIO16-23, too
        in1 = (uint8_t *) sio_hw->gpio_in;
        out_value2 = in2[0];
        __compiler_memory_barrier();
        ((uint8_t *) &sio_hw->gpio_out)[0] = out_value1;
        in2 = (uint8_t *) sio_hw->gpio_in;
        out_value1 = in1[0];
        __compiler_memory_barrier();
        ((uint8_t *) &sio_hw->gpio_out)[0] = out_value2;
        in1 = (uint8_t *) sio_hw->gpio_in;
        out_value2 = in2[0];
        __compiler_memory_barrier();
        ((uint8_t *) &sio_hw->gpio_out)[0] = out_value1;
        in2 = (uint8_t *) sio_hw->gpio_in;
        out_value1 = in1[0];
        __compiler_memory_barrier();
      }
      __compiler_memory_barrier();
      t_end = riscv_timer_get_mtime();
      __compiler_memory_barrier();
    }
    stdio_init_all();
    busy_wait_us(2000000);
    printf("Loop (128*3*4=1536 ops) took %lld ticks\n", t_end - t_start - 5);
    busy_wait_us(1000000);
    stdio_deinit_all();
  }
}
  /*
  // On RP2350/RISC-V, this compiles to the following.
  // At 125MHz, about 84-124ns latency = 10-16 cycles (measured)
  // However, I would expect...
  //  optimal case: 2 sync + 2 pads + 8 cycles
  //  worst case:   2 sync + 2 pads + 10/11 cycles
  20080000:       00000033                add     zero,zero,zero
  20080004:       0ff00613                li      a2,255
  20080008:       200106b7                lui     a3,0x20010
  2008000c:       d00007b7                lui     a5,0xd0000
  20080010:       85b2                    mv      a1,a2
  20080012:       00068693                mv      a3,a3
  20080016:       07c1                    addi    a5,a5,16 # d0000010 <__StackTop+0xaff7e010>
  20080018:       d0000737                lui     a4,0xd0000

  2008001c:       00c78023                sb      a2,0(a5)  3      out2 => SIO
  20080020:       0006c603                lbu     a2,0(a3)  4        in2 => out2 lookup
  20080024:       4354                    lw      a3,4(a4)  2    SIO => in1
  20080026:       00b78023                sb      a1,0(a5)  1 out1 => SIO
  2008002a:       0006c583                lbu     a1,0(a3)  2    in1 => out1 lookup
  2008002e:       4354                    lw      a3,4(a4)  3      SIO => in2
  20080030:       00c78023                sb      a2,0(a5)  4        out2 => SIO
  20080034:       0006c603                lbu     a2,0(a3)  3      in2 => out2 lookup
  20080038:       4354                    lw      a3,4(a4)  1 SIO => in1
  2008003a:       00b78023                sb      a1,0(a5)  2    out1 => SIO
  2008003e:       0006c583                lbu     a1,0(a3)  1 in1 => out1 lookup
  20080042:       4354                    lw      a3,4(a4)  4        SIO => in2
  20080044:       bfe1                    j       2008001c <core0+0x1c>
  */

#else

// just pass input directly, no lookup/translation
void __attribute__((noinline)) __scratch_x("c0") core0() {
  while(1) {
    uint8_t gpios = (uint8_t) sio_hw->gpio_in;
    ((uint8_t *) &sio_hw->gpio_out)[0] = gpios;
    __compiler_memory_barrier();
  }

  /*
  // On RP2040, that code compiles to this (should result in 2-6 cycles latency)
  // At 20MHz, about 180-360ns latency = 9-18 cycles (synch off: 80-260ns = 4-13 cycles)
  // At 125MHz, about 40-72ns = 5-9 cycles (sync off: 24ns-56ns = 3-7 cycles)
  20040000:       21d0            movs    r1, #208        @ 0xd0
  20040002:       4a02            ldr     r2, [pc, #8]
  20040004:       0609            lsls    r1, r1, #24
  20040006:       684b            ldr     r3, [r1, #4]
  20040008:       7013            strb    r3, [r2, #0]
  2004000a:       e7fc            b.n     20040006
  2004000c:       d0000010        .word   0xd0000010

  // On RP2350/RISC-V, that code compiles to this (should give 2-5 cycles latency)
  // **COMPRESSED INSTRUCTIONS OFF**
  // At 20MHz, about 175-350ns latency = 9-17 cycles (sync off: 75-250 = 4-12 cycles)
  // At 125MHz, about 40-64ns = 5-8 cycles (sync off: 24-48ns = 3-6 cycles)
  20080000:       d0000737                lui     a4,0xd0000
  20080004:       01070713                addi    a4,a4,16 # d0000010 <__StackTop+0xaff7e010>
  20080008:       d00006b7                lui     a3,0xd0000
  2008000c:       0046a783                lw      a5,4(a3) # d0000004 <__StackTop+0xaff7e004>
  20080010:       00f70023                sb      a5,0(a4)
  20080014:       ff9ff06f                j       2008000c <core0+0xc>
  */
}
#endif

int main() {
  // init pins
  for(int pin=0; pin<=29; pin++) {
    gpio_init(pin);
    if(pin <= 15) {
      gpio_set_dir(pin, GPIO_IN);
      gpio_pull_up(pin);
    } else if(pin <= 23) {
      gpio_set_dir(pin, GPIO_OUT);
      gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_4MA);
    }
  }

  // disable GPIO input synchronizers, reduces latency, introduces glitches
  //syscfg_hw->proc_in_sync_bypass = 0x3fffffff;

  riscv_timer_set_enabled(true);
  riscv_timer_set_fullspeed(true);

  // init LED
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 1);

  // set system clock
  vreg_set_voltage(VREG_VOLTAGE_1_30);
  busy_wait_us(10000);
  set_sys_clock_khz(SYS_CLOCK_KHZ, true);
  gpio_put(LED_PIN, 0);
  gpio_set_dir(LED_PIN, GPIO_IN);
  busy_wait_us(10000);

  #ifndef SIMPLE
  // set upper GPIO input overrides so that reading the GPIOs
  // just produces the appropriate address of the xlate array entry
  // Note to self: any subsequent gpio_init() disables overrides again!
  for(int pin=16; pin<=29; pin++) {
    gpio_set_input_enabled(pin, false);
    gpio_set_inover(pin, ((((uint32_t) xlate_array) >> pin) & 1) ? GPIO_OVERRIDE_HIGH : GPIO_OVERRIDE_LOW);
  }
  // init xlate_array
  for(int i=0; i<0x10000; i++) xlate_array[i] = i & 0xff;
  #endif

  core0();
}
