#pragma once

#include <ArduinoOTA.h>

class ArduinoOTAMgr {
  uint8_t _enabled;
 public:
  ArduinoOTAMgr() { _enabled = 0; }

  void boot(const char *hostname=NULL,const char *pw=NULL);
  void disable() { _enabled = 0; }
  void enable() { _enabled = 1; }
  void handle();
};
