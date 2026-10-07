// Browser-only appearance preference. This module has no MIDI/device access.
(() => {
  const palettes={red:{name:'Red',h:0,s:65,l:55,accent:'#ff8585'},beige:{name:'Beige',h:40,s:31,l:73,accent:'#ddcbaa'},black:{name:'Black',h:218,s:9,l:17,accent:'#b9c3cf'},white:{name:'White',h:60,s:7,l:89,accent:'#e9e9e2'},blue:{name:'Blue',h:200,s:94,l:46,accent:'#42b6fa'},green:{name:'Green',h:147,s:49,l:41,accent:'#70d39a'},orange:{name:'Orange',h:29,s:86,l:53,accent:'#ffb568'}};
  const tones=["#24b5fa", "#079ae3", "#0785cf", "#0a9de7", "#0868b0", "#077dc3", "#064c87", "#1dbaff", "#087acd", "#215876", "#74d9ff", "#a4e4ff", "#02070c", "#064474", "#052e51", "#0870b4", "#1ca2e8", "#084d7f", "#030a11", "#02070b", "#0b131b", "#79dfff", "#108ed2", "#56c8ff", "#117fc0", "#078dd0", "#30b5f6", "#116da4", "#159de0", "#43bdf5", "#169fe5", "#41b9f0", "#111c28", "#3794c6", "#06101c", "#1770a8", "#121c27", "#071019", "#111d29", "#02314f", "#070e17", "#048cdb", "#66cdff", "#b9edff", "#088fd9"];
  const root=document.documentElement,buttons=[...document.querySelectorAll('.color-swatch[data-device-color]')];
  const key='teletype.deviceColor';
  const valid=name=>Object.prototype.hasOwnProperty.call(palettes,name);
  const lightness=hex=>{const rgb=[1,3,5].map(i=>parseInt(hex.slice(i,i+2),16)/255);return (Math.max(...rgb)+Math.min(...rgb))*50;};
  const reference=lightness('#079ae3');
  function apply(name,persist){
    if(!valid(name))name='blue';
    const p=palettes[name];
    tones.forEach((original,i)=>{
      const source=lightness(original);
      const l=source<reference?p.l*source/reference:p.l+(100-p.l)*(source-reference)/(100-reference);
      root.style.setProperty(`--device-tone-${i}`,name==='blue'?original:`hsl(${p.h} ${p.s}% ${l}%)`);
    });
    const rgb=[1,3,5].map(i=>parseInt(p.accent.slice(i,i+2),16));
    root.style.setProperty('--blue',p.accent);
    root.style.setProperty('--accent-soft',`rgba(${rgb.join(',')},.14)`);
    root.style.setProperty('--accent-line',`rgba(${rgb.join(',')},.4)`);
    root.dataset.deviceColor=name;
    document.getElementById('device-color-name').textContent=p.name;
    document.getElementById('device-description').textContent=`${p.name} enclosure in a three-quarter view, five black keys, four black knobs with ${p.name.toLowerCase()} caps and two black slide switches. Read-only display of incoming MIDI. Bank switches show the bank inferred from the last uniquely mapped note.`;
    buttons.forEach(button=>{const active=button.dataset.deviceColor===name;button.setAttribute('aria-checked',String(active));button.tabIndex=active?0:-1;});
    if(persist){try{localStorage.setItem(key,name);}catch{/* Appearance still works when browser storage is unavailable. */}}
  }
  buttons.forEach((button,i)=>{
    button.addEventListener('click',()=>apply(button.dataset.deviceColor,true));
    button.addEventListener('keydown',event=>{
      const step=['ArrowRight','ArrowDown'].includes(event.key)?1:['ArrowLeft','ArrowUp'].includes(event.key)?-1:0;
      if(!step&&event.key!=='Home'&&event.key!=='End')return;
      event.preventDefault();const next=event.key==='Home'?0:event.key==='End'?buttons.length-1:(i+step+buttons.length)%buttons.length;
      buttons[next].focus();apply(buttons[next].dataset.deviceColor,true);
    });
  });
  let saved='blue';try{saved=localStorage.getItem(key)||'blue';}catch{/* Keep the default when storage is unavailable. */}
  apply(saved,false);
})();
