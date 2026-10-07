#pragma once
#include "Arduino.h"
struct FakeEeprom{
  std::vector<byte> data=std::vector<byte>(512,255);
  bool commitOk=true;
  void begin(int){} byte read(unsigned i){return data.at(i);}
  void write(unsigned i,byte v){data.at(i)=v;} bool commit(){return commitOk;}
};
inline FakeEeprom EEPROM;
