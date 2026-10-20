/* Estado general de la maquina e interfaz con la que la HMI la comanda. */
#include "machine.h"
#include "main.h"
#include "axis.h"
#include "FreeRTOS.h"
#include "task.h"

/* Frecuencia de la interrupcion que mueve los ejes; fija la resolucion de los flancos (1/PULSE_TICK_HZ).
   Su prioridad (2) es mas urgente que el limite de FreeRTOS (5), por lo que NO puede llamar a la API del RTOS. */
#define PULSE_TICK_HZ             40000U
#define PULSE_TICK_PRIORITY       2U

/* Valores por defecto del home del stroke.
   TODO: validar en banco segun la mecanica. */
#define STROKE_HOME_FAST_SPS      800U
#define STROKE_HOME_SLOW_SPS      100U
#define STROKE_HOME_BACKOFF_STEPS 400U
#define STROKE_HOME_MAX_STEPS     20000U

/* Limites de velocidad aceptados desde la HMI, pasos/s */
#define HOME_SPEED_MIN_SPS        10U
#define HOME_SPEED_MAX_SPS        5000U

/* Jog a velocidad fija con limite de recorrido.
   TODO: valores provisionales; confirmar con la mecanica antes de acoplar el motor. */
#define STROKE_JOG_SPS            400U
#define STROKE_JOG_MAX_TRAVEL     10000
/* Hombre muerto: si no llega una orden de jog en este tiempo, el motor se detiene. */
#define JOG_DEADMAN_MS            300U

static Axis_t stroke_axis;
static volatile Machine_State_t machine_state = MAQ_SIN_HOME;
static int8_t jog_dir = 0;
static uint32_t jog_last_request_ms = 0U;

static void PulseTimer_Start(void);

static uint32_t Clamp_Speed(uint32_t sps)
{
  if (sps < HOME_SPEED_MIN_SPS)
  {
    return HOME_SPEED_MIN_SPS;
  }
  if (sps > HOME_SPEED_MAX_SPS)
  {
    return HOME_SPEED_MAX_SPS;
  }
  return sps;
}

void Machine_Init(void)
{
  stroke_axis.pul_port          = STROKE_PUL_PORT;
  stroke_axis.pul_pin           = STROKE_PUL_PIN;
  stroke_axis.dir_port          = STROKE_DIR_PORT;
  stroke_axis.dir_pin           = STROKE_DIR_PIN;
  stroke_axis.home_port         = STROKE_HOME_PORT;
  stroke_axis.home_pin          = STROKE_HOME_PIN;
  stroke_axis.home_active_level = STROKE_HOME_ACTIVE;
  stroke_axis.dir_to_home       = STROKE_HOME_DIR;
  stroke_axis.activity_led_port = LED_D2_PORT;
  stroke_axis.activity_led_pin  = LED_D2_PIN;

  stroke_axis.home_fast_sps      = STROKE_HOME_FAST_SPS;
  stroke_axis.home_slow_sps      = STROKE_HOME_SLOW_SPS;
  stroke_axis.home_backoff_steps = STROKE_HOME_BACKOFF_STEPS;
  stroke_axis.home_max_steps     = STROKE_HOME_MAX_STEPS;
  stroke_axis.max_travel_steps   = STROKE_JOG_MAX_TRAVEL;

  Axis_Init(&stroke_axis);
  machine_state = MAQ_SIN_HOME;
  PulseTimer_Start();
}

/* TIM7 cuenta a PCLK1 porque APB1 no tiene divisor; si cambia el reloj, el calculo de ARR debe revisarse. */
static void PulseTimer_Start(void)
{
  __HAL_RCC_TIM7_CLK_ENABLE();
  TIM7->PSC = 0U;
  TIM7->ARR = (SystemCoreClock / PULSE_TICK_HZ) - 1U;
  TIM7->EGR = TIM_EGR_UG;
  TIM7->SR = 0U;
  TIM7->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(AXIS_TICK_IRQn, PULSE_TICK_PRIORITY, 0U);
  HAL_NVIC_EnableIRQ(AXIS_TICK_IRQn);
  TIM7->CR1 = TIM_CR1_CEN;
}

void TIM7_IRQHandler(void)
{
  TIM7->SR = ~TIM_SR_UIF;
  Axis_Update(&stroke_axis);
}

/* Machine_Update y los comandos de la HMI corren en tareas distintas y modifican machine_state;
   la seccion critica los serializa. Es corta y no enmascara la interrupcion de pulsos. */
void Machine_Update(void)
{
  taskENTER_CRITICAL();

  if (machine_state == MAQ_HACIENDO_HOME)
  {
    if (Axis_IsHomed(&stroke_axis))
    {
      machine_state = MAQ_EN_HOME;
    }
    else if (Axis_HasError(&stroke_axis))
    {
      machine_state = MAQ_ERROR;
    }
  }
  else if (machine_state == MAQ_JOG)
  {
    if (!Axis_IsJogging(&stroke_axis))
    {
      /* El eje se detuvo solo: sensor de home o limite de recorrido */
      machine_state = MAQ_EN_HOME;
    }
    else if ((HAL_GetTick() - jog_last_request_ms) >= JOG_DEADMAN_MS)
    {
      Machine_JogStop();
    }
  }

  taskEXIT_CRITICAL();
}

/* Se ignora si el home ya esta en curso o hay un jog activo. */
void Machine_RequestHome(void)
{
  taskENTER_CRITICAL();
  if (machine_state != MAQ_HACIENDO_HOME && machine_state != MAQ_JOG)
  {
    Axis_StartHome(&stroke_axis);
    machine_state = MAQ_HACIENDO_HOME;
  }
  taskEXIT_CRITICAL();
}

Machine_State_t Machine_GetState(void)
{
  return machine_state;
}

void Machine_SetHomeSpeed(uint32_t fast_sps, uint32_t slow_sps)
{
  Axis_SetHomeSpeed(&stroke_axis, Clamp_Speed(fast_sps), Clamp_Speed(slow_sps));
}

void Machine_GetHomeSpeed(uint32_t *fast_sps, uint32_t *slow_sps)
{
  *fast_sps = stroke_axis.home_fast_sps;
  *slow_sps = stroke_axis.home_slow_sps;
}

/* Solo se acepta con el home hecho. Cada llamada renueva el tiempo del hombre muerto. */
void Machine_JogRequest(int8_t dir)
{
  taskENTER_CRITICAL();

  if (machine_state == MAQ_EN_HOME || machine_state == MAQ_JOG)
  {
    jog_last_request_ms = HAL_GetTick();

    if (!(machine_state == MAQ_JOG && Axis_IsJogging(&stroke_axis) && dir == jog_dir))
    {
      Axis_JogStop(&stroke_axis);
      jog_dir = dir;
      if (Axis_JogStart(&stroke_axis, (dir > 0) ? 1U : 0U, STROKE_JOG_SPS))
      {
        machine_state = MAQ_JOG;
      }
      else
      {
        machine_state = MAQ_EN_HOME;
      }
    }
  }

  taskEXIT_CRITICAL();
}

void Machine_JogStop(void)
{
  taskENTER_CRITICAL();
  if (machine_state == MAQ_JOG)
  {
    Axis_JogStop(&stroke_axis);
    machine_state = MAQ_EN_HOME;
  }
  taskEXIT_CRITICAL();
}
