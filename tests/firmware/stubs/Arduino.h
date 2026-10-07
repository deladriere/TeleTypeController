#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>
using byte=uint8_t;
constexpr int HIGH=1, LOW=0, INPUT_PULLUP=2, A0=26, A1=27, A2=28, A3=29;
inline unsigned long testMillis=0;
inline int testDigital[30]={};
inline int testAnalog[30]={};
inline unsigned long millis(){return testMillis;}
inline void delay(unsigned long ms){testMillis+=ms;}
inline void delayMicroseconds(unsigned long){}
inline void pinMode(int,int){}
inline int digitalRead(int pin){return testDigital[pin];}
inline int analogRead(int pin){return testAnalog[pin];}
inline long map(long x,long a,long b,long c,long d){return (x-a)*(d-c)/(b-a)+c;}
template<typename T> T constrain(T x,T a,T b){return std::min(b,std::max(a,x));}
struct HardwareSerial{void setTX(int){}};
inline HardwareSerial Serial1;
