# PrintMAQ Auto-Kit

[Español](README.es.md) · **English**

A retrofit electronics kit that turns a manual screen-printing machine into a semi-automatic one, without replacing the existing machine. This repository holds the firmware, the display project and the documentation of the kit.

> **Status:** work in progress. The firmware builds and the stroke-axis wiring is validated, but the HMI, homing and jog have **not yet been bench-tested** with the current (FreeRTOS) version.

## What it does today

- **Stroke axis homing** in two phases (fast and slow approach) using an inductive sensor, with adjustable speeds.
- **Manual jog** of the stroke axis with a dead-man switch: if the display stops sending the command for 300 ms, the motor stops.
- **Nextion HMI** over USART1: HOME button, color-coded machine status, speed settings and jog.
- **FreeRTOS**: separate tasks for the machine logic, the HMI and the status LED. Motor pulses are generated in a 40 kHz TIM7 interrupt to avoid scheduler jitter.

Still to do: squeegee axis, acceleration ramps, parameter persistence in flash and a hardware emergency stop.

## Hardware

| Component | Detail |
|---|---|
| Microcontroller | STM32F407VETx on the DevEBox "STM32F4VE" board (no onboard ST-Link) |
| Motor drivers | 2× DM556 (STEP/DIR), common-cathode wiring |
| Motors | 2× LDO-86STH98-554A steppers (stroke and squeegee) |
| Display | Nextion NX4827T043_011, 480×272 |
| Home sensors | Inductive NPN, normally open |

### Pinout

| Function | Pin |
|---|---|
| Stroke: PUL / DIR | PC0 / PC1 |
| Stroke: home sensor | PC6 |
| Squeegee (reserved): PUL / DIR / sensor | PC3 / PC4 / PC7 |
| Nextion HMI: board TX / RX | PA9 / PA10 |
| Onboard LEDs (active low) | PA6 (D2), PA7 (D3) |

Pins worth avoiding on this board: PA11/PA12 (USB), PA13/PA14 (SWD), PB3–PB5 (probably the SPI flash), PC8–PC12 and PD2 (probably the microSD slot) and the D/E ports (probably the FSMC bus).

> **Safety:** the emergency stop must physically cut the power of the drivers, independently of the firmware. It is not implemented yet.

## Repository layout

```
STM32/                    STM32CubeIDE project (firmware)
├── Core/Src, Core/Inc    Application code
│   ├── axis.c/h          One stepper axis: pulses and homing sequence
│   ├── machine.c/h       Machine state and the interface used by the HMI
│   ├── hmi.c/h           Nextion display protocol
│   ├── freertos.c        RTOS hooks
│   └── main.c/h          Startup, tasks and pin definitions
├── Drivers, Middlewares  ST HAL, CMSIS and FreeRTOS
└── PrintMAQ.ioc          STM32CubeMX configuration
HMI/                      Display project (Nextion Editor)
```

## How it works

```
Nextion ──UART──> USART1 interrupt ──> buffer ──> HmiTask ──> Machine_*()
                                                                  │
                                 MachineTask (every 1 ms) <───────┘

TIM7 (every 25 µs) ──> Axis_Update ──> STEP/DIR pulses to the DM556
```

| Task / interrupt | Priority | Role |
|---|---|---|
| TIM7 interrupt | 2 (must not call the RTOS API) | Pulses, sensor filter and homing sequence |
| `MachineTask` | Above Normal | State handling, dead-man switch |
| `HmiTask` | Normal | Display communication |
| `LedTask` | Low | Status LED on D3 |

LED D3 shows the machine state: off = not homed, slow blink = homing, solid = at home, very slow blink = jogging, fast blink = error. If D2 and D3 are both **solid on**, the RTOS detected a stack overflow or an out-of-memory condition.

## Build

Requires **STM32CubeIDE 2.2.0** (GNU Tools for STM32 14.3.1). Do not use 1.3.0: it does not recognize the project configuration.

From the IDE, import the `STM32/` folder and build. It can also be built without opening it:

```powershell
& "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe" --launcher.suppressErrors -nosplash `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data "<temporary workspace folder>" -import "<path>\STM32" -cleanBuild "PrintMAQ"
```

The output is `STM32/Debug/PrintMAQ.elf`.

> If you regenerate the code from CubeMX, add `#include <sys/types.h>` back to `Core/Src/sysmem.c`; without it the code does not build with toolchain 14.3.1.

## Flashing (USB DFU, no ST-Link)

1. Put the board in programming mode: **BOOT0 → 3.3V**, **BOOT1 → GND**, then press reset. Windows should list `STM32 BOOTLOADER`.
2. The first time, assign the **WinUSB** driver to that device with [Zadig](https://zadig.akeo.ie) (*Options → List All Devices*).
3. Flash with STM32CubeProgrammer:
   ```powershell
   STM32_Programmer_CLI.exe -c port=usb1 -d "<path>\STM32\Debug\PrintMAQ.elf" -v
   ```
4. To run the program, set **BOOT0 → GND** and reset. With BOOT0 at 3.3V the board always goes back to the bootloader.

## Display

Open `HMI/PrintMaq.HMI` with Nextion Editor and compile it; the exported `.tft` file is not versioned.

The display only sends commands, while the logic and the safety checks live in the firmware. Each command is text: `#` followed by a letter (`H` home, `J` jog, `V` home speeds, `P` data request), its fixed-length payload and three closing `0xFF` bytes. The firmware answers with Nextion instructions (for example `main.t_estado.txt="En home"`), also terminated with `0xFF 0xFF 0xFF`. The parser is in `hmi.c`.
