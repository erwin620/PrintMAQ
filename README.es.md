# PrintMAQ Auto-Kit

**Español** · [English](README.md)

Kit electrónico de *retrofit* que convierte una máquina de serigrafía manual en una máquina semi-automática, sin reemplazar la máquina existente. Este repositorio contiene el firmware, el proyecto de la pantalla y la documentación del kit.

> **Estado:** en desarrollo. El firmware compila y el cableado del eje de stroke está validado, pero la HMI, el home y el jog **todavía no se han probado en banco** con la versión actual (FreeRTOS).

## Qué hace hoy

- **Home del eje stroke** en dos fases (acercamiento rápido y lento) con sensor inductivo, con velocidades ajustables.
- **Jog manual** del stroke con hombre muerto: si la pantalla deja de enviar la orden durante 300 ms, el motor se detiene.
- **HMI Nextion** por USART1: botón HOME, estado de la máquina con colores, ajuste de velocidades y jog.
- **FreeRTOS**: tareas separadas para la lógica de la máquina, la HMI y el LED de estado. Los pulsos del motor se generan en una interrupción de TIM7 a 40 kHz para evitar el jitter del planificador.

Pendiente: eje del squeegee, rampas de aceleración, persistencia de parámetros en flash y paro de emergencia por hardware.

## Hardware

| Componente | Detalle |
|---|---|
| Microcontrolador | STM32F407VETx en la tarjeta DevEBox "STM32F4VE" (sin ST-Link integrado) |
| Drivers de motor | 2× DM556 (STEP/DIR), cableado de cátodo común |
| Motores | 2× paso a paso LDO-86STH98-554A (stroke y squeegee) |
| Pantalla | Nextion NX4827T043_011, 480×272 |
| Sensores de home | Inductivos NPN normalmente abiertos |

### Pines

| Función | Pin |
|---|---|
| Stroke: PUL / DIR | PC0 / PC1 |
| Stroke: sensor de home | PC6 |
| Squeegee (reservado): PUL / DIR / sensor | PC3 / PC4 / PC7 |
| HMI Nextion: TX / RX de la tarjeta | PA9 / PA10 |
| LEDs de la tarjeta (activos en bajo) | PA6 (D2), PA7 (D3) |

Pines que conviene evitar en esta tarjeta: PA11/PA12 (USB), PA13/PA14 (SWD), PB3–PB5 (probable flash SPI), PC8–PC12 y PD2 (probable microSD) y los puertos D/E (probable bus FSMC).

> **Seguridad:** el paro de emergencia debe cortar físicamente la alimentación de los drivers, independiente del firmware. No está implementado todavía.

## Estructura del repositorio

```
STM32/                    Proyecto de STM32CubeIDE (firmware)
├── Core/Src, Core/Inc    Código de la aplicación
│   ├── axis.c/h          Un eje paso a paso: pulsos y secuencia de home
│   ├── machine.c/h       Estado de la máquina e interfaz para la HMI
│   ├── hmi.c/h           Protocolo con la pantalla Nextion
│   ├── freertos.c        Hooks del RTOS
│   └── main.c/h          Inicio, tareas y definición de pines
├── Drivers, Middlewares  HAL de ST, CMSIS y FreeRTOS
└── PrintMAQ.ioc          Configuración de STM32CubeMX
HMI/                      Proyecto de la pantalla (Nextion Editor)
```

## Cómo funciona

```
Nextion ──UART──> interrupción USART1 ──> buffer ──> HmiTask ──> Machine_*()
                                                                     │
                                  MachineTask (cada 1 ms) <──────────┘

TIM7 (cada 25 µs) ──> Axis_Update ──> pulsos STEP/DIR al DM556
```

| Tarea / interrupción | Prioridad | Función |
|---|---|---|
| Interrupción de TIM7 | 2 (no usa la API del RTOS) | Pulsos, filtro del sensor y secuencia de home |
| `MachineTask` | Above Normal | Estado, hombre muerto |
| `HmiTask` | Normal | Comunicación con la pantalla |
| `LedTask` | Low | LED de estado en D3 |

El indicador D3 muestra el estado: apagado = sin home, parpadeo lento = haciendo home, fijo = en home, parpadeo muy lento = jog, parpadeo rápido = error. Si D2 y D3 quedan **fijos y encendidos**, el RTOS detectó un desbordamiento de pila o falta de memoria.

## Compilar

Requiere **STM32CubeIDE 2.2.0** (toolchain GNU Tools for STM32 14.3.1). No usar la 1.3.0: no reconoce la configuración del proyecto.

Desde la IDE, importa la carpeta `STM32/` y compila. También se puede compilar sin abrirla:

```powershell
& "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe" --launcher.suppressErrors -nosplash `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data "<carpeta de workspace temporal>" -import "<ruta>\STM32" -cleanBuild "PrintMAQ"
```

El resultado es `STM32/Debug/PrintMAQ.elf`.

> Si regeneras el código desde CubeMX, vuelve a agregar `#include <sys/types.h>` en `Core/Src/sysmem.c`; sin él no compila con el toolchain 14.3.1.

## Flashear (USB DFU, sin ST-Link)

1. Pon la tarjeta en modo programación: **BOOT0 → 3.3V**, **BOOT1 → GND** y pulsa reset. Windows debe listar `STM32 BOOTLOADER`.
2. La primera vez, asigna el driver **WinUSB** a ese dispositivo con [Zadig](https://zadig.akeo.ie) (*Options → List All Devices*).
3. Flashea con STM32CubeProgrammer:
   ```powershell
   STM32_Programmer_CLI.exe -c port=usb1 -d "<ruta>\STM32\Debug\PrintMAQ.elf" -v
   ```
4. Para ejecutar el programa, **BOOT0 → GND** y reset. Con BOOT0 en 3.3V la tarjeta vuelve siempre al bootloader.

## Pantalla

Abre `HMI/PrintMaq.HMI` con Nextion Editor y compílalo; el `.tft` exportado no se versiona.

La pantalla solo envía comandos y la lógica y la seguridad viven en el firmware. Cada comando es de texto: `#` seguido de una letra (`H` home, `J` jog, `V` velocidades de home, `P` petición de datos), su carga de longitud fija y tres bytes `0xFF` de cierre. El firmware responde con instrucciones de la Nextion (por ejemplo, `main.t_estado.txt="En home"`), también cerradas con `0xFF 0xFF 0xFF`. El parseo está en `hmi.c`.
