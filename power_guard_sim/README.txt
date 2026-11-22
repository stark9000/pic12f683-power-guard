Power Guard host simulation
Build:  gcc -w -o sim harness.c
Run:    ./sim <scenario> <end_seconds> <ee0> <ee1> <ee2> <ee3> <ee4> <ee5> <ee6> <ee7>   (EEPROM bytes in hex)
Scenarios (ADC input over time): 0 good, 1 one bad sample, 2 two bad, 3 three bad (trips),
  4 HIGH then later LOW, 5 alternating hi/lo, 6 HIGH swings straight to LOW.
Example: ./sim 3 700 02 99 02 00 02 58 00 A5
power_guard.c is the unmodified source; harness.c fakes registers, EEPROM, Adc_Read and Delay_ms.
