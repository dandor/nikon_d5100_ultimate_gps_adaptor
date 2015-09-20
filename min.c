#include <avr/io.h>

int main(void)
{
 //Set Port B pins as all outputs
   DDRB = 0xff;

     // Set all Port B pins as HIGH
       PORTB = 0xff;

         return 1;
         }
