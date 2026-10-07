(() => {
  const {SX,encode,decode,configFrom,performance}=window.TeletypeProtocol, view=window.TeletypeView;
  const $=id=>document.getElementById(id);
  let access=null,input=null,output=null,config=null,ready=false,epoch=0;
  let pending=null,queue=Promise.resolve(),queued=0,connecting=false;
  const dirty=new Set();
  const names={};for(const [name,id] of Object.entries(SX))names[id]=name;
  function log(text,type='info',announce=true){
    const row=document.createElement('div');row.className=type;
    row.textContent=`${new Date().toLocaleTimeString()}  ${text}`;$('console').append(row);
    while($('console').children.length>160)$('console').firstChild.remove();
    $('console').scrollTop=$('console').scrollHeight;
    if(announce)$('event').textContent=text;
  }
  function setStatus(text,connected=false){$('connection-status').textContent=text;$('connection-status').classList.toggle('ready',connected);}
  function setReady(value){ready=value;$('configuration').disabled=!value;for(const id of ['save','load','reset'])$(id).disabled=!value;}
  function edits(){$('edits-status').textContent=!ready?'Connect to read the device’s mappings.':dirty.size?`${dirty.size} unapplied edit${dirty.size===1?'':'s'}. Click Set for each; Save stores applied settings only.`:'All displayed mappings are applied. Click Save to keep them after power-off.';$('edits-status').classList.toggle('pending',dirty.size>0);}
  function mark(id){dirty.add(id);$(id).classList.add('dirty');edits();}
  function clean(id){dirty.delete(id);$(id).classList.remove('dirty');edits();}
  function renderConfig(p){
    config=configFrom(p);dirty.clear();document.querySelectorAll('.dirty').forEach(el=>el.classList.remove('dirty'));
    $('button-channel').value=config.buttonChannel;$('pot-channel').value=config.potChannel;
    config.notes.forEach((note,i)=>$(`note-${i}`).value=note);
    for(let i=0;i<4;i++){$(`type-${i}`).value=config.types[i];$(`cc-${i}`).value=config.cc[i];$(`mute-config-${i}`).checked=Boolean(config.flags&(1<<i));$(`cc-${i}`).disabled=config.types[i]===1;}
    view.setConfig(config);edits();
  }
  function resetConnection(message='Disconnected'){
    epoch++;ready=false;connecting=false;
    if(pending){clearTimeout(pending.timer);pending.reject(Error('Connection closed'));pending=null;}
    const closing=[];
    if(input){input.onmidimessage=null;closing.push(input.close().catch(()=>{}));}if(output)closing.push(output.close().catch(()=>{}));
    input=null;output=null;config=null;dirty.clear();view.reset();setReady(false);edits();setStatus(message);
    $('connect').disabled=false;$('connect').textContent='Connect MIDI ↗';$('disconnect').hidden=true;
    $('version').textContent='Teletype / USB MIDI';$('stage-caption').textContent='Connect Teletype, then play the physical keys and knobs.';
    $('bank-hint').textContent='Bank is inferred from the last uniquely mapped note; switch movements alone send no MIDI.';
    return Promise.all(closing);
  }
  function request(cmd,payload=[],expected=SX.ACK){
    return new Promise((resolve,reject)=>{
      if(!output || output.state==='disconnected'){reject(Error('Teletype is not connected.'));return;}
      if(pending){reject(Error('A device request is still pending.'));return;}
      const timer=setTimeout(()=>{pending=null;reject(Error(`No reply to ${names[cmd]}. Check the USB connection and firmware.`));},1500);
      pending={cmd,expected,resolve,reject,timer};
      try{output.send(encode(cmd,payload));}catch(error){clearTimeout(timer);pending=null;reject(error);}
    });
  }
  function enqueue(action){
    const generation=epoch;queued++;
    const job=queue.catch(()=>{}).then(async()=>{if(generation!==epoch)throw Error('Connection changed');return action();});
    queue=job.catch(error=>{if(generation===epoch)log(error.message,'error');}).finally(()=>queued--);
    return queue;
  }
  function onMessage(event){
    const frame=decode(event.data);
    if(!frame){
      if(!ready)return;
      const message=performance(event.data,config);
      if(message){view.receive(message);log(message.text,'info');}
      return;
    }
    const {cmd,payload:p}=frame;
    if(cmd===SX.VERSION)$('version').textContent=`Firmware ${String.fromCharCode(...p)}`;
    if(!pending)return;
    if(cmd===SX.ACK&&p[0]===pending.cmd){
      if(p[1]!==0){const item=pending;pending=null;clearTimeout(item.timer);const error=Error(`${names[p[0]]} was rejected by the device.`);error.rejected=true;item.reject(error);}
      else if(pending.expected===SX.ACK){const item=pending;pending=null;clearTimeout(item.timer);item.resolve(p);}
    }else if(cmd===pending.expected){const item=pending;pending=null;clearTimeout(item.timer);item.resolve(p);}
  }
  function pairs(){
    if(!access)return [];
    const inputs=[...access.inputs.values()].filter(p=>p.state!=='disconnected');
    const outputs=[...access.outputs.values()].filter(p=>p.state!=='disconnected');
    return outputs.flatMap(out=>{
      const matches=inputs.filter(p=>p.name===out.name&&p.manufacturer===out.manufacturer);
      const same=outputs.filter(p=>p.name===out.name&&p.manufacturer===out.manufacturer);
      return matches.length===1&&same.length===1?[{out,inp:matches[0]}]:[];
    });
  }
  function populate(){
    const previous=$('device').value, found=pairs();$('device').replaceChildren();
    for(const pair of found){const option=document.createElement('option');option.value=pair.out.id;option.textContent=pair.out.name||'MIDI device';$('device').append(option);}
    if(found.some(p=>p.out.id===previous))$('device').value=previous;
    else{const preferred=found.find(p=>/teletype/i.test(p.out.name));if(preferred)$('device').value=preferred.out.id;}
    $('device-label').hidden=found.length<2;return found;
  }
  async function bind(){
    const selected=pairs().find(p=>p.out.id===$('device').value);if(!selected)return;
    const closed=resetConnection('Connecting…');const generation=epoch;connecting=true;$('connect').disabled=true;$('disconnect').hidden=false;
    input=selected.inp;output=selected.out;input.onmidimessage=onMessage;
    try{
      await closed;if(generation!==epoch)return;
      await Promise.all([input.open(),output.open()]);if(generation!==epoch)return;
      // Wait for cancelled jobs to settle before beginning a fresh handshake.
      await queue;if(generation!==epoch)return;
      await request(SX.GET_VERSION,[],SX.VERSION);
      const p=await request(SX.GET_CONFIG,[],SX.CONFIG);renderConfig(p);
      if(generation!==epoch)return;
      connecting=false;setReady(true);edits();setStatus(`Connected to ${output.name||'Teletype'}`,true);
      $('stage-caption').textContent='Play your Teletype. The display follows incoming MIDI.';
      log('Listening to Teletype. The monitor sends no MIDI.','success');
    }catch(error){if(generation!==epoch)return;resetConnection('Unable to connect');log(error.message,'error');}
  }
  $('connect').addEventListener('click',async()=>{
    if(!navigator.requestMIDIAccess){log('Web MIDI is unavailable. Open this page in Chrome or Edge over HTTPS or localhost.','error');return;}
    $('connect').disabled=true;
    try{
      access=await navigator.requestMIDIAccess({sysex:true});
      access.onstatechange=()=>{
        if(input?.state==='disconnected'||output?.state==='disconnected'){resetConnection('Teletype disconnected');log('USB connection lost. Reconnect when the controller is ready.','error');}
        populate();
      };
      if(!populate().length)throw Error('No matching MIDI input/output pair. Connect one Teletype; disconnect devices with identical names.');
      await bind();
    }catch(error){resetConnection('Unable to connect');log(error.message,'error');}
  });
  $('disconnect').addEventListener('click',()=>{if(access)access.onstatechange=null;resetConnection();log('Disconnected.');});
  $('device').addEventListener('change',()=>{if(dirty.size&&!confirm('Switch device and discard unapplied edits?')){$('device').value=output?.id||'';return;}bind();});
  function numeric(id,min,max){const text=$(id).value.trim(),value=Number(text);if(text===''||!Number.isInteger(value)||value<min||value>max){$(id).focus();throw Error(`Enter a whole number from ${min} to ${max}.`);}return value;}
  function setAction(button,action){button.addEventListener('click',()=>{if(!ready)return;try{const job=action();enqueue(job);}catch(error){log(error.message,'error');}});}
  for(let bank=0;bank<4;bank++){
    const section=document.createElement('section');section.className='mapping-bank';section.innerHTML=`<h4>Bank ${bank+1}</h4>`;
    for(let key=0;key<5;key++){
      const i=bank*5+key,id=`note-${i}`,row=document.createElement('div');row.className='mapping-row';
      row.innerHTML=`<label for="${id}">Key ${key+1}</label><input id="${id}" aria-label="Bank ${bank+1} key ${key+1} note" type="number" min="0" max="127" value="${36+i}"><button type="button" aria-label="Set bank ${bank+1} key ${key+1}">Set</button>`;
      section.append(row);row.querySelector('input').addEventListener('input',()=>mark(id));
      setAction(row.querySelector('button'),()=>{const value=numeric(id,0,127);return async()=>{await request(SX.SET_BTN_NOTE,[i+1,value]);config.notes[i]=value;if(Number($(id).value)===value)clean(id);view.setConfig(config);log(`Bank ${bank+1}, key ${key+1}: note ${value} applied.`,'success');};});
    }
    $('button-mappings').append(section);
  }
  for(let i=0;i<4;i++){
    const row=document.createElement('div');row.className='pot-mapping';row.id=`pot-mapping-${i}`;
    row.innerHTML=`<strong>Knob ${i+1}</strong><select id="type-${i}" aria-label="Knob ${i+1} message type"><option value="0">CC</option><option value="1">Pitch bend</option></select><input id="cc-${i}" type="number" min="0" max="127" value="${[1,2,74,71][i]}" aria-label="Knob ${i+1} CC number"><label><input id="mute-config-${i}" type="checkbox">Mute</label><button type="button" aria-label="Set knob ${i+1}">Set</button>`;
    $('pot-mappings').append(row);
    for(const el of row.querySelectorAll('input,select'))el.addEventListener('input',()=>{mark(row.id);$(`cc-${i}`).disabled=+$(`type-${i}`).value===1;});
    setAction(row.querySelector('button'),()=>{
      const type=+$(`type-${i}`).value,cc=type?0:numeric(`cc-${i}`,0,127),mute=$(`mute-config-${i}`).checked;
      return async()=>{
        await request(SX.SET_POT,[i+1,type,cc]);config.types[i]=type;if(!type)config.cc[i]=cc;view.setConfig(config);
        await request(SX.SET_POT_IGNORE,[i+1,+mute]);config.flags=mute?config.flags|(1<<i):config.flags&~(1<<i);
        if(+$(`type-${i}`).value===type&&(type||Number($(`cc-${i}`).value)===cc)&&$(`mute-config-${i}`).checked===mute)clean(row.id);
        view.setConfig(config);log(`Knob ${i+1} mapping applied.`,'success');
      };
    });
  }
  for(const [id,button,cmd,field] of [['button-channel','set-button-channel',SX.SET_BTN_CH,'buttonChannel'],['pot-channel','set-pot-channel',SX.SET_POT_CH,'potChannel']]){
    $(id).addEventListener('change',()=>mark(id));setAction($(button),()=>{const value=numeric(id,1,16);return async()=>{await request(cmd,[value]);config[field]=value;if(Number($(id).value)===value)clean(id);view.setConfig(config);log(`MIDI channel ${value} applied.`,'success');};});
  }
  $('save').addEventListener('click',()=>{if(!ready)return;if(dirty.size){log('Click Set for each unapplied edit before saving.','error');return;}enqueue(async()=>{await request(SX.SAVE);log('Settings saved to Teletype.','success');});});
  for(const [id,cmd,text] of [['load',SX.LOAD,'Load saved settings and discard unsaved edits?'],['reset',SX.RESET,'Restore factory defaults? Save afterward to keep them.']])$(id).addEventListener('click',()=>{
    if(!ready||!confirm(text))return;
    enqueue(async()=>{await request(cmd);const p=await request(SX.GET_CONFIG,[],SX.CONFIG);renderConfig(p);log(id==='load'?'Saved configuration loaded.':'Factory defaults applied. Save to keep them.','success');});
  });
  $('clear-log').addEventListener('click',()=>{$('console').replaceChildren();});
  const tabs=['monitor','configure'];
  function showTab(name){for(const tab of tabs){const active=tab===name;$(tab).hidden=!active;$(`tab-${tab}`).setAttribute('aria-selected',String(active));$(`tab-${tab}`).tabIndex=active?0:-1;}}
  tabs.forEach((name,i)=>{
    $(`tab-${name}`).addEventListener('click',()=>showTab(name));
    $(`tab-${name}`).addEventListener('keydown',e=>{if(['ArrowLeft','ArrowRight','Home','End'].includes(e.key)){e.preventDefault();const next=e.key==='Home'?0:e.key==='End'?1:1-i;showTab(tabs[next]);$(`tab-${tabs[next]}`).focus();}});
  });
  $('help-open').addEventListener('click',()=>$('help').showModal());
  window.addEventListener('pagehide',()=>{if(access)access.onstatechange=null;resetConnection();});
})();
