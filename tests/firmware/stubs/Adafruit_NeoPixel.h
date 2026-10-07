#pragma once
#include "Arduino.h"
constexpr int NEO_GRB=0,NEO_KHZ800=0;
struct Adafruit_NeoPixel{
  Adafruit_NeoPixel(int,int,int){}
  void begin(){} void setBrightness(int){} void clear(){} void show(){}
  void setPixelColor(int,uint32_t){}
};
