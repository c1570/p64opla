#ifdef __arm__
  uint8_t *gpios1, *gpios2, *gpios3, *gpios4;
  uint8_t out_value1, out_value2, out_value3, out_value4;
  out_value1 = out_value2 = out_value3 = out_value4 = 0xff;
  gpios1 = gpios2 = gpios3 = gpios4 = (uint8_t *) PLA_OUTPUT;
  while(1) {
    ((uint8_t *) &sio_hw->gpio_out)[0] = out_value3;
    gpios1 = (uint8_t *) sio_hw->gpio_in;
    out_value4 = gpios4[0];
    __compiler_memory_barrier();
    ((uint8_t *) &sio_hw->gpio_out)[0] = out_value4;
    gpios2 = (uint8_t *) sio_hw->gpio_in;
    out_value1 = gpios1[0];
    __compiler_memory_barrier();
    ((uint8_t *) &sio_hw->gpio_out)[0] = out_value1;
    gpios3 = (uint8_t *) sio_hw->gpio_in;
    out_value2 = gpios2[0];
    __compiler_memory_barrier();
    ((uint8_t *) &sio_hw->gpio_out)[0] = out_value2;
    gpios4 = (uint8_t *) sio_hw->gpio_in;
    out_value3 = gpios3[0];
    __compiler_memory_barrier();
  }
/*
// ...the above compiles to this (1 inner ldr/ldrb/strb=fetch/map/store is 6 cycles).
// On RP2040, this code gives 24-35ns latency at 400MHz.

20040000:       23d0            movs    r3, #208        @ 0xd0
20040002:       20ff            movs    r0, #255        @ 0xff
20040004:       4907            ldr     r1, [pc, #28]   @ (20040024 <core0+0x24>)
20040006:       4a08            ldr     r2, [pc, #32]   @ (20040028 <core0+0x28>)
20040008:       061b            lsls    r3, r3, #24
2004000a:       7010            strb    r0, [r2, #0]
2004000c:       6858            ldr     r0, [r3, #4]
2004000e:       7809            ldrb    r1, [r1, #0]
20040010:       7011            strb    r1, [r2, #0]
20040012:       6859            ldr     r1, [r3, #4]
20040014:       7800            ldrb    r0, [r0, #0]
20040016:       7010            strb    r0, [r2, #0]
20040018:       6858            ldr     r0, [r3, #4]
2004001a:       7809            ldrb    r1, [r1, #0]
2004001c:       7011            strb    r1, [r2, #0]
2004001e:       6859            ldr     r1, [r3, #4]
20040020:       7800            ldrb    r0, [r0, #0]
20040022:       e7f2            b.n     2004000a <core0+0xa>
20040024:       20020000        .word   0x20020000
20040028:       d0000010        .word   0xd0000010
*/
#endif

#ifdef __riscv
  uint8_t *gpios1, *gpios2, *gpios3, *gpios4;
  uint8_t out_value1, out_value2, out_value3, out_value4;
  out_value1 = out_value2 = out_value3 = out_value4 = 0xff;
  gpios1 = gpios2 = gpios3 = gpios4 = (uint8_t *) PLA_OUTPUT;
  while(1) {
    ((uint8_t *) &sio_hw->gpio_out)[0] = out_value2;
    gpios1 = (uint8_t *) sio_hw->gpio_in;
    out_value2 = gpios2[0];
    __compiler_memory_barrier();
    ((uint8_t *) &sio_hw->gpio_out)[0] = out_value1;
    gpios2 = (uint8_t *) sio_hw->gpio_in;
    out_value1 = gpios1[0];
    __compiler_memory_barrier();
  }
/*
// ...the above compiles to this (1 inner lw/lbu/sb=fetch/map/store is 3 cycles?).
// On RP2350, this code gives 28-35ns latency at 400MHz.
20080000:       0ff00693                li      a3,255
20080004:       20020737                lui     a4,0x20020
20080008:       d00007b7                lui     a5,0xd0000
2008000c:       85b6                    mv      a1,a3
2008000e:       00070713                mv      a4,a4
20080012:       07c1                    addi    a5,a5,16 # d0000010 <__StackTop+0xaff7e010>
20080014:       d0000637                lui     a2,0xd0000
20080018:       00d78023                sb      a3,0(a5)
2008001c:       00074683                lbu     a3,0(a4)
20080020:       4258                    lw      a4,4(a2)
20080022:       00b78023                sb      a1,0(a5)
20080026:       00074583                lbu     a1,0(a4)
2008002a:       4258                    lw      a4,4(a2)
2008002c:       b7f5                    j       20080018 <core0+0x18>
*/
#endif
