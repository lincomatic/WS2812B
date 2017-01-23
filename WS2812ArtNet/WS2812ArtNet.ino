/*
Hacked by SCL from
 https://github.com/natcl/Artnet/blob/master/examples/ArtnetNeoPixel/ArtnetNeoPixel.ino
 https://github.com/overflo23/ESP8266_ARTNET_WS2801/tree/master/artnet_esp

 This example will receive multiple universes via Artnet and control a strip of ws2811 leds
*/

#include <eagle_soc.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "./ArtnetWifi.h"
#include "./ArduinoOTAMgr.h"

//
// --- begin configuration
//
#define WIFI_MGR // use AP mode to configure via captive portal
#define OTA_UPDATE // OTA firmware update

#define PIXEL_CNT 148 //16 // number of LED's

#define PIN_DATA  14 
#define PIN_LED    0
// n.b. pin 15 internal pullup doesn't seem to work so shouldn't be used for reset
#define PIN_FACTORY_RESET 12 // ground this pin to wipe out EEPROM & WiFi settings

#define AP_PREFIX "WS2812ArtNet_"

// OTA_UPDATE
#define OTA_HOST "WS2812ArtNet"
#define OTA_PASS "ws2812artnet"

#ifndef WIFI_MGR
//Wifi settings
const char* ssid = "YOUR-SSID";
const char* password = "YOUR-PASSPHRASE";
#endif // !WIFI_MGR


//
// --- end configuration
//

#define CHANNEL_CNT (PIXEL_CNT*3) // Total number of channels you want to receive (1 led = 3 channels)
#define BYTE_CNT (PIXEL_CNT*3)

// Artnet settings
int startUniverse = 0; 
const int maxUniverses = CHANNEL_CNT / 512 + ((CHANNEL_CNT % 512) ? 1 : 0);

#ifdef OTA_UPDATE
ArduinoOTAMgr AOTAMgr;
#endif

typedef struct pixel_grb {
  uint8_t g;
  uint8_t r;
  uint8_t b;
} PIXEL_GRB;
PIXEL_GRB g_BlkPixel = {0,0,0};

class WS2812Strand {
  PIXEL_GRB pixelBuffer[PIXEL_CNT];
  uint8_t *displayBuffer;
  uint32_t endTime;       // Latch timing reference
  uint8_t dataPin;

  inline bool canShow(void) { return (micros() - endTime) >= 50L; }
public:
  WS2812Strand(uint8_t datapin);

  void begin();
  void fill(PIXEL_GRB *pixel);
  void show();
  uint8_t *getDisplayBuffer() { return displayBuffer; }
  void setPixel(uint16_t idx,PIXEL_GRB *p);
  PIXEL_GRB *getPixel(uint16_t idx) { return pixelBuffer + idx; }
};

WS2812Strand::WS2812Strand(uint8_t datapin)
{
  displayBuffer = (uint8_t *)pixelBuffer;
  dataPin = datapin;
  endTime = 0;
}

void WS2812Strand::begin()
{
  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  endTime = micros();
}

void WS2812Strand::fill(PIXEL_GRB *pixel)
{
  for (uint8_t i=0;i < PIXEL_CNT;i++) {
    pixelBuffer[i] = *pixel;
  }
}

void WS2812Strand::setPixel(uint16_t idx,PIXEL_GRB *p)
{
  if (idx < PIXEL_CNT) {
    pixelBuffer[idx] = *p;
  }
}


#ifdef ESP8266
// ESP8266 show() is external to enforce ICACHE_RAM_ATTR execution
extern "C" void ICACHE_RAM_ATTR espShow(
  uint8_t pin, uint8_t *pixels, uint32_t numBytes, uint8_t type);
#endif // ESP8266

void WS2812Strand::show()
{
  // Data latch = 50+ microsecond pause in the output stream.  Rather than
  // put a delay at the end of the function, the ending time is noted and
  // the function will simply hold off (if needed) on issuing the
  // subsequent round of data until the latch time has elapsed.  This
  // allows the mainline code to start generating the next frame of data
  // rather than stalling for the latch.
  while(!canShow());
  espShow(dataPin,displayBuffer,BYTE_CNT,1);
  endTime = micros(); // Save EOD time for latch on next call
}


#define BTN_PRESS_SHORT 50  // ms
#define BTN_PRESS_LONG 2000 // ms

#define BTN_STATE_OFF   0
#define BTN_STATE_SHORT 1 // short press
#define BTN_STATE_LONG  2 // long press

class Btn {
  uint8_t btnGpio;
  uint8_t buttonState;
  unsigned long lastDebounceTime;  // the last time the output pin was toggled
  
public:
  Btn(uint8_t gpio);
  void init();

  void read();
  uint8_t shortPress();
  uint8_t longPress();
};

Btn::Btn(uint8_t gpio)
{
  btnGpio = gpio;
  buttonState = BTN_STATE_OFF;
  lastDebounceTime = 0;
}

void Btn::init()
{
  pinMode(btnGpio,INPUT_PULLUP);
}

void Btn::read()
{
  uint8_t sample;
  unsigned long delta;
  sample = digitalRead(btnGpio) ? 0 : 1;
  if (!sample && (buttonState == BTN_STATE_LONG) && !lastDebounceTime) {
    buttonState = BTN_STATE_OFF;
  }
  if ((buttonState == BTN_STATE_OFF) ||
      ((buttonState == BTN_STATE_SHORT) && lastDebounceTime)) {
    if (sample) {
      if (!lastDebounceTime && (buttonState == BTN_STATE_OFF)) {
	lastDebounceTime = millis();
      }
      delta = millis() - lastDebounceTime;

      if (buttonState == BTN_STATE_OFF) {
	if (delta >= BTN_PRESS_SHORT) {
	  buttonState = BTN_STATE_SHORT;
	}
      }
      else if (buttonState == BTN_STATE_SHORT) {
	if (delta >= BTN_PRESS_LONG) {
	  buttonState = BTN_STATE_LONG;
	}
      }
    }
    else { //!sample
      lastDebounceTime = 0;
    }
  }
}

uint8_t Btn::shortPress()
{
  if ((buttonState == BTN_STATE_SHORT) && !lastDebounceTime) {
    buttonState = BTN_STATE_OFF;
    return 1;
  }
  else {
    return 0;
  }
}

uint8_t Btn::longPress()
{
  if ((buttonState == BTN_STATE_LONG) && lastDebounceTime) {
    lastDebounceTime = 0;
    return 1;
  }
  else {
    return 0;
  }
}

#ifdef WIFI_MGR
#include "./WiFiManager.h"          // https://github.com/tzapu/WiFiManager

typedef struct config_parms {
int startUniverse;
  char artnet_universe[3];
  char staticIP[16]; // optional static IP
  char staticGW[16]; // optional static gateway
  char staticNM[16]; // optional static netmask
} CONFIG_PARMS;




//flag for saving data

class WifiConfigurator {
  CONFIG_PARMS configParms;
  // a flag for ip setup
  boolean set_static_ip;

  //network stuff
  //default custom static IP, changeable from the webinterface
  void resetConfigParms() { memset(&configParms,0,sizeof(configParms)); }
public:
  static bool shouldSaveConfig;

  WifiConfigurator();

  void StartManager(void);
  String getUniqueSystemName();
  void printMac();
  void readCfg();
  void resetCfg(int dowifi=0);
  uint8_t getConfigSize() { return sizeof(configParms); }
};


bool WifiConfigurator::shouldSaveConfig; // instantiate

WifiConfigurator::WifiConfigurator()
{
  resetConfigParms();

  set_static_ip = false;
}



//creates the string that shows if the device goes into accces point mode
#define WL_MAC_ADDR_LENGTH 6
String WifiConfigurator::getUniqueSystemName()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);


  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String("-") + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);

  macID.toUpperCase();
  String UniqueSystemName = String(AP_PREFIX) + macID;

  return UniqueSystemName;
}





// displays mac address on serial port
void WifiConfigurator::printMac()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);

  Serial.print("MAC: ");
  for (int i = 0; i < 5; i++){
    Serial.print(mac[i], HEX);
    Serial.print(":");
  }
  Serial.println(mac[5],HEX);
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  WifiConfigurator::shouldSaveConfig = true;
}


void WifiConfigurator::StartManager(void)
{
  Serial.println("StartManager() called");

  shouldSaveConfig = false;

  // add parameter for artnet setup in GUI
  WiFiManagerParameter custom_artnet_universe("artnet_universe", "Art-Net Universe (Default: 0)", configParms.artnet_universe, sizeof(configParms.artnet_universe));

  // add parameters for IP setup in GUI
  WiFiManagerParameter custom_ip("ip", "Static IP (Blank for DHCP)", configParms.staticIP, sizeof(configParms.staticIP));
  WiFiManagerParameter custom_gw("gw", "Static Gateway (Blank for DHCP)", configParms.staticGW, sizeof(configParms.staticGW));
  WiFiManagerParameter custom_nm("nm", "Static Netmask (Blank for DHCP)", configParms.staticNM, sizeof(configParms.staticNM));
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // this is what is called if the webinterface want to save data, callback is right above this function and just sets a flag.
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // this actually adds the parameters defined above to the GUI
  wifiManager.addParameter(&custom_artnet_universe);

  // if the flag is set we configure a STATIC IP!
  if (set_static_ip) {
    //set static ip
    IPAddress _ip, _gw, _nm;
    _ip.fromString(configParms.staticIP);
    _gw.fromString(configParms.staticGW);
    _nm.fromString(configParms.staticNM);
    
    // this adds 3 fields to the GUI for ip, gw and netmask, but IP needs to be defined for this fields to show up.
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _nm);
    
    Serial.println("Setting IP to:");
    Serial.print("IP: ");
    Serial.println(configParms.staticIP);
    Serial.print("GATEWAY: ");
    Serial.println(configParms.staticIP);
    Serial.print("NETMASK: ");
    Serial.println(configParms.staticNM);
  }
  else {
    // i dont want to fill these fields per default so i had to implement this workaround .. its ugly.. but hey. whatever.  
    wifiManager.addParameter(&custom_ip);
    wifiManager.addParameter(&custom_gw);
    wifiManager.addParameter(&custom_nm);
  }

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep in seconds
  // also really annoying if you just connected and the damn thing resets in the middle of filling in the GUI..
  wifiManager.setConfigPortalTimeout(10*60);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(getUniqueSystemName().c_str())) {
    Serial.println("timed out and failed to connect");
    Serial.println("rebooting...");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    //   we never get here.
  }

  //everything below here is only executed once we are connected to a wifi.

  //if you get here you have connected to the WiFi

  digitalWrite(PIN_LED,LOW); // turn on LED
  Serial.print("CONNECTED to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // this is true if we come from the GUI and clicked "save"
  // the flag is created in the callback above..
  if (shouldSaveConfig) {
    // connection worked so lets save all those parameters to the config file
    strcpy(configParms.artnet_universe, custom_artnet_universe.getValue());
    if (*configParms.artnet_universe) {
      startUniverse = atoi(configParms.artnet_universe);
    }
    else {
      startUniverse = 0;
    }
    strcpy(configParms.staticIP, custom_ip.getValue());
    strcpy(configParms.staticGW, custom_gw.getValue());
    strcpy(configParms.staticNM, custom_nm.getValue());
    
    // if we defined something in the gui before that does not work we might to get rid of previous settings in the config file
    // so if the form is transmitted empty, delete the entries.
    //if (strlen(ip) < 8) {
    //      resetConfigParms();
    //    }
    
    
    Serial.println("saving config");
    
    int eepidx = 0;
    int i;
    EEPROM.write(eepidx++,strlen(configParms.artnet_universe));
    for (i=0;i < (int)strlen(configParms.artnet_universe);i++) {
      EEPROM.write(eepidx++,configParms.artnet_universe[i]);
    }

    EEPROM.write(eepidx++,strlen(configParms.staticIP));
    for (i=0;i < (int)strlen(configParms.staticIP);i++) {
      EEPROM.write(eepidx++,configParms.staticIP[i]);
    }

    EEPROM.write(eepidx++,strlen(configParms.staticGW));
    for (i=0;i < (int)strlen(configParms.staticGW);i++) {
      EEPROM.write(eepidx++,configParms.staticGW[i]);
    }

    EEPROM.write(eepidx++,strlen(configParms.staticNM));
    for (i=0;i < (int)strlen(configParms.staticNM);i++) {
      EEPROM.write(eepidx++,configParms.staticNM[i]);
    }

    EEPROM.commit();

    Serial.print("artnet_universe: ");
    Serial.println(*configParms.artnet_universe ? configParms.artnet_universe : "(default)");
    Serial.print("IP: ");
    Serial.println(*configParms.staticNM ? configParms.staticNM : "(DHCP)");
    Serial.print("GW: ");
    Serial.println(*configParms.staticGW ? configParms.staticGW : "(DHCP)");
    Serial.print("NM: ");
    Serial.println(*configParms.staticNM ? configParms.staticNM : "(DHCP)");
    //end save
  }
}


void WifiConfigurator::resetCfg(int dowifi)
{
  Serial.print("resetCfg()");

  // reset config parameters
  resetConfigParms();

  // clear EEPROM
  EEPROM.write(0,0);
  EEPROM.commit();

  if (dowifi) {
    // erase WiFi settings (SSID/passphrase/etc
    WiFiManager wifiManager;
    wifiManager.resetSettings();
  }
}

void WifiConfigurator::readCfg()
{
  int resetit = 0;

  int eepidx = 0;
  int i;

  resetConfigParms();

  uint8_t len = EEPROM.read(eepidx++);
  if ((len == 0) || (len >= sizeof(configParms.artnet_universe))) {
    // assume uninitialized
    resetit = 1;
  }
  else {
    for (i=0;i < len;i++) {
      configParms.artnet_universe[i] = EEPROM.read(eepidx++);
    }
    configParms.artnet_universe[i] = 0;

    len = EEPROM.read(eepidx++);
    if ((len > 0) && (len < sizeof(configParms.staticIP))) {
      for (i=0;i < len;i++) {
	configParms.staticIP[i] = EEPROM.read(eepidx++);
      }
      configParms.staticIP[i] = 0;
    }
    else {
      resetit = 1;
    }

    if (!resetit) {
      len = EEPROM.read(eepidx++);
      if ((len > 0) && (len < sizeof(configParms.staticGW))) {
	for (i=0;i < len;i++) {
	  configParms.staticGW[i] = EEPROM.read(eepidx++);
	}
	configParms.staticGW[i] = 0;
      }
      else {
	resetit = 1;
      }
    }

    if (!resetit) {
      len = EEPROM.read(eepidx++);
      if ((len > 0) && (len < sizeof(configParms.staticNM))) {
	for (i=0;i < len;i++) {
	  configParms.staticNM[i] = EEPROM.read(eepidx++);
	}
	configParms.staticNM[i] = 0;
      }
      else {
	resetit = 1;
      }
    }
  }

  if (!resetit) { // valid config
    startUniverse = atoi(configParms.artnet_universe);
    
    if (*configParms.staticIP) {
      // lets use the IP settings from the config file for network config.
      Serial.println("setting static ip from config");
      set_static_ip = 1;
    }
    else {
      Serial.println("using DHCP");
    }
  }
  else {
    resetCfg(0);
  }
}

WifiConfigurator wfCfg;
#endif // WIFI_MGR
WS2812Strand leds(PIN_DATA);
Btn btnReset(PIN_FACTORY_RESET);


ArtnetWifi artnet;
// Check if we got all universes
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;

#ifndef WIFI_MGR
// connect to wifi – returns true if successful or false if not
boolean ConnectWifi(void)
{
  boolean state = true;
  int i = 0;

  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");
  
  // Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false;
      break;
    }
    i++;
  }
  if (state){
    digitalWrite(0,LOW); // turn on onboard LED

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  
  return state;
}

#endif // !WIFI_MGR



void initTest()
{
  delay(100);
  leds.fill(&g_BlkPixel);
  leds.show();
  delay(100);

  PIXEL_GRB p;
  p.r = 127;
  p.g = 0;
  p.b = 0;
  leds.fill(&p);
  leds.show();
  delay(1000);

  p.r = 0;
  p.g = 127;
  p.b = 0;
  leds.fill(&p);
  leds.show();
  delay(1000);

  p.r = 0;
  p.g = 0;
  p.b = 127;
  leds.fill(&p);
  leds.show();
  delay(1000);

  leds.fill(&g_BlkPixel);
  leds.show();
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  digitalWrite(PIN_LED,LOW); // onboard LED on

  sendFrame = 1;
  /*
  // set brightness of the whole strip 
  if (universe == 15)
  {
    leds.setBrightness(data[0]);
    leds.show();
  }
  */

  // Store which universe has got in
  if ((universe - startUniverse) < maxUniverses)
    universesReceived[universe - startUniverse] = 1;

  for (int i = 0 ; i < maxUniverses ; i++)
  {
    if (universesReceived[i] == 0)
    {
      //Serial.println("Broke");
      sendFrame = 0;
      break;
    }
  }

  // read universe and put into the right part of the display buffer
  PIXEL_GRB *p = (PIXEL_GRB *)data;
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    leds.setPixel(led,p++);
  }
  previousDataLength = length;     
  
  if (sendFrame)
  {
    leds.show();
    // Reset universeReceived to 0
    memset(universesReceived, 0, maxUniverses);
  }

  digitalWrite(PIN_LED,HIGH); // onboard LED off
}

void factoryReset()
{
  Serial.println("Factory Reset");
  wfCfg.resetCfg(1);
  delay(1000);
  ESP.reset(); 
}

void setup()
{
  // onboard leds also outputs
  pinMode(PIN_LED, OUTPUT); // onboard LED
  digitalWrite(PIN_LED,HIGH); // turn off onboard LED
  Serial.begin(115200);

  btnReset.init();

#ifdef WIFI_MGR
  EEPROM.begin(wfCfg.getConfigSize());  

  // display the MAC on Serial
  wfCfg.printMac();

  wfCfg.readCfg();
  Serial.println("readCfg() done.");

  wfCfg.StartManager();
  Serial.println("StartManager() done.");
#else
  ConnectWifi();
#endif // WIFI_MGR

#ifdef OTA_UPDATE
  AOTAMgr.boot(OTA_HOST,OTA_PASS);
#endif


  Serial.print("Art-Net universe: ");
  Serial.println(startUniverse);
  Serial.println("Starting Art-Net");
  artnet.begin();
  leds.begin();
  initTest();

  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
}

void loop()
{
  btnReset.read();
  if (btnReset.longPress()) {
    Serial.println("long press");
    factoryReset();
  }


  // we call the read function inside the loop
  artnet.read();

#ifdef OTA_UPDATE
  AOTAMgr.handle();
#endif

}
