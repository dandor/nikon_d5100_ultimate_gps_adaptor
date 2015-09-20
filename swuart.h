
#if !defined(__SWUART_H__)
#define __SWUART_H__

#include "fifo.h"

#define SWUART_RX_BUFFER_SIZE 2
#undef SWUART_INVERT

typedef struct _uartQ {
  rbq base;
  uint8_t data[SWUART_RX_BUFFER_SIZE+1];
} uartQ;

void swuartInit(void);
void swuartOff(void);
void swuartSetBaudRate(uint32_t txBaudRate, uint32_t rxBaudRate);
void swuartTransmitByte(uint8_t data);
uint8_t swuartReceiveByte(uint8_t* rxData);
uint8_t swuartReceiveByte(uint8_t* rxData);

#endif
