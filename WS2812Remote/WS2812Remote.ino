// 20170103 SCL creation

#include "WS2812Remote.h"

PIXEL_GRB g_pixelBuffer[PIXEL_CNT];
uint8_t *display_buffer = (uint8_t *)g_pixelBuffer;
PIXEL_GRB g_BlkPixel = {0,0,0};
uint8_t g_mode = MODE_BOTH;


inline uint8_t readByte()
{
  while(Serial.peek() == -1);
  return Serial.read();
}

void pixelFill(PIXEL_GRB &pixel)
{
  for (uint8_t i=0;i < PIXEL_CNT;i++) {
    g_pixelBuffer[i] = pixel;
  }
}

//##############################################################################
//                                                                             #
// WS2812 output routine                                                       #
// Extracted from a ligh weight WS2812 lib by Tim (cpldcpu@gmail.com)          #
// Found on wwww.microcontroller.net                                           #
// Requires F_CPU = 16MHz                                                      #
//                                                                             #
//##############################################################################

void ShowBuffer()
{
  uint8_t *data = display_buffer;
  uint16_t datlen = PIXEL_CNT * sizeof(PIXEL_GRB);
  uint8_t curbyte,ctr,masklo;
  uint8_t maskhi = _BV(DATA_PIN);

  cli();
  masklo =~ maskhi & DATA_PORT;
  maskhi |= DATA_PORT;

  while (datlen--) 
  {
    curbyte = *data++;

    asm volatile
    (
      "		ldi %0,8	\n\t"		// 0
      "loop%=:out %2, %3	\n\t"		// 1
      "lsl	%1		\n\t"		// 2
      "dec	%0		\n\t"		// 3
      "		rjmp .+0	\n\t"		// 5
      "		brcs .+2	\n\t"		// 6l / 7h
      "		out %2,%4	\n\t"		// 7l / -
      "		rjmp .+0	\n\t"		// 9
      "		nop		\n\t"		// 10
      "		out %2,%4	\n\t"		// 11
      "		breq end%=	\n\t"		// 12      nt. 13 taken
      "		rjmp .+0	\n\t"		// 14
      "		rjmp .+0	\n\t"		// 16
      "		rjmp .+0	\n\t"		// 18
      "		rjmp loop%=	\n\t"		// 20
      "end%=:			\n\t" 
      :	"=&d" (ctr)
      :	"r" (curbyte), "I" (_SFR_IO_ADDR(DATA_PORT)), "r" (maskhi), "r" (masklo)
    );
  }

  sei();
}

void doGlediator()
{
  uint8_t *ptr = display_buffer;

  for (uint16_t i=0;i < BYTE_CNT;i++) {
    *(ptr++)=readByte();
  }

  ShowBuffer(); 
}

void doPkt()
{
  uint8_t chk = START_BYTE_PKT;
  uint8_t chkByte = readByte();
  uint8_t opCode = readByte();
  chk ^= opCode;
  switch(opCode) {
  case OPC_FILL:
    {
      PIXEL_GRB p;
      p.r = readByte();
      chk ^= p.r;
      p.g = readByte();
      chk ^= p.g;
      p.b = readByte();
      chk ^= p.b;
      if (chk == chkByte) {
	pixelFill(p);
	ShowBuffer();
      }
    }
    break;
  case OPC_SHOW_FRAME:
    {
      uint8_t *ptr = display_buffer;

      for (uint16_t i=0;i < BYTE_CNT;i++) {
	chk ^= *(ptr++)=readByte();
      }
      if (chk == chkByte) {
	ShowBuffer();
      }
    }
    break;
  case OPC_BLANK:
    if (chk == chkByte) {
      pixelFill(g_BlkPixel);
      ShowBuffer();
    }
    break;
  case OPC_SET_MODE:
    {
      uint8_t mode = readByte();
      chk ^= mode;
      if (chk == chkByte) {
	g_mode = mode;
      }
    }
    break;
  default:
    ;
  }
}

//##############################################################################
//                                                                             #
// Setup                                                                       #
//                                                                             #
//##############################################################################

void setup()
{
  // Set data pin as output
  DATA_DDR |= (1 << DATA_PIN);    //  pinMode(DATA_PIN,OUTPUT);

  Serial.begin(BAUD_RATE);

  // blank out the screen on startup
  pixelFill(g_BlkPixel);
  ShowBuffer();
}

//##############################################################################
//                                                                             #
// Main loop                                                                   #
//                                                                             #
//##############################################################################


void loop()
{  
  /*debug stuff
  uint8_t b = readByte();
  Serial.write(b);
  switch(b) {
  */
  switch(readByte()) {
  case START_BYTE_GLEDIATOR:
    if (g_mode & MODE_GLEDIATOR) {
      doGlediator();
    }
    break;
  case START_BYTE_PKT:
    if (g_mode & MODE_PKT) {
      doPkt();
    }
    break;
  default:
    ;
  }    
}


