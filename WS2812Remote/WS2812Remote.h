#pragma once


//##############################################################################
//                                                                             #
// contains code hacked from
// Glediator to WS2812 pixel converter                                         #
// by R. Heller                                                                #
// V 1.0 - 07.01.2014                                                          #//            
// www.SolderLab.de                                                            #
//                                                                             #
// Maximum number of supported pixels is 512 !!!                               #
//                                                                             #
// In the Glediator software set output mode to "Glediator_Protocol",          #
// color order to "GRB" and baud rate to "1000000"                             #
//                                                                             #
//##############################################################################
//##############################################################################
//                                                                             #
// To find out the correct port, ddr and pin name when you just know the       #
// Arduino's digital pin number just google for "Arduino pin mapping".         #
// In the present example digital Pin 2 is used which corresponds to "PORTD",  #
// "DDRD" and "2", respectively.                                               #
//                                                                             #
//##############################################################################

// tested successfully using FTDI and Windows host PC at 1000000 baud
//#define BAUD_RATE 1000000 //256000//1000000 // 115200
#define BAUD_RATE 1000000
#define DATA_PORT  PORTD
#define DATA_DDR   DDRD						
#define DATA_PIN   2							
#define PIXEL_CNT  16

// LED for showing activity, NOT THE LED STRAND
// when a packet is received, blink this LED
// digital pin 13 - onboard LED = PB5
// comment out LED_PORT to disable
#define LED_PORT PORTB
#define LED_DDR  DDRB
#define LED_PIN  5


#define BYTE_CNT (PIXEL_CNT*3)
#define BUF_LEN BYTE_CNT

#define START_BYTE_GLEDIATOR 1
#define START_BYTE_PKT       2

// fill with RGB data
#define OPC_FILL       1
#define OPC_SHOW_FRAME 2
#define OPC_BLANK      3
#define OPC_SET_MODE   4

#define MODE_GLEDIATOR 1
#define MODE_PKT       2
#define MODE_BOTH      (MODE_GLEDIATOR|MODE_PKT)

#ifdef _MSVC_VER
#pragma pack(1)
#endif
typedef struct pixel_grb {
  uint8_t g;
  uint8_t r;
  uint8_t b;
} PIXEL_GRB;
