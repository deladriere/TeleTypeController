#include <cassert>
#include <iostream>
#include "../../src/main.cpp"
void command(byte cmd,std::initializer_list<byte> payload={}){
  std::vector<byte> bytes={0xF0,SYSEX_MFG,SYSEX_DEV0,SYSEX_DEV1,cmd};
  bytes.insert(bytes.end(),payload.begin(),payload.end());bytes.push_back(0xF7);
  handleSysEx(bytes.data(),bytes.size());
}
void clear(){USB_MIDI.events.clear();USB_MIDI.sysex.clear();SERIAL_MIDI.events.clear();SERIAL_MIDI.sysex.clear();}
bool has(byte cmd,std::initializer_list<byte> data){
  std::vector<byte> wanted={0xF0,SYSEX_MFG,SYSEX_DEV0,SYSEX_DEV1,cmd};
  wanted.insert(wanted.end(),data.begin(),data.end());wanted.push_back(0xF7);
  return std::find(USB_MIDI.sysex.begin(),USB_MIDI.sysex.end(),wanted)!=USB_MIDI.sysex.end();
}
int main(){
  for(int i=0;i<30;i++){testDigital[i]=HIGH;testAnalog[i]=512;}
  TinyUSB_Device_Init(0); // Arduino-Pico calls this before the sketch.
  setup();assert(std::string(PICO_VERSION)=="v1.0.1");
  assert(testUsbInitCalls==1); // setup must not clear the core's CDC interface.
  assert(testUsbDetachCalls==1&&testUsbAttachCalls==1);
  // Configuration queries produce no performance output.
  clear();command(SX_GET_VERSION);command(SX_GET_CONFIG);
  assert(USB_MIDI.sysex.size()==2&&USB_MIDI.events.empty()&&SERIAL_MIDI.events.empty());
  // Physical switches choose the bank and custom key mapping on both outputs.
  testDigital[BANK_SEL_PIN0]=LOW;config.buttonNotes[10]=90;config.btnChannel=4;
  clear();testDigital[BUTTON_PIN1]=LOW;Scan_User();testMillis+=51;Scan_User();
  assert(SERIAL_MIDI.events.size()==2&&USB_MIDI.events.size()==2);
  assert(SERIAL_MIDI.events[0].kind==0x90&&SERIAL_MIDI.events[0].a==90&&SERIAL_MIDI.events[0].ch==4);
  assert(SERIAL_MIDI.events[1].kind==0x80&&SERIAL_MIDI.events[1].a==90);
  assert(USB_MIDI.sysex.empty()&&SERIAL_MIDI.sysex.empty());
  // The withdrawn remote controls and feedback query are unsupported.
  for(byte cmd:{0x03,0x30,0x31,0x32}){
    clear();command(cmd,{1});assert(has(SX_ACK,{cmd,SX_STATUS_ERR}));
    assert(USB_MIDI.events.empty()&&SERIAL_MIDI.events.empty());
  }
  // Physical pots keep their original CC, pitch bend and mute behavior.
  config.potChannel=8;config.potValues[0]=7;config.potTypes[2]=POT_TYPE_PITCHBEND;
  setPotIgnored(1,true);clear();Scan_Pots();
  assert(SERIAL_MIDI.events.size()==3&&USB_MIDI.events.size()==3);
  assert(SERIAL_MIDI.events[0].kind==0xB0&&SERIAL_MIDI.events[0].a==7&&SERIAL_MIDI.events[0].ch==8);
  assert(SERIAL_MIDI.events[1].kind==0xE0);
  assert(USB_MIDI.sysex.empty()&&SERIAL_MIDI.sysex.empty());
  command(SX_SET_BTN_NOTE,{1,60});command(SX_SAVE);
  config.buttonNotes[0]=61;command(SX_LOAD);assert(config.buttonNotes[0]==60);
  testUsbMounted=false;setup();
  assert(testUsbInitCalls==1&&testUsbDetachCalls==1&&testUsbAttachCalls==1);
  std::cout<<"USB startup regression, physical MIDI, configuration, mute and rejection of withdrawn commands passed.\n";
}
