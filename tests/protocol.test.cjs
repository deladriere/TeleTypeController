const test=(name,run)=>{run();console.log(`PASS ${name}`);};
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const context={window:{}};vm.createContext(context);vm.runInContext(fs.readFileSync(__dirname+'/../GUI/protocol.js','utf8'),context);
const {SX,encode,decode,configFrom,performance}=context.window.TeletypeProtocol;
const config=()=>({buttonChannel:4,potChannel:8,flags:0,notes:Array.from({length:20},(_,i)=>36+i),types:[0,0,1,0],cc:[1,2,74,71]});
const array=x=>Array.from(x);
test('configuration command IDs agree with firmware',()=>{
  const header=fs.readFileSync(__dirname+'/../src/sysex_protocol.h','utf8');
  for(const [name,id] of Object.entries(SX))assert.match(header,new RegExp(`#define SX_${name}\\s+0x${id.toString(16).toUpperCase().padStart(2,'0')}\\b`));
  assert.equal(JSON.stringify(encode(SX.SET_BTN_NOTE,[5,60])),'[240,125,84,84,16,5,60,247]');
  for(const value of [-1,128,NaN,1.5])assert.throws(()=>encode(SX.SET_BTN_NOTE,[1,value]));
});
test('malformed configuration frames are rejected',()=>{
  assert.equal(decode([240,1,84,84,66,247]),null);
  assert.equal(decode(encode(SX.CONFIG,[1,1])),null);
  assert.equal(decode(encode(SX.ACK,[])),null);
  const p=[4,8,5,...Array(20).fill(90),0,1,0,0,74,2,7,71];
  const c=configFrom(decode(encode(SX.CONFIG,p)).payload);
  assert.equal(c.buttonChannel,4);assert.equal(c.potChannel,8);assert.equal(c.flags,5);assert.equal(c.notes[19],90);
});
test('unique Note On maps to exact key and bank on its configured channel',()=>{
  const m=performance([0x93,42,127],config());assert.deepEqual(array(m.slots),[6]);
  for(const bytes of [[0x90,42,127],[0x93,42,0],[0x83,42,127],[0xF8],[0x93,128,127],[0x93,42]])assert.equal(performance(bytes,config()),null);
});
test('duplicate notes remain ambiguous, independent of previous bank',()=>{
  const c=config();c.notes[0]=42;
  assert.deepEqual(array(performance([0x93,42,100],c).slots),[0,6]);
  assert.equal(performance([0x93,90,100],c).slots.length,0);
});
test('CC matching respects channel, duplicate assignments and mute',()=>{
  const c=config();c.cc[1]=1;
  let m=performance([0xB7,1,99],c);assert.deepEqual(array(m.pots),[0,1]);assert.equal(m.value,99);
  c.flags=1;assert.deepEqual(array(performance([0xB7,1,99],c).pots),[1]);
  assert.equal(performance([0xB0,1,99],c),null);
});
test('pitch bend decodes signed endpoints and identifies all unmuted PB mappings',()=>{
  const c=config();c.types[3]=1;
  for(const [bytes,bend,value] of [[[0xE7,0,0],-8192,0],[[0xE7,0,64],0,64],[[0xE7,127,127],8191,127]]){
    const m=performance(bytes,c);assert.equal(m.bend,bend);assert.equal(m.value,value);assert.deepEqual(array(m.pots),[2,3]);
  }
});
