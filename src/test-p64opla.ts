// test p64opla - use with https://github.com/c1570/rp2040js

import * as fs from 'fs';
import { bootromB1 } from './bootrom';
import { RP2040 } from '../src';
import { GPIOPinState } from '../src/gpio-pin';
import { loadHex } from './intelhex';

const hex = fs.readFileSync(`${process.env.P64OPLAHEX}`, 'utf-8');
const mcu = new RP2040();
mcu.loadBootrom(bootromB1);
loadHex(hex, mcu.flash, 0x10000000);

for(let i = 0; i < 16; i++) mcu.gpio[i].setInputValue(true);

const initial_pc = 0x10000000;
mcu.core0.PC = initial_pc;
mcu.core1.PC = initial_pc;
mcu.core1.waiting = true;

mcu.uart[0].onByte = (value: number) => {
  process.stdout.write(new Uint8Array([value]));
};

mcu.gpio[24].setInputValue(true);
mcu.gpio[26].setInputValue(true);

let nextTimeUpdate = 0;
let game = true;
try {
  while(1) {
    mcu.step();
    if(mcu.cycles > nextTimeUpdate) {
      let s = "";
      for(let i = 16; i < 24; i++) s = (mcu.gpio[i].outputValue ? "1" : "0") + s;
      console.log(`PC: 0x${mcu.core0.PC.toString(16)} Game: ${game ? "1" : "0"} - Output: 0b${s} - Sideset: ${mcu.gpio[27].outputValue ? "1" : "0"}`);
      game = !game;
      mcu.gpio[10].setInputValue(game);
      //console.log(`Time: ${((mcu.cycles / 40000000) >>> 0)/10} secs`);
      nextTimeUpdate += 1000;
    }
  }
} catch(e) {
  console.error(`Cycles: ${mcu.cycles}, ${e}`);
  throw e;
}
