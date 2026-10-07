// Passive MIDI display. There are no gesture handlers or outbound commands here.
(() => {
  const $=id=>document.getElementById(id), svg=$('instrument'), timers=new Map();
  const bankHint='Bank is inferred from the last uniquely mapped note; switch movements alone send no MIDI.';
  function inspect(name,value,unit,message,assignment,channel,progress=0){
    $('selected-name').textContent=name;$('selected-value').textContent=value;
    $('selected-unit').textContent=unit;$('message-type').textContent=message;
    $('assignment').textContent=assignment;$('selected-channel').textContent=channel;
    $('value-fill').style.width=`${progress}%`;
  }
  function highlight(pots=[]){
    document.querySelectorAll('[data-control]').forEach(el=>el.classList.toggle('selected',pots.includes(+el.dataset.control)));
    svg.querySelectorAll('[data-knob]').forEach(el=>el.classList.toggle('selected',pots.includes(+el.dataset.knob)));
  }
  function drawPot(i,value){
    const known=value!==null, position=known?value:64;
    $(`range-${i}`).value=known?value:0;$(`range-${i}`).classList.toggle('unknown',!known);
    $(`value-${i}`).textContent=known?value:'—';
    const knob=svg.getElementById(`knob-${i+1}`);
    knob.classList.toggle('unknown',!known);
    knob.setAttribute('aria-label',`Knob ${i+1}: ${known?value:'not yet received'}`);
    // The 270° sweep starts at the front-left and ends at the front-right.
    const angle=225-position/127*270;
    svg.getElementById(`knob-marker-${i+1}`).setAttribute('transform',`rotate(${angle+90})`);
    const a=angle*Math.PI/180,x=52+i*20,y=92;
    const project=(r,z,t)=>{const px=x+r*Math.cos(t),py=y+r*Math.sin(t);return `${95+px*3.5+py*1.3},${435+px*.8-py*1.7-z*3.4}`;};
    const stripe=svg.getElementById(`knob-stripe-${i+1}`);
    stripe.setAttribute('points',[project(6.65,51.3,a-.1),project(6.65,51.3,a+.1),project(5.72,64.1,a+.1),project(5.72,64.1,a-.1)].join(' '));
    stripe.style.visibility=(.8*Math.cos(a)-1.7*Math.sin(a))>0?'visible':'hidden';
  }
  function drawBank(bank){
    $('bank-number').textContent=bank===null?'—':String(bank+1).padStart(2,'0');
    document.querySelectorAll('[data-bank]').forEach(el=>el.classList.toggle('active',+el.dataset.bank===bank));
    for(let i=0;i<2;i++){
      const el=svg.getElementById(`switch-${i+1}`),on=bank!==null&&Boolean(bank&(1<<i));
      el.classList.toggle('unknown',bank===null);
      el.setAttribute('aria-label',bank===null?'Bank switch position unknown':`Bank switch ${i+1}: inferred from last note`);
      svg.getElementById(`switch-lever-${i+1}`).setAttribute('transform',on?'translate(3.1 -4.1)':'translate(0 0)');
    }
  }
  function reset(){
    for(const timer of timers.values())clearTimeout(timer);timers.clear();
    svg.querySelectorAll('[data-key]').forEach(el=>el.classList.remove('active'));
    for(let i=0;i<4;i++)drawPot(i,null);
    drawBank(null);highlight();inspect('Waiting for MIDI','—','','—','—','—');
    $('bank-hint').textContent=bankHint;
  }
  window.TeletypeView={
    setConfig(config){
      reset();for(let i=0;i<4;i++){$(`meta-${i}`).textContent=config.types[i]?'PITCH BEND':`CC ${config.cc[i]}`;$(`mute-${i}`).textContent=(config.flags&(1<<i))?'MUTED':'';}
    },
    receive(m){
      if(m.kind==='note'){
        highlight();
        if(m.slots.length===1){
          const slot=m.slots[0],key=slot%5,bank=Math.floor(slot/5),el=svg.getElementById(`key-${key+1}`);
          drawBank(bank);$('bank-hint').textContent=bankHint;
          inspect(`Key ${key+1}`,m.note,'NOTE','Note trigger',`Bank ${bank+1}`,m.channel,100);
          el.classList.add('active');clearTimeout(timers.get(key));
          timers.set(key,setTimeout(()=>{el.classList.remove('active');timers.delete(key);},180));
        }else{
          drawBank(null);
          inspect(`Note ${m.note}`,m.note,'NOTE','Note trigger',m.slots.length?'Shared mapping':'Unmapped',m.channel,100);
          $('bank-hint').textContent=m.slots.length?'This note is assigned to several keys. MIDI cannot identify which key or bank sent it.':bankHint;
        }
      }else{
        highlight(m.pots);m.pots.forEach(i=>drawPot(i,m.value));
        const name=m.pots.length===1?`Knob ${m.pots[0]+1}`:m.pots.length?'Shared mapping':'MIDI message';
        inspect(name,m.kind==='bend'?m.bend:m.value,m.kind==='bend'?'PB':'/ 127',m.kind==='bend'?'Pitch bend':'Control change',
          m.pots.length>1?`Knobs ${m.pots.map(i=>i+1).join(', ')}`:m.kind==='bend'?'Pitch bend':`CC ${m.cc}`,m.channel,m.value/127*100);
      }
    },reset
  };
  reset();
})();
