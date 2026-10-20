/* Eje paso a paso con driver STEP/DIR: genera los pulsos sin bloquear y ejecuta el home. */
#ifndef AXIS_H
#define AXIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* Interrupcion que ejecuta Axis_Update. Las funciones publicas que modifican el eje
   la enmascaran brevemente para no competir con ella por el mismo estado. */
#define AXIS_TICK_IRQn   TIM7_IRQn

typedef enum
{
  AXIS_HOME_IDLE = 0,
  AXIS_HOME_SALIR_DEL_SENSOR,
  AXIS_HOME_APROX_RAPIDA,
  AXIS_HOME_RETROCESO,
  AXIS_HOME_APROX_LENTA,
  AXIS_HOME_HECHO,
  AXIS_HOME_ERROR
} Axis_HomeState_t;

typedef struct
{
  /* Configuracion de hardware (se llena antes de Axis_Init) */
  GPIO_TypeDef *pul_port;
  uint16_t      pul_pin;
  GPIO_TypeDef *dir_port;
  uint16_t      dir_pin;
  GPIO_TypeDef *home_port;
  uint16_t      home_pin;
  GPIO_PinState home_active_level;
  GPIO_PinState dir_to_home;
  GPIO_TypeDef *activity_led_port;  /* Opcional: LED que cambia en cada paso (NULL = sin LED) */
  uint16_t      activity_led_pin;

  /* Parametros del home (modificables en tiempo de ejecucion) */
  uint32_t home_fast_sps;           /* Velocidad de acercamiento rapido, pasos/s */
  uint32_t home_slow_sps;           /* Velocidad de acercamiento lento, pasos/s */
  uint32_t home_backoff_steps;      /* Pasos de retroceso entre las dos fases */
  uint32_t home_max_steps;          /* Maximo de pasos por fase antes de dar error */
  int32_t  max_travel_steps;        /* Limite de recorrido hacia afuera del home (jog), pasos */

  /* Estado (solo lectura desde fuera) */
  int32_t          position;        /* Pasos desde home; valido solo si el home esta hecho */
  Axis_HomeState_t home_state;

  /* Estado interno: lo modifica Axis_Update desde la interrupcion */
  uint32_t half_period_cycles;
  uint32_t last_edge_cycles;
  uint32_t phase_steps;
  int8_t   dir_sign;
  uint8_t  moving;
  uint8_t  jog_active;
  uint8_t  jog_away;
  uint8_t  move_active;
  int32_t  move_target;
  uint8_t  pul_high;
  uint8_t  sensor_active;
  uint8_t  sensor_count;
  uint32_t last_sample_cycles;
} Axis_t;

void Axis_Init(Axis_t *axis);
void Axis_Update(Axis_t *axis);
void Axis_StartHome(Axis_t *axis);
void Axis_SetHomeSpeed(Axis_t *axis, uint32_t fast_sps, uint32_t slow_sps);
uint8_t Axis_JogStart(Axis_t *axis, uint8_t away_from_home, uint32_t sps);
void Axis_JogStop(Axis_t *axis);
uint8_t Axis_IsJogging(const Axis_t *axis);
uint8_t Axis_MoveTo(Axis_t *axis, int32_t target, uint32_t sps);
uint8_t Axis_IsMoving(const Axis_t *axis);
uint8_t Axis_IsHoming(const Axis_t *axis);
uint8_t Axis_IsHomed(const Axis_t *axis);
uint8_t Axis_HasError(const Axis_t *axis);

#ifdef __cplusplus
}
#endif

#endif /* AXIS_H */
