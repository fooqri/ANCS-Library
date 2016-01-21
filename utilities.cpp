/** (c)2013, Bernard Pratz, bernard at pratz dot net
 *  under the WTFPL License
 */
//#define STATUS1
#include "utilities.h"

void free_ram (void) {    
#ifdef STATUS1	
  extern int __heap_start, *__brkval; 
  int v; 
  Serial.print(F("[free SRAM] "));
  Serial.print((int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
  Serial.println(F(" bytes"));   
#endif
}

void serial_print_char(char c) { 
#ifdef STATUS1	
    if (c >= 32 && c < 128) {
        Serial.print((char)c);
    } else {
        Serial.print(F("0x"));
        Serial.print(c, HEX);
    }
    Serial.print(' ');     
#endif
}
