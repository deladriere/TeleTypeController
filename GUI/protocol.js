// Shared browser protocol. IDs and layouts match src/sysex_protocol.h.
(() => {
  const SX = Object.freeze({GET_VERSION:1, GET_CONFIG:2,
    SET_BTN_NOTE:16, SET_POT:17, SET_POT_IGNORE:18, SET_BTN_CH:19, SET_POT_CH:20,
    SAVE:32, LOAD:33, RESET:34,
    VERSION:65, CONFIG:66, ACK:79});
  function encode(cmd, payload=[]) {
    if (![cmd,...payload].every(v=>Number.isInteger(v)&&v>=0&&v<=127)) throw Error('Invalid MIDI data');
    return [240,125,84,84,cmd,...payload,247];
  }
  function decode(data) {
    if (!data || data.length<6 || data[0]!==240 || data[1]!==125 || data[2]!==84 || data[3]!==84 || data[data.length-1]!==247) return null;
    const cmd=data[4], payload=Array.from(data.slice(5,-1));
    if (![cmd,...payload].every(v=>Number.isInteger(v)&&v>=0&&v<=127)) return null;
    const lengths={[SX.CONFIG]:31,[SX.ACK]:2};
    if (cmd in lengths && payload.length!==lengths[cmd]) return null;
    if (cmd===SX.CONFIG && (payload[0]<1 || payload[0]>16 || payload[1]<1 || payload[1]>16 || payload[2]>15 || payload.slice(23,27).some(v=>v>1))) return null;
    if (cmd===SX.VERSION && (payload.length<1 || payload.length>31)) return null;
    return {cmd,payload};
  }
  function configFrom(p) {return {buttonChannel:p[0],potChannel:p[1],flags:p[2],notes:p.slice(3,23),types:p.slice(23,27),cc:p.slice(27,31)};}
  // Interpret ordinary incoming MIDI against the applied configuration.
  // Duplicate mappings remain explicit; a MIDI message cannot identify its source control.
  function performance(data,config) {
    if(!config || !data || data.length!==3)return null;
    const [status,a,b]=Array.from(data);
    if(!Number.isInteger(status)||status<128||status>239||![a,b].every(v=>Number.isInteger(v)&&v>=0&&v<=127))return null;
    const kind=status&240,channel=(status&15)+1;
    if(kind===144 && b>0){
      if(channel!==config.buttonChannel)return null;
      const slots=config.notes.flatMap((note,i)=>note===a?[i]:[]);
      return {kind:'note',note:a,channel,slots,text:`Note ${a} · velocity ${b} · channel ${channel}${slots.length>1?' · shared mapping':slots.length?'':' · unmapped'}`};
    }
    if(channel!==config.potChannel || (kind!==176 && kind!==224))return null;
    const bend=(a|(b<<7))-8192;
    const pots=config.types.flatMap((type,i)=>!(config.flags&(1<<i)) && (kind===224?type===1:type===0&&config.cc[i]===a)?[i]:[]);
    return {kind:kind===224?'bend':'cc',cc:a,bend,channel,pots,value:kind===224?Math.round((bend+8192)*127/16383):b,
      text:`${kind===224?`Pitch bend ${bend}`:`CC ${a} · ${b}`} · channel ${channel}${pots.length>1?' · shared mapping':pots.length?'':' · unmapped'}`};
  }
  window.TeletypeProtocol={SX,encode,decode,configFrom,performance};
})();
