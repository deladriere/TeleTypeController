#pragma once
#include "Arduino.h"
constexpr int MIDI_CHANNEL_OMNI=0;
struct MidiEvent {int kind,a,b,ch;};
struct FakeMidi{
  std::vector<MidiEvent> events;
  std::vector<std::vector<byte>> sysex;
  bool thru=true;
  void setHandleSystemExclusive(void(*)(byte*,unsigned)){}
  void begin(int){} void turnThruOff(){thru=false;} void read(){}
  void sendNoteOn(byte n,byte v,byte c){events.push_back({0x90,n,v,c});}
  void sendNoteOff(byte n,byte v,byte c){events.push_back({0x80,n,v,c});}
  void sendControlChange(byte n,byte v,byte c){events.push_back({0xB0,n,v,c});}
  void sendPitchBend(int n,byte c){events.push_back({0xE0,n,0,c});}
  void sendSysEx(unsigned n,const byte* p,bool){sysex.emplace_back(p,p+n);}
};
#define MIDI_CREATE_INSTANCE(Type,Port,Name) FakeMidi Name
