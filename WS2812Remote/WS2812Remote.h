#pragma once

// tested successfully using FTDI and Windows host PC at 1000000 baud
//#define BAUD_RATE 1000000 //256000//1000000 // 115200
#define BAUD_RATE 115200
#define DATA_PORT          PORTD
#define DATA_DDR           DDRD						
#define DATA_PIN           2							
#define PIXEL_CNT   16
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

