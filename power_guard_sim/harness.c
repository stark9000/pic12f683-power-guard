/* Host harness: fakes PIC12F683 registers + mikroC library, runs the UNMODIFIED power_guard.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

volatile unsigned char OSCCON, ADCON0, ADCON1, CMCON0, CMCON1, ANSEL, TRISIO, OPTION_REG, WPU, GPIO;
static unsigned char ee[256];
static long now_ms = 0, end_ms = 0;
static int scen = 0;
static jmp_buf done;
static int last_gpio = -1;
static int adc_calls = 0;

/* ADC value (10-bit) as a function of time, per scenario */
static int adc_at(long t) {
  double s = t / 1000.0;
  switch (scen) {
  case 0: return 600;                                   /* always good */
  case 1: return (s>=310.0 && s<310.5) ? 700 : 600;     /* 1 bad sample */
  case 2: return (s>=310.0 && s<311.0) ? 700 : 600;     /* 2 bad samples */
  case 3: return (s>=310.0 && s<311.5) ? 700 : 600;     /* 3 bad samples -> trips */
  case 4: if (s>=310 && s<330) return 700;              /* HIGH then LOW swing */
          if (s>=900 && s<930) return 400; return 600;
  case 6: return (s>=310 && s<400) ? 700 : (s>=400 && s<1000) ? 400 : 600;  /* HIGH straight to LOW */
  case 5: return (s>=310.0 && s<310.5) ? 700 : (s>=310.5 && s<311.0) ? 400 : 600; /* alternating hi/lo, never 3 in a row */
  }
  return 600;
}
int Adc_Read(int ch){ adc_calls++; return adc_at(now_ms); }
unsigned char Eeprom_Read(unsigned a){ return ee[a&0xFF]; }
void Eeprom_Write(unsigned a, unsigned char v){
  ee[a&0xFF]=v; printf("[%8.1fs] EEPROM[%u] <- %02X\n", now_ms/1000.0, a, v);
}
static void report(void){
  if (((int)GPIO & ~0x10) != last_gpio) {
    last_gpio = GPIO & ~0x10;
    printf("[%8.1fs] GPIO=%02X  RLY=%d K=%d HIGH=%d LOW=%d\n", now_ms/1000.0, GPIO,
      !!(GPIO&0x20), !!(GPIO&0x10), !!(GPIO&0x02), !!(GPIO&0x04));
  }
}
void Delay_ms(unsigned ms){
  report();
  now_ms += ms;
  if (now_ms > end_ms) { report(); longjmp(done,1); }
}
static void on_alarm(int s){ report(); printf("[stuck in while(1) lockout loop at t=%.1fs]\n", now_ms/1000.0); fflush(stdout); _exit(0); }

#define main pg_main
#define int short
#include "power_guard.c"
#undef int
#undef main

int main(int argc, char**argv){
  /* args: scenario endsec  ee0 ee1 ee2 ee3 ee4 ee5 ee6 ee7 (hex) */
  scen = atoi(argv[1]); end_ms = atol(argv[2])*1000L;
  for (int i=0;i<8;i++) ee[i] = (unsigned char)strtol(argv[3+i],0,16);
  signal(SIGALRM,on_alarm); alarm(2);
  printf("sizeof(int) in sim = %d (PIC12 mikroC int = 2)\n", (int)sizeof(short));
  if (!setjmp(done)) pg_main();
  printf("--- end t=%.1fs  EEPROM: %02X %02X %02X %02X %02X %02X %02X %02X\n", now_ms/1000.0,
    ee[0],ee[1],ee[2],ee[3],ee[4],ee[5],ee[6],ee[7]);
  return 0;
}
