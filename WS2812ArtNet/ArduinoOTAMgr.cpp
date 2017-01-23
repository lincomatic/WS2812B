#include "./ArduinoOTAMgr.h"

void ArduinoOTAMgr::boot(const char *hostname,const char *pw)
{
  Serial.println("OTA boot");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  if (hostname) ArduinoOTA.setHostname(hostname);

  // No authentication by default
  if (pw) ArduinoOTA.setPassword(pw);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  
  _enabled = 1;

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void ArduinoOTAMgr::handle()
{
  if (_enabled) ArduinoOTA.handle();
}
