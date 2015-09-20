
#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <util/delay_basic.h>
#include <util/delay.h>
#include <string.h>
#include <ctype.h>

#include "stddefs.h"
#include "ltypes.h"
#include "swuart.h"

#define FUNC_MIDI_TX 0
#define FUNC_MIDI_RX 1

#define COMM_TIMEOUT 3000
#define SAT_NUM_FIELD 8

#define INPUT_BUFFER_SIZE 450

unsigned char nmea_str[INPUT_BUFFER_SIZE];

char hex_str[]= "0123456789ABCDEF";

int main(void) {
  uint8_t data = '5';
  uint8_t bShouldSend = 1;
  swuartInit();
  swuartSetBaudRate(4800,9600);
  DDRB |= _BV(PB2);
  PORTB &= ~_BV(PB2);
  _delay_ms(200);

  uint16_t nCnt = 0;
  typedef enum  {eState_None, eState_Read, eState_Write } eState_t;
  typedef enum  {eTransformState_StartLine, eTransformState_ReadAllLine, eTransformState_ParseGPGGA, eTransformState_End } eTransformState_t;
  eState_t eState = eState_None;
  eTransformState_t eTransformState = eTransformState_StartLine;
  nmea_str[0] = 0;
  uint16_t nNmeaIdx = 0;

#if 0
  while(1) {
		swuartTransmitByte('X');
		_delay_ms(3);
		swuartTransmitByte('U');
		_delay_ms(3);
  }
#endif


#if 0
  while(1) {
		while(!swuartReceiveByte(&data)) {} 
		swuartTransmitByte(data);
  }
#endif

#if 1
  while(1) {
  	switch(eState) {
		case eState_None: {
  			nNmeaIdx = 0;
			while(!swuartReceiveByte(&data)) {} 
			nCnt = 0;
			nmea_str[nNmeaIdx++] = data;
			nmea_str[nNmeaIdx] = 0;
			eState = eState_Read;
		}
		break;
		case eState_Read: {
			if(!swuartReceiveByte(&data)) { 
				nCnt++; 
			} else if(nNmeaIdx < INPUT_BUFFER_SIZE-1) {
				//append nmea_str
				nmea_str[nNmeaIdx++] = data;
				nmea_str[nNmeaIdx] = 0;
				nCnt = 0; 
			}
			if(nNmeaIdx >= INPUT_BUFFER_SIZE-1) { 
				//TODO signal error
				eState = eState_Write;
			}
			if(COMM_TIMEOUT<=nCnt) { 
				eState = eState_Write;
			}
		}
		break;
		case eState_Write: {
			uint16_t curr_idx=0;
			unsigned char nGpggaFieldCnt = 0;
			eTransformState = eTransformState_StartLine;

			while(eTransformState != eTransformState_End) {
				switch(eTransformState) {
					case eTransformState_StartLine: {
						if(nmea_str[curr_idx] != 0 && curr_idx < INPUT_BUFFER_SIZE-1) {
							//TODO check for "$GPGGA"
							bShouldSend = 1;
							if( &nmea_str[curr_idx] == strstr(&nmea_str[curr_idx], "$GPGSA") ||
							    &nmea_str[curr_idx] == strstr(&nmea_str[curr_idx], "$GPGSV")
							) {
							  bShouldSend = 0;
							}
							if(&nmea_str[curr_idx] == strstr(&nmea_str[curr_idx], "$GPGGA")) {
								eTransformState = eTransformState_ParseGPGGA;
								//eTransformState = eTransformState_ReadAllLine;
								nGpggaFieldCnt = 1;
							} else {
								eTransformState = eTransformState_ReadAllLine;
							}
						} else {
							eTransformState = eTransformState_End;
						}
					}
					break;
					case eTransformState_ReadAllLine: {
						do {
							if(nmea_str[curr_idx] == 0) {
								eTransformState = eTransformState_End;
								break;
							}
							if(bShouldSend) {
								swuartTransmitByte(nmea_str[curr_idx]);
								_delay_ms(3);
							}
						} while(nmea_str[curr_idx++] != '\n' && curr_idx < INPUT_BUFFER_SIZE-1); 
						eTransformState = eTransformState_StartLine;
					}
					break;
					case eTransformState_ParseGPGGA: {
						unsigned char checksum = 0;
						nGpggaFieldCnt = 1;
						char nSendChecksum = 1;
						do {
							if(nmea_str[curr_idx] != '*' && nmea_str[curr_idx] != '$') {
								checksum ^= nmea_str[curr_idx];
							}
							if(nGpggaFieldCnt==SAT_NUM_FIELD) {
								if(isdigit(nmea_str[curr_idx]) && nmea_str[curr_idx+1] == ',') { //field is only one digit
									checksum ^= '0';
									swuartTransmitByte('0');
									_delay_ms(3);

									swuartTransmitByte(nmea_str[curr_idx]);
									_delay_ms(3);
									++nGpggaFieldCnt;//!!!
								} 
							} else {
								swuartTransmitByte(nmea_str[curr_idx]);
								_delay_ms(3);
							}
							if(nmea_str[curr_idx] == ',') {
								++nGpggaFieldCnt;
							}
						} while(nmea_str[curr_idx++] != '*' && curr_idx < INPUT_BUFFER_SIZE-1);
						if(curr_idx >= INPUT_BUFFER_SIZE-1) {
							eTransformState = eTransformState_StartLine;
						} 
						if(nSendChecksum) {
							swuartTransmitByte(hex_str[(checksum>>4) & 0xF]);
							_delay_ms(3);
							swuartTransmitByte(hex_str[checksum & 0xF]);
							_delay_ms(3);
							swuartTransmitByte(0x0D);
							_delay_ms(3);
//							swuartTransmitByte(0x0A);
//							_delay_ms(3);
							eTransformState = eTransformState_StartLine;
							curr_idx += 3;//skip original checksum and newline (2+1 bytes)
						}
					}
					break;
					default: {
					}
				}	
			}
			eState = eState_None;
			//if(str == strstr(str, "$GPGGA")) {
			//}

			/*
			for(nCnt=0; nCnt<100; ++nCnt) {
              swuartTransmitByte('U');
			}
			*/
			/*
  			uint16_t ii = 0;
			while (ii<nNmeaIdx) {
              swuartTransmitByte(nmea_str[ii++]);
			}
			nNmeaIdx = 0;
			eState = eState_None;
			*/
		}
		break;
		default: {
		}
		break;
	};
//    while( !swuartReceiveByte(&data) ) { nCnt++; }
//    swuartTransmitByte(data);
/*
    if(nCnt>100) {
	  for(nCnt=0; nCnt<5; nCnt++) {
        swuartTransmitByte('U');
	  }
	}
  nCnt = 0;
*/
//    swuartTransmitByte('U');
//    _delay_ms(1);
//	nCnt = 0;
//    PORTB = PORTB ^ _BV(PB2);
//    _delay_ms(100);
//!    _delay_ms(500);
//    swuartTransmitByte(data);
//    PORTB = PORTB ^ _BV(PB2);
//    _delay_ms(100);
//!    _delay_ms(1);
//    _delay_ms(100);
  }
#endif
}
