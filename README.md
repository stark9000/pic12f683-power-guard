# Simple Power Guard (PIC12F683) - 5 Minute Start Delay Edition

A mains over/under-voltage cutoff with a start delay, built on a PIC12F683.
It switches a relay on only when the mains voltage is inside a safe window,
and waits before reconnecting the load after power returns or after a fault.
This protects fridges, ACs, TVs and other equipment from sustained over-voltage
and under-voltage and from rapid on/off cycling. It is not a surge protector
(see Limitations).

This is a modified version of the **Simple Power Guard** by Praneeth Kanishka
(original design and schematic:
<https://scopionz.blogspot.com/2015/09/simple-power-guard-using-12f683.html>).

## What is different from the original

| Original | This version |
|---|---|
| Start delay stored in 1 byte, max about 127 s (`char`) | Start delay stored in 2 bytes, up to about 9 hours; **default 5 minutes** |
| Counter at EEPROM address 5 counted every power-up and could lock the unit after about 100 power cycles | Counter is **cleared after the first normal run**, so only repeated boots that never reach a normal state add up |
| EEPROM had to be programmed by hand | Delay, counter and marker are **written automatically on first boot** |
| ADC clock left at the default (Fosc/2) | ADC clock set to Fosc/16 (`ADCON1 = 0x50`) |
| Outputs changed with bit operations on `GPIO` (read-modify-write) | Outputs kept in a RAM shadow byte and written to `GPIO` as a whole byte |
| One ADC reading outside the window trips the relay | A fault must be seen on `TRIP_COUNT` readings in a row (default 3, at least about 1 s) |
| After a high-to-low (or low-to-high) swing both fault LEDs could stay lit | Setting one fault LED clears the other |

---

## WARNING: mains voltage is dangerous

This circuit is connected to mains electricity, which can kill you and start
fires.

- Build and test only if you are experienced with mains wiring.
- While developing and testing, power the board from a low-voltage source or
  an isolated supply, and do not connect a real mains load.
- Use a relay and wiring rated for your load current and voltage.
- Enclose the finished unit and fuse the mains input.
- You build and use this at your own risk.

---

## How it works

1. On power-up the relay is **off** and the unit waits for the start delay
   (default 5 minutes). The status LED blinks while waiting.
2. After the delay it measures the mains voltage through the ADC (pin GP0).
3. If the voltage is inside the window, the relay turns **on**.
4. If the voltage stays above the high limit or below the low limit for 3
   readings in a row (at least about 1 second), the relay turns **off**, the
   matching LED lights, and the start delay runs again before the relay can turn
   on.

### LED and relay states

| State | Relay | Status LED (GP4) | HIGH LED (GP1) | LOW LED (GP2) |
|---|---|---|---|---|
| Waiting (start delay) | OFF | blinking | unchanged | unchanged |
| Voltage OK | ON | ON | OFF | OFF |
| Voltage too high | OFF | OFF | ON | OFF |
| Voltage too low | OFF | OFF | OFF | ON |
| Locked out (see below) | OFF | ON | ON | ON |

During the start delay after a fault, the HIGH or LOW LED from that fault stays
lit while the status LED blinks.

---

## Limitations

- **Sustained faults only.** The voltage is sampled once every 500 ms with a
  single ADC reading and no averaging, and a fault must be seen on 3 readings
  in a row (at least about 1 s) before the relay trips. Over-voltage and
  under-voltage that last longer than that are caught. Fast spikes and transients pass
  straight through, so use a MOV or surge protector for those. The relay can
  stay on for up to about 1.5 s into a real fault.
- **Noisy sense signal.** Confirmation over several readings ignores isolated
  bad samples, but a badly filtered sense input can still cause nuisance trips.
  Make sure the sense circuit has a good smoothing (filter) capacitor, and keep
  a clear gap between `v_h` and `v_l`.
- **Lockout on an unstable supply.** See "Power-up counter and lockout" below.

---

## What you need

**Hardware**
- PIC12F683 and the circuit from the original blog post (see its schematic:
  voltage sensing into GP0, relay driver, 3 LEDs)
- A programmer for PIC (PICkit 2/3/4 or similar)
- A multimeter

**Software**
- mikroC PRO for PIC (the code uses `Eeprom_Read`, `Eeprom_Write`, `Adc_Read`
  and `Delay_ms` from the mikroC libraries)
- Your programmer's software (for example MPLAB IPE or PICkit 2 software)

---

## Pin map (PIC12F683, 8-pin DIP)

| Pin | Name | Function |
|---|---|---|
| 1 | VDD | +5 V |
| 2 | GP5 | Relay driver (RLY) |
| 3 | GP4 | Status / blink LED (L_K) |
| 4 | GP3 | Not used by this code |
| 5 | GP2 | LOW voltage LED (L_L) |
| 6 | GP1 | HIGH voltage LED (L_H) |
| 7 | GP0 / AN0 | Voltage sense input (ADC) |
| 8 | VSS | GND |

---

## EEPROM map

The settings live in the PIC's internal data EEPROM.

| Address | Content | Written by |
|---|---|---|
| 0, 1 | `v_h` upper ADC limit (high byte, low byte) | **You** |
| 2, 3 | `v_l` lower ADC limit (high byte, low byte) | **You** |
| 4, 5 | Start delay (high byte, low byte), units of 500 ms | Automatic on first boot |
| 6 | Power-up counter | Automatic |
| 7 | Layout marker (`A5` = done) | Automatic |

**You only ever have to write addresses 0-3.** If they are blank (`FF FF`),
the voltage comparison will not work properly.

---

## Step 1: Choose your voltage limits (ADC values)

The ADC is 10-bit (0 to 1023) and uses VDD (5 V) as its reference, so:

```
ADC value = (voltage at GP0 / 5.0) x 1023
```

The easy and accurate way to find your numbers is to measure, because it does
not depend on knowing your divider resistor values:

1. Power the circuit from a variable source (for example a variac), or use a
   known supply voltage for testing.
2. Set the input to the **highest voltage you want to allow** (for example
   260 V AC) and measure the DC voltage at pin 7 (GP0) with the multimeter.
   Convert it with the formula above. This is `v_h`.
3. Do the same at the **lowest voltage you want to allow** (for example
   170 V AC). This is `v_l`.

**Worked example:** 3.25 V at GP0 gives 3.25 / 5.0 x 1023 = 664.95, which
rounds to 665. 2.50 V at GP0 gives 511.5, which rounds to 512. (The formula is
only good to about one ADC count, which does not matter here.)

### Convert to EEPROM bytes

Convert each value to 4-digit hex, then split into high byte and low byte.

| Decimal | Hex | High byte | Low byte |
|---|---|---|---|
| 665 | `0299` | `02` | `99` |
| 512 | `0200` | `02` | `00` |

So with `v_h = 665` and `v_l = 512`:

| Address | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| Value | `02` | `99` | `02` | `00` |

Any programmer calculator (Windows Calculator in Programmer mode) will do the
conversion.

> Keep `v_h` larger than `v_l`, with a gap between them so the relay does not
> chatter at the edge of the window.

---

## Step 2: Compile

1. Create a new mikroC PRO for PIC project.
2. Device: **PIC12F683**, clock: **8 MHz** (internal oscillator, the code sets
   `OSCCON = 0x70`).
3. Set the configuration bits to match the original project from the blog
   post. As a guide: internal oscillator with I/O on the clock pins, watchdog
   timer **off**, power-up timer **on**, and **brown-out reset enabled**. The
   code writes to EEPROM on every boot, and a sagging supply during a write can
   corrupt it; brown-out reset holds the chip in reset until the supply is
   healthy. The watchdog must stay off: the code never clears it, so with it on
   the chip would keep resetting during the start delay and in the lockout loop.
4. Add `power_guard.c` to the project and build it. You get a `.hex` file.

> **ADC setup note:** the original design never sets the ADC clock, so
> `ADCON1` stays at its reset value (Fosc/2), which is too fast at 8 MHz. This
> version adds `ADCON1 = 0x50;` in `main()` (Fosc/16, as the PIC12F683
> datasheet recommends at 8 MHz). It changes only the ADC clock and leaves the
> pin setup alone. Avoid replacing it with `ADC_Init()`: that can change the
> `ANSEL` pin setup in some mikroC versions. If readings still look noisy,
> check the filter capacitor on the sense input.

---

## Step 3: Program the chip

### Case A: existing, working unit (ADC limits already in EEPROM)

1. Program the new `.hex` **without erasing the EEPROM**. In your programmer
   software, untick "Erase EEPROM" or "Program EEPROM" (program memory only).
2. Power up. On first boot the code sees that address 7 is not `A5`, then
   writes the 5-minute delay, resets the counter and sets the marker. Your
   addresses 0-3 are untouched.

If your programmer wipes the EEPROM anyway, follow Case B.

### Case B: new chip, or EEPROM was erased

1. Program the `.hex`.
2. In the programmer's EEPROM tab, write your ADC limits to addresses 0-3
   (see Step 1). Example: `02 99 02 00`.
3. Leave addresses 4-7 alone, or fill them in by hand as `02 58 00 A5`. The
   code fills them in automatically on first boot.
4. Power up.

---

## Step 4: Test

1. Power up on a bench (low voltage, no mains load). The relay stays off and
   the status LED blinks. This is the start delay.
2. After 5 minutes the relay clicks on and the status LED stays lit.
3. Check fault confirmation: push the input out of range for **less than 1
   second** and put it back. Nothing should trip.
4. Raise the input above the high limit and hold it for several seconds: within
   about 1.5 s the relay goes off, the HIGH LED lights and the delay restarts.
5. Lower the input below the low limit and hold it for several seconds: within
   about 1.5 s the relay goes off, the LOW LED lights (the HIGH LED goes out)
   and the delay restarts.
6. Check the power-up counter: power the unit up and down a few times without
   waiting for the start delay to end, then read EEPROM address 6 with your
   programmer. It should have gone up by one for each power-up. Now power up and
   let it reach the normal state (relay on), power down and read address 6
   again: it should be `00`.
7. Check the lockout: with the power off, write `63` (99 in decimal) to EEPROM
   address 6, then power up. The relay should stay off and all three LEDs
   (status, HIGH and LOW) should light. Power-cycle it: the next two power-ups
   stay locked out, and the one after those starts normally (status LED
   blinking during the start delay). To clear a lockout at once, write `00` to
   address 6. If you changed the lockout number in the source, write that number
   instead of 99.

To test quickly, set a short delay first (see below), then set it back to
5 minutes for the final unit.

---

## Changing the delay

Delay = value x 0.5 seconds. Write the value to EEPROM as **high byte at
address 4, low byte at address 5**.

| Delay | Value | Address 4 | Address 5 |
|---|---|---|---|
| 10 s | 20 | `00` | `14` |
| 30 s | 60 | `00` | `3C` |
| 1 min | 120 | `00` | `78` |
| **5 min (default)** | 600 | `02` | `58` |
| 10 min | 1200 | `04` | `B0` |

The maximum is 65535 x 0.5 s, about 9 hours.

To make the chip rewrite the 5-minute default, set address 7 to anything other
than `A5` and power-cycle.

To change the default in the source, edit `DEFAULT_DELAY` (for example
`#define DEFAULT_DELAY 120` for 1 minute) and recompile.

---

## Fault confirmation (`TRIP_COUNT`)

The code acts on an over-voltage or under-voltage only after seeing it on
`TRIP_COUNT` readings in a row. Readings are 500 ms apart, so the default of 3
means a fault must be seen on 3 readings, which takes at least about 1 second.
Isolated noisy readings are ignored,
and one good reading resets the count. The trade-off is that the relay can stay
on for up to about 1.5 s into a real fault. Set `TRIP_COUNT` to 1 for the
original behaviour (trips on a single reading), or raise it if your sense
signal is noisy.

---

## Power-up counter and lockout

The counter at address 6 counts boots. It is cleared once the unit has reached
the normal state (voltage OK, relay on). If the unit reboots about 100 times
in a row without ever reaching the normal state, for example because of a
constant brown-out or crashing supply, it locks out with the relay off and all
three LEDs lit.

The lockout is not permanent. Once it has locked out, the next two power-ups
are also locked out, and the power-up after those starts normally (the
counter rolls back to 0 once the stored value passes 101). To clear it straight
away, set address 6 to `00` with your programmer.

**Unstable supply warning:** "normal state" needs the full start delay plus a
good voltage reading, so more than 5 minutes of uninterrupted good power. On a
very unreliable supply with frequent short outages, 100 short power-ups in a
row can lock the unit out, and brown-out resets count as power-ups too. One
good 5-minute run clears the counter. For a fridge or freezer this means the
load stays off until the lockout clears, so if that is a risk in your
installation, raise the threshold in the source: change `if(error>99)` (the
lockout) to a larger number such as `if(error>199)`, **and** change the
blank-EEPROM check `if(error>101)` to a number 2 higher, such as
`if(error>201)`. Keep the second number 2 above the first and below 255. If you
change only the first number, the second line resets the counter at 102 and the
lockout can never trigger. To remove the lockout completely, delete the
`if(error>99) { ... }` block.

---

## Troubleshooting

| Problem | Likely cause |
|---|---|
| Relay never turns on | ADC limits in addresses 0-3 are blank (`FF FF`) or wrong; check them. Blank limits show up as a permanent HIGH fault (HIGH LED lit) |
| Delay is huge (hours) | Addresses 4-5 hold old data; set address 7 to something other than `A5` and power-cycle, or write `02 58` to addresses 4-5 |
| Delay shorter or longer than expected | Check the units: value x 0.5 s |
| Relay chatters at the edge of the window | `v_h` and `v_l` are too close; widen the gap |
| Relay trips randomly, readings jumpy | Noisy sense signal; check the filter capacitor on the sense input, widen the window, raise `TRIP_COUNT`, see the ADC setup note |
| All LEDs on, relay off | Lockout; see the counter section |
| EEPROM got wiped when programming | Turn off "Erase EEPROM" in the programmer, or re-enter addresses 0-3 |

---

## Files

| File | Description |
|---|---|
| `power_guard.c` | Full mikroC source |
| `README.md` | This guide |
| `LICENSE` | GNU General Public License v3 (full text) |

## Credits

- **Original design and code:** Praneeth Kanishka (Scorpionz), Copyright (C)
  2014. Blog: <http://scopionz.blogspot.com>. Original project post (with the
  circuit schematic this firmware is built for):
  <https://scopionz.blogspot.com/2015/09/simple-power-guard-using-12f683.html>
- **Modifications (2026):** [stark9000](https://github.com/stark9000): 16-bit
  start delay, automatic EEPROM migration, power-up counter reset, ADC clock
  setting (Fosc/16), RAM shadow byte for the outputs, fault confirmation over
  several readings (`TRIP_COUNT`), clearing the other fault LED when one is
  set, and this documentation.

This is an independent modification and is not endorsed by the original
author. The circuit schematic belongs to the original project and is not
included here; see the original post.

## License

This program is free software: you can redistribute it and/or modify it under
the terms of the **GNU General Public License v3** (or, at your option, any
later version), the same license as the original. See the [LICENSE](LICENSE)
file for the full text. It is distributed in the hope that it will be useful,
but **without any warranty**, without even the implied warranty of
merchantability or fitness for a particular purpose.

If you redistribute this or your own changes, keep the license and copyright
notices in `power_guard.c`, add a line saying what you changed and when, and
release your version under the same license.
