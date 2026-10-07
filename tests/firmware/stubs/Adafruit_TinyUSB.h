#pragma once
#include "Arduino.h"
struct Adafruit_USBD_MIDI {void setCableName(int,const char*){}};
struct FakeUsb{void setManufacturerDescriptor(const char*){} void setProductDescriptor(const char*){}};
inline FakeUsb USBDevice;
inline int testUsbInitCalls=0, testUsbDetachCalls=0, testUsbAttachCalls=0;
inline bool testUsbMounted=true;
struct FakeTinyUsb {
  bool mounted(){return testUsbMounted;}
  void detach(){testUsbDetachCalls++;}
  void attach(){testUsbAttachCalls++;}
};
inline FakeTinyUsb TinyUSBDevice;
inline void TinyUSB_Device_Init(int){testUsbInitCalls++;}
