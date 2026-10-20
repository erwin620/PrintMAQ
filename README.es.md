# PrintMAQ

**Español** · [English](README.md)

Firmware de un kit de *retrofit* que agrega control de movimiento a una máquina de serigrafía manual. Un taller que ya tiene una máquina manual puede producir más rápido y con más consistencia sin comprar una automática completa, y puede volver al modo manual cuando quiera.

Corre en un STM32F407 (tarjeta DevEBox "STM32F4VE"), maneja dos motores paso a paso con drivers DM556 y se opera desde una pantalla táctil Nextion. Un motor mueve el eje de stroke (el barrido a lo largo de la mesa) y el otro el squeegee (subida, bajada y presión).

## En qué punto está

Lo que funciona y compila hoy: el home del eje stroke con un sensor inductivo, el jog manual y las pantallas de la Nextion (botón de home, estado de la máquina, ajuste de velocidades, jog). El cableado del driver del stroke está validado en banco.

Lo que falta: el eje del squeegee, las rampas de aceleración, guardar parámetros en flash y el paro de emergencia por hardware. Además migré hace poco el firmware a FreeRTOS; esa versión compila, pero todavía no la he corrido con el motor y la pantalla conectados.

## Algunas decisiones de diseño

**Los pulsos del motor salen de una interrupción de timer, no de una tarea.** Una tarea despertada por el tick del RTOS no puede pasar de un paso por milisegundo y tiene jitter, que un motor paso a paso nota como ruido y pasos perdidos. TIM7 se dispara cada 25 µs y genera los flancos de STEP, el filtro del sensor y la secuencia de home. Como esa interrupción es más urgente que el límite de FreeRTOS, nunca llama a su API; las tareas hablan con ella mediante secciones críticas cortas.

**La pantalla pide y el firmware decide.** Al mantener el jog, la pantalla envía una orden cada cierto tiempo. Si las órdenes se detienen 300 ms (cable desconectado, pantalla congelada), el motor se detiene. El jog además se bloquea en el sensor de home y en un límite de recorrido, diga lo que diga la pantalla.

**El paro de emergencia tiene que ser de hardware.** Debe cortar la alimentación de los drivers sin importar lo que haga el firmware. Por eso no considero un paro por software como sustituto, y sigue en la lista de pendientes.

El firmware se divide en módulos pequeños: `axis` maneja un eje paso a paso (pulsos y home), `machine` guarda el estado de la máquina y es la única interfaz que usa la pantalla, y `hmi` implementa el protocolo de la Nextion. Las tres tareas de FreeRTOS (lógica de la máquina, pantalla y LED de estado) están en `main.c`.

## Hardware

- STM32F407VETx en una tarjeta DevEBox "STM32F4VE". No trae ST-Link; su único USB va directo al chip.
- 2× drivers DM556 con cableado de cátodo común: el "+" de cada señal va al GPIO y el "−" a GND. Los 3.3 V del GPIO bastan, sin transistor.
- 2× motores paso a paso LDO-86STH98-554A.
- Pantalla Nextion NX4827T043_011, 480×272.
- Sensores inductivos NPN (normalmente abiertos) para el home.

| Función | Pin |
|---|---|
| Stroke STEP / DIR | PC0 / PC1 |
| Sensor de home del stroke | PC6 |
| Squeegee STEP / DIR / sensor (reservados) | PC3 / PC4 / PC7 |
| TX / RX de la tarjeta hacia la Nextion | PA9 / PA10 |
| LEDs de la tarjeta, activos en bajo | PA6 (D2), PA7 (D3) |

Pines que conviene evitar en esta tarjeta: PA11/PA12 (USB), PA13/PA14 (SWD), PB3–PB5 (probable flash SPI), PC8–PC12 y PD2 (probable microSD) y los puertos D/E (probable bus FSMC).

El LED D3 muestra el estado de la máquina: apagado sin home, parpadeo lento haciendo home, fijo en home, parpadeo muy lento en jog y parpadeo rápido en error. Si D2 y D3 quedan fijos y encendidos, el RTOS detectó un desbordamiento de pila o se quedó sin memoria.

## Compilar

Hace falta STM32CubeIDE 2.2.0 (GNU Tools for STM32 14.3.1). La 1.3.0 no reconoce la configuración del proyecto. Importa la carpeta `STM32/` y compila, o desde la línea de comandos:

```powershell
& "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\stm32cubeidec.exe" --launcher.suppressErrors -nosplash `
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data "<workspace temporal>" -import "<ruta>\STM32" -cleanBuild "PrintMAQ"
```

El resultado es `STM32/Debug/PrintMAQ.elf`. Si regeneras el código desde CubeMX, hay que volver a agregar `#include <sys/types.h>` en `Core/Src/sysmem.c`; sin eso no compila con el toolchain 14.3.1.

## Flashear por USB DFU

No hay ST-Link, así que flasheo con el bootloader USB que trae el chip:

1. Conecta BOOT0 a 3.3 V y BOOT1 a GND, y haz reset. Windows debe listar `STM32 BOOTLOADER`. La primera vez hay que darle a ese dispositivo el driver WinUSB con [Zadig](https://zadig.akeo.ie).
2. Flashea con STM32CubeProgrammer:
   ```powershell
   STM32_Programmer_CLI.exe -c port=usb1 -d "<ruta>\STM32\Debug\PrintMAQ.elf" -v
   ```
3. Regresa BOOT0 a GND y haz reset para ejecutar el programa. Es el error más fácil de cometer: con BOOT0 en 3.3 V la tarjeta siempre vuelve al bootloader y nunca ejecuta la aplicación.

## La pantalla

Abre `HMI/PrintMaq.HMI` con Nextion Editor y compílalo (el `.tft` exportado no se versiona). La pantalla envía comandos de texto: `#`, una letra (`H` home, `J` jog, `V` velocidades de home, `P` petición de datos), una carga de tamaño fijo y tres bytes `0xFF`. El firmware responde con instrucciones normales de la Nextion, como `main.t_estado.txt="En home"`. El parseo está en `hmi.c`.

## Estructura del repositorio

```
STM32/    Proyecto de STM32CubeIDE (código de la aplicación en Core/, HAL de ST y FreeRTOS en Drivers/ y Middlewares/)
HMI/      Proyecto de Nextion Editor
```

## Licencia

Mi código se publica bajo la [licencia MIT](LICENSE). El HAL de ST y CMSIS en `STM32/Drivers/` y FreeRTOS en `STM32/Middlewares/` conservan sus propias licencias.
