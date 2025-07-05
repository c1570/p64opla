let pla_arr = new Uint8Array(0x10000);

for(let _HIRAM of [false, true]) {
for(let _LORAM of [false, true]) {
for(let _CHAREN of [false, true]) {
for(let A15 of [false, true]) {
for(let A14 of [false, true]) {
for(let A13 of [false, true]) {
for(let A12 of [false, true]) {
for(let _VA14 of [false, true]) {
for(let VA13 of [false, true]) {
for(let VA12 of [false, true]) {
for(let _AEC of [false, true]) { // true iff VIC-II has bus control
for(let R__W of [false, true]) { // true on bus reads by CPU/VIC-II/exp port, false on writes by CPU/exp port
for(let _EXROM of [false, true]) {
for(let _GAME of [false, true]) {
for(let BA of [false, true]) { // true iff no VIC-II DMA happening/upcoming
for(let _CAS of [false, true]) {

let _ROMH = !(_HIRAM  & A15  & !A14  & A13  & !_AEC  & R__W  & !_EXROM  & !_GAME 
         | A15  & A14  & A13  & !_AEC  & _EXROM  & !_GAME 
         | _AEC  & _EXROM  & !_GAME  & VA13  & VA12 );

let _ROML = !(_LORAM  & _HIRAM  & A15  & !A14  & !A13  & !_AEC  & R__W  & !_EXROM 
         | A15  & !A14  & !A13  & !_AEC  & _EXROM  & !_GAME );

let _IO = !(_HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & _GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & _GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & _GAME 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & !_EXROM  & !_GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & !_EXROM  & !_GAME 
             | A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & _EXROM  & !_GAME 
             | A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & _EXROM  & !_GAME );

let _CHAROM = !(_HIRAM  & !_CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & R__W  & _GAME 
             | _LORAM  & !_CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & !_CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _VA14  & _AEC  & _GAME  & !VA13  & VA12 
             | _VA14  & _AEC  & !_EXROM  & !_GAME  & !VA13  & VA12 );

let _KERNAL = !(_HIRAM  & A15  & A14  & A13  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & A15  & A14  & A13  & !_AEC  & R__W  & !_EXROM  & !_GAME );

let _BASIC = !(_LORAM  & _HIRAM  & A15  & !A14  & A13  & !_AEC  & R__W  & _GAME );

let _CASRAM = (_LORAM  & _HIRAM  & A15  & !A14  & A13  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & A15  & A14  & A13  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & A15  & A14  & A13  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _HIRAM  & !_CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & R__W  & _GAME 
             | _LORAM  & !_CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & !_CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _VA14  & _AEC  & _GAME  & !VA13  & VA12 
             | _VA14  & _AEC  & !_EXROM  & !_GAME  & !VA13  & VA12 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & _GAME 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & _GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & _GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & _GAME 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _HIRAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & !_EXROM  & !_GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | _LORAM  & _CHAREN  & A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & !_EXROM  & !_GAME 
             | A15  & A14  & !A13  & A12  & BA  & !_AEC  & R__W  & _EXROM  & !_GAME 
             | A15  & A14  & !A13  & A12  & !_AEC  & !R__W  & _EXROM  & !_GAME 
             | _LORAM  & _HIRAM  & A15  & !A14  & !A13  & !_AEC  & R__W  & !_EXROM 
             | A15  & !A14  & !A13  & !_AEC  & _EXROM  & !_GAME 
             | _HIRAM  & A15  & !A14  & A13  & !_AEC  & R__W  & !_EXROM  & !_GAME 
             | A15  & A14  & A13  & !_AEC  & _EXROM  & !_GAME 
             | _AEC  & _EXROM  & !_GAME  & VA13  & VA12 
             | !A15  & !A14  & A12  & _EXROM  & !_GAME 
             | !A15  & !A14  & A13  & _EXROM  & !_GAME 
             | !A15  & A14  & _EXROM  & !_GAME 
             | A15  & !A14  & A13  & _EXROM  & !_GAME 
             | A15  & A14  & !A13  & !A12  & _EXROM  & !_GAME 
             | _CAS );

let GR_W = !(!_CAS & A15 & A14 & !A13 & A12 & !_AEC & !R__W);

let v_in = (A13<<0)|(A14<<1)|(A15<<2)|
           (_VA14<<3)|
           (_CHAREN<<4)|(_HIRAM<<5)|(_LORAM<<6)|
           (_CAS<<7)|
           (VA12<<8)|(VA13<<9)|
           (_GAME<<10)|(_EXROM<<11)|
           (R__W<<12)|(_AEC<<13)|(BA<<14)|(A12<<15);
let v_out = (_ROMH<<0)|(_ROML<<1)|(_IO<<2)|(GR_W<<3)|(_CHAROM<<4)|(_KERNAL<<5)|(_BASIC<<6)|(_CASRAM<<7);
pla_arr[v_in]=v_out;

}}}}}}}}}}}}}}}}

let s = '';
let i = 0;
pla_arr.forEach(function(byte) {
  s += `0x${byte.toString(16).padStart(2, '0')}`;
  i++;
  if(i>0xffff) return;
  if((i%16)==0) s+= ",\n"; else s+= ",";
});
console.log(`const __attribute__ ((aligned(0x10000))) uint8_t PLA_OUTPUT[] = {\n${s}\n};`);
