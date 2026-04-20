# PCB Design Notes

Captures intentional design choices and debugging history that aren't
obvious from the schematic alone, plus a TODO list for the next revision.

## Intentional design choices (not bugs)

### J1 wired to UART0 (GPIO1 / GPIO3)
J1 routes the dev kit's `RX0` and `TX0` pins to a 2-pin external header.
This is the **same** UART that the dev kit's onboard CP2102 is permanently
soldered to. Two USB-UARTs on those lines fight each other.

This is **intentional**, not a bug — the convention is: only one of
{onboard CP2102 USB, J1 external UART} should be plugged into a host
at a time. Use one or the other for serial / flashing.

If that convention ever needs to be relaxed (e.g. flash via J1 while
also keeping the onboard USB attached for power), move J1 to
`GPIO16/GPIO17` (RX2/TX2 — currently unconnected per the netlist) and
switch firmware to `Serial2.begin(...)`.

### Two USB-C breakouts (horizontal + vertical)
Both `adafruit_type_c_breakout_rev_b1` (horizontal) and
`adafruit_type_c_vertical_breakout1` (vertical) footprints are placed
intentionally as **mounting alternates**:

- **Horizontal**: desk use; the unit leans on the USB plug.
- **Vertical**: hanging the unit on a wall.

Pick one at assembly time and only solder that one. Both have D+/D-
deliberately unconnected — the breakouts are power-only, since serial /
programming goes through the dev kit's own micro-USB (or J1).

## Power architecture history

The redundant-looking power components (U1 = Pololu S13V30F5,
U3 = Pololu U3V70F5, plus Q1 IRLML6402 P-MOSFET, D1 1N5819 Schottky,
`sj_mini_3v3` jumper) are the result of several revs of brownout / reset
debugging:

- **Brownouts traced partly to mechanical interference**: the TFT's
  on-display SD-card shield was physically touching one of the
  decoupling capacitors on the carrier PCB, causing intermittent
  shorts / brownouts. **Mechanical fix on next rev**: shim or relocate
  the offending cap so it sits well clear of the SD shield's metal
  frame.
- **Multiple regulator footprints** were placed because the Pololu
  modules are expensive and sometimes go out of stock. Having an
  alternate populatable regulator means the board can be assembled
  with whichever is available without a spin.
- **Open solder jumpers** (`sj_Open_onboard_sdcard_*`,
  `sj_onboard_sdcard_clk`) were attempts to wire up the TFT module's
  *own* on-display SD card slot. **Never got it working.** Leave them
  open or remove from next rev to reduce confusion — the project uses
  a separate SD card on VSPI (GPIO5/18/19/23) instead. Forensic
  analysis of why it never mounted is in the section below.

## Why the TFT's onboard SD never mounted

How it was wired (closing the four solder jumpers JP6-JP9 routes the
TFT module's onboard SD card pins to the same ESP32 GPIOs as the
standalone SD on VSPI):

| TFT pad      | Jumper | ESP32 GPIO |
|--------------|--------|------------|
| `SD_CS`   15 | JP8    | GPIO5      |
| `SD_MOSI` 16 | JP6    | GPIO19     |
| `SD_MISO` 17 | JP7    | GPIO23     |
| `SD_SCK`  18 | JP9    | GPIO18     |

That maps to `User_Setup.h` (`SD_PIN=5, SD_MOSI=23, SD_MISO=19,
SD_SCLK=18`). Firmware-side it should "just work" — TFT itself is on
HSPI (`USE_HSPI_PORT`), SD on VSPI, no bus conflict. So the failure
is hardware-side. Most likely culprits, ranked:

1. **No pull-ups on the SD lines.** SD-mode requires 10-50 kΩ pull-ups
   to 3.3V on `CS`, `MOSI`, `MISO`, `SCK`. None present on the JP6-JP9
   nets in this design, and cheap MSP4021 modules typically don't add
   them on the SD socket either. Without pulls, the card may never
   respond to init — exact symptom: `SD.begin()` returns false.
2. **MISO not tri-stated by the TFT module's onboard SD socket.**
   Notorious gotcha on this class of TFT — the SD's `DO` (MISO) line
   is wired straight to the connector with no tri-state buffer, so the
   card holds the line even when CS is high. Standard fix is a 74HC125
   buffer on MISO.
3. **Brownouts during SD inrush** (~100-300 mA). Same family of issue
   as the documented brownout history. Lacks bulk decoupling near the
   TFT VCC pin.
4. **Solder-jumper reliability** — four hand-bridged SJ pads in series
   on the SD signals; one cold joint kills the whole path. Hard to
   diagnose without per-jumper LEDs/test points.
5. **VSPI default-instance edge cases** — usually fine but worth
   passing an explicit `SPIClass` to `SD.begin()` if revisiting.

### If the TFT-SD path is ever revisited (next rev)
- Add **10 kΩ pull-ups to 3.3V on CS, MOSI, MISO, SCK** between the
  jumpers and the ESP. Single biggest payoff.
- Add a **74HC125 tri-state buffer on MISO** between the TFT module and
  the ESP. Decouples the SD's bad MISO behavior from the bus.
- **Generous decoupling on TFT VCC**: 10 µF tantalum + 100 nF ceramic
  right at TFT pin 1, plus 22 µF on the 3.3V rail near the SD jumpers.
- **Replace solder jumpers with 0Ω resistors or a DIP switch** so
  contact is reliable.

(Practical note: the standalone SD module already works fine for this
project; the TFT-SD path is forensic curiosity, not a real blocker.)

## TODO for next PCB rev

### Strapping-pin pull resistors
- [ ] **10kΩ pull-up on GPIO15 (TFT_CS) to 3.3V.** Without it, GPIO15
      can be low at power-on, which suppresses the ROM bootloader's
      diagnostic log on UART0.
- [ ] **10kΩ pull-down on GPIO12 (TFT_MISO) to GND.** Belt-and-suspenders
      for the flash-voltage strapping. Fine on WROOM-32 modules thanks
      to eFuses, but cheap insurance against bare-chip variants.
- [ ] **10kΩ pull-up on GPIO5 (SD_CS) to 3.3V.** Strapping pin must be
      HIGH at power-on. SD card modules usually pull it up but don't
      rely on it.

### Mechanical / cleanup
- [ ] **TFT SD shield clearance**: ensure no decoupling cap sits under
      the metal frame of the TFT's SD slot — historical brownout source.
- [ ] **External reset button**: wire ESP32 `EN` (Pad 16) to a tactile
      button on the carrier. The dev kit's on-board EN is hard to reach
      once the unit is in an enclosure.
- [ ] **Drop unused TFT on-display SD jumpers** (`sj_Open_onboard_sdcard_*`,
      `sj_onboard_sdcard_clk`) — that path was abandoned.

### Optional improvements
- [ ] **Reduce strapping-pin usage**: TFT and SD currently use three
      strapping pins (GPIO5, GPIO12, GPIO15). Consider re-routing TFT
      pins to GPIO25/26/27/32/33 etc. so boot is less sensitive to what
      external peripherals are doing.
- [ ] **Verify I2C pull-ups** on GPIO21 (SDA) / GPIO22 (SCL): typically
      provided by BME280 / BH1750 breakouts, but worth confirming if
      any breakout is swapped for a bare-chip footprint.
- [ ] **Consider collapsing power architecture** if regulator
      availability has stabilized — fewer parts, fewer failure modes.
