/* swuart.c - Software UART implementation for ATTiny(x)5, supporting split Tx/Rx baud rate
 *
 * (c) 2011 Russ Magee
 */

/* Note that the Tx and Rx baud rates do not have to match; indeed this code is
 * written to use both Timer0 and Timer1 for Tx and Rx, respectively, so that a
 * split-baud UART is implemented.
 * Assignments of port pins for Tx and Rx are easily changed below. The
 * code to handle Tx and Rx interrupts will need modification if the default
 * Timer0 [Tx] and Timer1 [Rx] are not appropriate for your design, or if you
 * want to use something other than the pin change IRQ to detect Rx start bits.
 *
 * Tx: PORTB[1] + TIMER0_COMPA_VECT
 * Rx: PORTB[0],PCINT[0] + TIMER1_COMPA_VECT
 *
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>

#include "swuart.h"

#define SWUART_TX_DDR       DDRB
#define SWUART_TX_PORT      PORTB
#define SWUART_TX_PIN       PB1  /* pin 6 */

#define SWUART_RX_DDR       DDRB
#define SWUART_RX_PIN       PB0  /* pin 5 */
#define SWUART_RX_PORT      PORTB
#define SWUART_RX_PORTIN    PINB

static volatile uint8_t txBusy;
static volatile uint8_t txData;
static volatile uint8_t txBitNum;

/* Value for TCCR0B[CS2:0] to set Tx rate */
static volatile uint8_t txBaudRateDiv; // = 2;   /* CLK/8 */
static volatile uint8_t txBaudRateVal; // = 32;  /* Given F_CPU of 8Mhz: 8Mhz/8/32 = 31250 */
/* Value for TCCR1[CS13:CS10] to set Rx rate */
static volatile uint8_t rxBaudRateDiv; // = 4;   /* CK/8 */
static volatile uint8_t rxBaudRateVal; // = 26;  /* Given F_CPU of 8MHz: 8MHz/8/26 = 38461.5 = 38400 +0.1% */

/* uart receive status and data variables */
static volatile uint8_t rxBusy;
static volatile uint8_t rxData;
static volatile uint8_t rxBitNum;

static volatile uint8_t inByte = 0;

/* receive buffer */
static uartQ rxBuffer;

/* enable and initialise the software uart */
void swuartInit(void)
{
  /* initialise the UART receive buffer */
  initQ(SWUART_RX_BUFFER_SIZE, (rbq *)(&rxBuffer));

  /* initialise the ports */
  /* Tx */
  SWUART_TX_DDR |= _BV(SWUART_TX_PIN);  /* Output */
  /* Set initial idle state for Tx pin */
#ifdef SWUART_INVERT
  SWUART_TX_PORT &= ~_BV(SWUART_TX_PIN);
#else
  SWUART_TX_PORT |= _BV(SWUART_TX_PIN);
#endif

  /* Rx - input, no pullup */
  SWUART_RX_DDR &= ~_BV(SWUART_RX_PIN);
  SWUART_RX_PORT &= ~_BV(SWUART_RX_PIN);

  /* initialise baud rate */
  swuartSetBaudRate(4800,9600);

  /* setup the transmitter */
  txBusy = FALSE;

  /* disable Tx interrupt */
  TIMSK &= ~ _BV(OCIE0A);

  /* setup the receiver */
  rxBusy = FALSE;
  /* enable Rx interrupt - pin change, enable for Rx pin */
  PCMSK |= _BV(PCINT0);
  GIMSK |= _BV(PCIE);   /* FIXME: probably should save GIMSK so swuartOff() can restore.. */
  PCMSK |= _BV(SWUART_RX_PIN);  /* Technically should be PCINTx, but meh. */

  /* turn on interrupts */
  sei();
}

/* turns off software UART */
void swuartOff(void)
{
  /* disable interrupts */
  /* Bit timers */
  TIMSK &= ~(_BV(OCIE0A) | _BV(OCIE1A));
  /* Pin IRQ source for Rx */
  PCMSK &= ~_BV(SWUART_RX_PIN); /* Again, should really be PCINTx but PBx corresponds */
  /* FIXME: restore GIMSK once swuartInit() saves it... */
}

void swuartSetBaudRate(uint32_t txBaudRate, uint32_t rxBaudRate)
{
  switch(txBaudRate) {
  case 4800:
    txBaudRateDiv = 2;  /* clk/64 */
    txBaudRateVal = 208;
    //txBaudRateVal = 202;
    //txBaudRateVal = 197;
    break;
   case 9600:
    txBaudRateDiv = 2;  /* clk/8 */
    txBaudRateVal = 104;
    break;
  case 31250:
    txBaudRateDiv = 2;  /* clk/8 */
    txBaudRateVal = 32;
    break;
  case 38400:
    txBaudRateDiv = 2;  /* clk/8 */
    txBaudRateVal = 26;
    break;
  }

  switch(rxBaudRate) {
  case 4800:
    rxBaudRateDiv = 4;  /* clk/8 */
    rxBaudRateVal = 208;
    break;
  case 9600:
    rxBaudRateDiv = 4;  /* clk/8 */
    rxBaudRateVal = 104;
    break;
  case 31250:
    rxBaudRateDiv = 4;  /* clk/8 */
    rxBaudRateVal = 32;
    break;
  case 38400:
    rxBaudRateDiv = 4;
    rxBaudRateVal = 26;
    break;
  }
}

void swuartTransmitByte(uint8_t data)
{
  /* wait until uart is ready */
  while(txBusy);

  /* set busy flag */
  txBusy = TRUE;
  /* save data */
  txData = data;
  /* set number of bits */
  txBitNum = 8;

  /* set the start bit */
#ifdef SWUART_INVERT
  SWUART_TX_PORT |= _BV(SWUART_TX_PIN);
#else
  SWUART_TX_PORT &= ~_BV(SWUART_TX_PIN);
#endif

  /* schedule the next bit */
  TCCR0B &= ~0x07; /* clear CS02:CS00 */
  TCNT0 = 0;
  OCR0A = txBaudRateVal;
//  TIFR |= _BV(OCF0A);  /* Clear any pending IRQ for OC0A */

  /* Start timer & enable Tx interrupt */
  TCCR0B |= txBaudRateDiv;
  TIMSK |= _BV(OCIE0A);
}

/** Tx interrupt routine
 */
ISR(TIMER0_COMPA_vect, ISR_NOBLOCK)
{
  uint8_t nextBitTime = txBaudRateVal;

  if(txBusy == TRUE) {
    /* there are bits still waiting to be transmitted */
    if(txBitNum > 0) {
      /* transmit data bits (LSB first) */
#ifdef SWUART_INVERT
      if( !(txData & 0x01) )
#else
        if( (txData & 0x01) )
#endif
          SWUART_TX_PORT |= _BV(SWUART_TX_PIN);
        else
          SWUART_TX_PORT &= ~_BV(SWUART_TX_PIN);
      /* shift bits down */
      txData = txData>>1;
      txBitNum--;
    }
    else {
      /* transmit stop bit */
#ifdef SWUART_INVERT
      SWUART_TX_PORT &= ~_BV(SWUART_TX_PIN);
#else
      SWUART_TX_PORT |= _BV(SWUART_TX_PIN);
#endif
//      nextBitTime += txBaudRateVal; /* make stop bit 2 bit lengths */
      /* transmission is done after stop bit */
      /* clear busy flag */
      txBusy = FALSE;
    }

//    TCNT0 = 0;
    OCR0A += nextBitTime;
  }
  else {
    /* disable Tx interrupt */
    TIMSK &= ~_BV(OCIE0A);
    TCCR0B &= ~0x07; /* clear CS02:CS00 */
  }
}

/** Get a byte (if available) from the receive buffer
 *
 * @param rxData - returned byte
 * @return TRUE if byte available, FALSE otherwise
 */
uint8_t swuartReceiveByte(uint8_t* rxData)
{
  /* make sure we have a receive buffer */
  if(rxBuffer.base.size) {
    /* make sure we have data */
    //if( inByte != 0 ) {
    if( !qEmpty((rbq *)(&rxBuffer)) ) {
      /* get byte from beginning of buffer */
      //*rxData = inByte; inByte = 0;
      *rxData = deQ((rbq *)(&rxBuffer));
      return TRUE;
    }
    else {
      /* no data */
      return FALSE;
    }
  }
  else {
    /* no buffer */
    return FALSE;
  }
}

ISR(SIG_PIN_CHANGE, ISR_NOBLOCK)
{
  /* Auto-ack [doc2586, 9.3.3] */
  /* Detection of start bit (if-stmt rejects wrong polarity start bits) */
  /* Disable pin change IRQ, enable TIMER1_COMPA_vect */
#ifdef SWUART_INVERT
  if( (SWUART_RX_PORTIN & _BV(SWUART_RX_PIN)) )
#else
  if( !(SWUART_RX_PORTIN & _BV(SWUART_RX_PIN)) )
#endif
  {
    PCMSK &= ~_BV(SWUART_RX_PIN);
    /* schedule data bit sampling 1.5 bit periods from now */
    TCCR1 &= ~0x0F; /* clear CS13:CS10 */

    TCNT1 = 0;
    OCR1A = rxBaudRateVal + rxBaudRateVal/2;

    rxBitNum = 0;
    rxData = 0;
    rxBusy = TRUE;

    /* Start timer & enable Rx Interrupt */
    TCCR1 |= rxBaudRateDiv;
    TIMSK |= _BV(OCIE1A);
  }
}

ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
  /* start bit has already been received
   * we're in the data bits */

  /* shift data byte to make room for new bit */
  rxData = rxData>>1;

  /* sample the data line */
#ifdef SWUART_INVERT
  if( !(SWUART_RX_PORTIN & _BV(SWUART_RX_PIN)) )
#else
  if( (SWUART_RX_PORTIN & _BV(SWUART_RX_PIN)) )
#endif
  {
    /* serial line is marking
     * record '1' bit */
    rxData |= 0x80u;
  }

  /* increment bit counter */
  rxBitNum++;
  /* check if we have a full byte */
  if(rxBitNum >= 8) {
    /* save data in receive buffer */
    //inByte = rxData;
    enQ((rbq *)(&rxBuffer), rxData);
    /* disable Rx interrupt */
    TIMSK &= ~_BV(OCIE1A);
    TCCR1 &= ~0x0F; /* clear CS13:CS10 to stop timer */
    //      /* re-enable pin-change interrupt for next start bit */
    //      GIMSK |= _BV(SWUART_RX_PIN);
    /* Re-enable pin change IRQ for next char's start bit */
    PCMSK |= _BV(SWUART_RX_PIN);
    /* clear busy flag */
    rxBusy = FALSE;
  }
  else {
    /* schedule next bit sample */
    OCR1A = OCR1A + rxBaudRateVal;
  }
}
