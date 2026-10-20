# PrintMAQ

[Español](README.es.md) · English

Firmware for a retrofit kit that adds motion control to a manual screen-printing press. A shop that already owns a manual machine can speed up and standardize its production without buying a fully automatic one, and can go back to manual operation whenever it wants.

It runs on an STM32F407 (a DevEBox "STM32F4VE" board), drives two stepper motors through DM556 drivers and is operated from a Nextion touch display. One motor moves the stroke axis (the sweep along the table) and the other moves the squeegee (up/down and pressure).

## Where it stands

What works and builds today: homing of the stroke axis with an inductive sensor, manual jog, and the Nextion screens (home button, machine state, speed settings, jog). The wiring of the stroke driver is validated on the bench.

What is missing: the squeegee axis, acceleration ramps, saving parameters to flash and the hardware emergency stop. I also moved the firmware to FreeRTOS recently and that version compiles, but I haven't run it yet with the motor and the display connected.

## A few design decisions

**Motor pulses come from a timer interrupt, not from a task.** A task woken by the RTOS tick can't go faster than one step per millisecond and it jitters, which a stepper motor notices as noise and lost steps. TIM7 fires every 25 µs and generates the STEP edges, the sensor filtering and the homing sequence. Because that interrupt is more urgent than FreeRTOS's own limit, it never calls the RTOS API; the tasks talk to it through short critical sections.

**The display only asks, the firmware decides.** Pressing jog on the screen sends a command every so often. If the commands stop for 300 ms (cable out, screen frozen), the motor stops. The jog is also blocked at the home sensor and at a travel limit, whatever the screen says.

**The emergency stop has to be hardware.** It must cut the power of the drivers regardless of what the firmware is doing. That's why I'm not treating a software stop as a substitute, and why it's still listed as missing.

The firmware is split in a few small modules: `axis` handles one stepper axis (pulses and homing), `machine` holds the machine state and is the only interface the display uses, and `hmi` implements the Nextion protocol. The three FreeRTOS tasks (machine logic, display, status LED) are in `main.c`.

## Hardware

- STM32F407VETx on a DevEBox "STM32F4VE" board. It has no onboard ST-Link; the only USB port goes straight to the chip.
- 2× DM556 drivers, wired common-cathode: the "+" of each signal goes to the GPIO and the "−" to GND. The 3.3 V of the GPIO is enough, no transistor needed.
- 2× LDO-86STH98-554A stepper motors.
- Nextion NX4827T043_011 display, 480×272.
- Inductive NPN sensors (normally open) for home.

| Function | Pin |
|---|---|
| Stroke STEP / DIR | PC0 / PC1 |
| Stroke home sensor | PC6 |
| Squeegee STEP / DIR / sensor (reserved) | PC3 / PC4 / PC7 |
| Nextion TX / RX on the board | PA9 / PA10 |
| Onboard LEDs, active low | PA6 (D2), PA7 (D3) |

Pins worth avoiding on this board: PA11/PA12 (USB), PA13/PA14 (SWD), PB3–PB5 (probably the SPI flash), PC8–PC12 and PD2 (probably the microSD slot) and the D/E ports (probably the FSMC bus).

LED D3 shows the machine state: off when not homed, slow blink while homing, solid at home, very slow blink while jogging and fast blink on error. If D2 and D3 are both solid on, the RTOS caught a stack overflow or ran out of memory.

## Building

You need STM32CubeIDE 2.2.0 (GNU Tools for STM32 14.3.1). Version 1.3.0 doesn't recognize the project configuration. Import the `STM32/` folder and build, or from the command line:

```powershell
& "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe" --launcher.suppressErrors -nosplash `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data "<temporary workspace>" -import "<path>\STM32" -cleanBuild "PrintMAQ"
```

The output is `STM32/Debug/PrintMAQ.elf`. If you regenerate the code from CubeMX, add `#include <sys/types.h>` back to `Core/Src/sysmem.c`, otherwise it doesn't compile with toolchain 14.3.1.

## Flashing over USB DFU

There is no ST-Link, so I flash through the chip's built-in USB bootloader:

1. Tie BOOT0 to 3.3 V and BOOT1 to GND, then reset. Windows should list `STM32 BOOTLOADER`. The first time, give that device the WinUSB driver with [Zadig](https://zadig.akeo.ie).
2. Flash with STM32CubeProgrammer:
   ```powershell
   STM32_Programmer_CLI.exe -c port=usb1 -d "<path>\STM32\Debug\PrintMAQ.elf" -v
   ```
3. Move BOOT0 back to GND and reset to run the program. This is the easiest mistake to make: with BOOT0 at 3.3 V the board always returns to the bootloader and never runs the application.

## The display

Open `HMI/PrintMaq.HMI` with Nextion Editor and compile it (the exported `.tft` isn't versioned). The screen sends text commands: `#`, a letter (`H` home, `J` jog, `V` home speeds, `P` data request), a fixed-size payload and three `0xFF` bytes. The firmware answers with ordinary Nextion instructions such as `main.t_estado.txt="En home"`. The parser is in `hmi.c`.

## Repository layout

```
STM32/    STM32CubeIDE project (application code in Core/, ST HAL and FreeRTOS in Drivers/ and Middlewares/)
HMI/      Nextion Editor project
```
