/***************************************************************************

    Simple Power Guard
    Copyright (C) 2014 Praneeth Kanishka

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses

    >> Email: scorpionzblog@gmail.com
    >> Web  : http://scopionz.blogspot.com

    Modified in 2026 by stark9000 (modifications Copyright (C) 2026
    stark9000, released under the same license): 16-bit start delay
    (default 5 minutes), automatic EEPROM migration, power-up counter that
    clears after a normal run, ADC clock set to Fosc/16, outputs written
    through a RAM shadow byte, a fault must be seen on several readings in a
    row before the relay trips, and the other fault LED is cleared when one
    is set.

***************************************************************************/

// Output pin masks (bit positions in GPIO)
#define M_H  0x02      // GP1  HIGH voltage LED
#define M_L  0x04      // GP2  LOW voltage LED
#define M_K  0x10      // GP4  status / blink LED
#define M_R  0x20      // GP5  relay driver

// ---------------------------------------------------------------
// EEPROM map
//   0,1 = v_h  upper ADC limit (high byte, low byte)
//   2,3 = v_l  lower ADC limit (high byte, low byte)
//   4,5 = v_d  start delay     (high byte, low byte), units of 500 ms
//   6   = power-up counter
//   7   = layout marker (0xA5 = new layout already written)
// ---------------------------------------------------------------

#define DEFAULT_DELAY  600     // 600 x 500 ms = 5 minutes
#define TRIP_COUNT     3       // bad readings in a row (x 500 ms) before the
                               // relay trips; 1 = original behaviour

unsigned char error, del = 1;   // error must be unsigned (compared with 99/101)
unsigned char gp, hi, lo;      // gp = RAM shadow of the GPIO outputs
unsigned int idx, v_d;
int sense, v_h, v_l;

// Change output pins through the shadow byte and write the whole port at
// once, so GPIO is never read back (avoids read-modify-write problems).
// "on" = bits to set, "off" = bits to clear.
void setout(unsigned char on, unsigned char off)
{
  gp = (gp | on) & ~off;
  GPIO = gp;
}

void main()
{
OSCCON = 0x70; // 8Mhz
ADCON0 = 0x00;
CMCON0 = 0x07; // Disable Comparators
CMCON1 = 0x00;
ANSEL  = 0x01;
TRISIO = 0b00000001;
OPTION_REG = 0;
WPU=0;
ADCON1 = 0x50; // ADC clock Fosc/16 (datasheet recommendation at 8 MHz)

Delay_ms(10);
gp   = 0;
GPIO = 0;

// ---- one-time migration to the new EEPROM layout ----
if(Eeprom_Read(7) != 0xA5) {
  Eeprom_Write(4, DEFAULT_DELAY >> 8);     Delay_ms(10);   // 0x02
  Eeprom_Write(5, DEFAULT_DELAY & 0xFF);   Delay_ms(10);   // 0x58
  Eeprom_Write(6, 0);                      Delay_ms(10);   // counter = 0
  Eeprom_Write(7, 0xA5);                   Delay_ms(10);   // mark done
}

v_h = Eeprom_Read(1) + Eeprom_Read(0)*256;
v_l = Eeprom_Read(3) + Eeprom_Read(2)*256;
v_d = ((unsigned int)Eeprom_Read(4) << 8) | Eeprom_Read(5);
error = Eeprom_Read(6);
if(error>101) error=0;
error++;
Eeprom_Write(6, error);
Delay_ms(10);

while(1) {

if(error>99) {                 // too many boots without a normal run
  setout(M_K | M_H | M_L, M_R);
  while(1);
}

if(del) {
  for(idx=0; idx<v_d; idx++) { // 600 = 5 min
    gp ^= M_K;                 // blink status LED
    GPIO = gp;
    Delay_ms(500);
  }
  del = 0;
  hi = 0;                      // start counting fresh after the delay
  lo = 0;
}

sense = Adc_Read(0);

     if(sense > v_h) { lo = 0; if(hi < TRIP_COUNT) hi++; }
else if(sense < v_l) { hi = 0; if(lo < TRIP_COUNT) lo++; }
else                 { hi = 0; lo = 0; }

if(hi >= TRIP_COUNT) {         // HIGH voltage confirmed
  setout(M_H, M_R | M_K | M_L);
  del = 1;
}
else if(lo >= TRIP_COUNT) {    // LOW voltage confirmed
  setout(M_L, M_R | M_K | M_H);
  del = 1;
}
else if(hi == 0 && lo == 0) {  // voltage OK
  setout(M_R | M_K, M_H | M_L);
  if(error != 0) {             // ran normally, clear the counter
    error = 0;
    Eeprom_Write(6, 0);
    Delay_ms(10);
  }
}
// else: out of range but not yet confirmed -> keep the outputs as they are

Delay_ms(500);
}
}
