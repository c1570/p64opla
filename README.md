# p64oPLA - microcontroller-based C64 PLA
https://github.com/c1570/p64opla

![p64oPLA prototype](/images/p64oPLA.jpg)

This implements the [Commodore 64](https://en.wikipedia.org/wiki/Commodore_64) [PLA chip](https://www.c64-wiki.com/wiki/PLA_(C64_chip)) using an [RP2040](https://en.wikipedia.org/wiki/RP2040).

This is work in progress.

Pronounced **picoPLA**. Stylizing as 'p64oPLA' makes this better to search for, and it's not really using the Pico (board) anyways.

* cheap
* simple hardware variant can be built using a spare IC socket and a few wires
* extra modes
  * **selftest** (GPIO26 grounded): Checks for shorts on GPIO pins, blinks GPIO num+1 times (GPIO2 = 3 flashes) in case of problems on that line. If no error found, turns LED on for five seconds, off for 100ms, repeat.
  * **offline PLA test** (GPIO26 and GPIO24 grounded): Checks a PLA connected in parallel for errors. LED will blink on errors. If no error found, turns LED on for five seconds, off for 100ms, repeat. Problems will be reported in text via USB serial.
  * **live PLA test** (GPIO24 grounded): If connected to a PLA seated in a C64, will check those PLA's outputs while the C64 is powered on. Make sure p64oPLA is not connected via USB then (otherwise current would flow from USB into the C64). LED will light on errors, otherwise the LED will stay off.

* Line delay is very similar to the original PLA.
* In contrast to most replacements, all signal lines exhibit the appropriate signal delay, not only CASRAM.
* There are no glitches (EPROM PLA, I'm looking at you).
* However, this implementation exhibits some delay jitter (signal delay is about 25-35ns).

# Building

* wire GPIO0-15 to PLA pins 2-9 and 20-27; wire GPIO16-23 to PLA pins 10-13 and 15-18; connect 5V/Vbus to PLA pin 28; connect GND to PLA pin 14 and PLA pin 19
* note the original Pico board does not expose GPIO23; "RGB LED" RP2040 boards do though (disable the RGB LED there though by unsoldering the "RGB" jumper)

Designing a dedicated PCB would be nice at some point.

# Implementation notes
This simply looks up output values using an array using the CPU cores, overclocked to 400MHz.
Both cores run in parallel, executing the same program, to reduce worst case latency.
They sync automatically because of SRAM lookup contention.

The data word read from the GPIOs represents the RAM address of the byte to look up directly because input overrides are set for upper GPIOs.

No shifting of the output value (to GPIO16 and up) is needed since byte writes to SIO registers are replicated over the full bus width (e.g., writing byte 0x12 actually writes 0x12121212).

## Abandoned PIO/DMA version
A PIO/DMA variant of this was also in testing (similar to [cknave's C64 Pico RAM interface](https://github.com/cknave/c64-pico-ram-interface)):

* reading from GPIOs using a PIO
* DMA the resulting address from the PIO RX FIFO to another DMA channel's read address (and trigger) register
* The other DMA channel fetches the result byte from the lookup table and writes it to another PIO
* Other PIO writes to GPIOs

(One could actually omit using the PIOs but unfortunately SIO GPIO control registers are only available to the CPU cores (not DMA).)

However, this has a rather long latency along these lines:
* 1 cycle input GPIO pad
* 2 cycles input synchronizer
* 1 cycle input PIO
* 4 cycles first DMA pipeline (details: [1](https://forums.raspberrypi.com/viewtopic.php?p=1915671#p1915671) [2](https://forums.raspberrypi.com/viewtopic.php?p=2196725#p2196725) [3](https://forums.raspberrypi.com/viewtopic.php?p=2247708#p2247708))
* 4 cycles second DMA pipeline
* 1 cycle output PIO
* 1 cycle output GPIO pad

Also, there's a lot of jitter, probably because PIO FIFO reads/writes and DMA control writes all go via a single AHB port.

Perhaps RP2350 improves this, but I doubt it.

# License
AGPLv3

In short, this means that...
* proper attribution has to be given
* source has to be made available

In case you don't want to do this (e.g., you want to sell white labeled versions commercially, or not hand out the sources, or similar), commercial licenses are available starting 20€/piece.

# Compiling
```
export PICO_SDK_PATH=...
cmake -DPICO_COPY_TO_RAM=1 .
make -j16
```

## Testing
Use `src/test-p64pla.ts` along with [rp2040js](https://github.com/c1570/rp2040js).
