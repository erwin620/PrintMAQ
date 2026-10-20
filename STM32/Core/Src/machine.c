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

/* Home del squeegee (sensor arriba).
   TODO: validar en banco segun la mecanica. */
#define SQUEEGEE_HOME_FAST_SPS      400U
#define SQUEEGEE_HOME_SLOW_SPS      100U
#define SQUEEGEE_HOME_BACKOFF_STEPS 200U
#define SQUEEGEE_HOME_MAX_STEPS     10000U
#define SQUEEGEE_MAX_TRAVEL         3000

/* Limites de velocidad aceptados desde la HMI, pasos/s */
#define HOME_SPEED_MIN_SPS        10U
#define HOME_SPEED_MAX_SPS        5000U

/* Jog a velocidad fija con limite de recorrido.
   TODO: valores provisionales; confirmar con la mecanica antes de acoplar el motor. */
#define STROKE_JOG_SPS            400U
#define STROKE_JOG_MAX_TRAVEL     10000
/* Hombre muerto: si no llega una orden de jog en este tiempo, el motor se detiene. */
#define JOG_DEADMAN_MS            300U

/* Resolucion de cada eje.
   TODO: valores provisionales; calibrar en banco con el microstepping del DM556 y el paso de la transmision. */
#define STROKE_STEPS_PER_INCH     1000
#define SQUEEGEE_STEPS_PER_INCH   1000

/* Ciclo de impresion: recorrido del stroke hacia afuera y bajada del squeegee, ambos desde home.
   Los destinos deben quedar dentro del limite de recorrido de cada eje o Axis_MoveTo los recorta. */
#define PRINT_STROKE_STEPS        (5 * STROKE_STEPS_PER_INCH)
#define PRINT_SQUEEGEE_STEPS      (2 * SQUEEGEE_STEPS_PER_INCH)
#define PRINT_STROKE_SPS          400U
#define PRINT_SQUEEGEE_SPS        400U

/* Machine_Update corre cada 1 ms: el sensor de prenda debe sostener su nivel este numero de lecturas. */
#define PRENDA_DEBOUNCE_MS        20U

/* El squeegee se referencia primero para que este arriba cuando el stroke se mueva. */
typedef enum
{
  HOME_FASE_SQUEEGEE = 0,
  HOME_FASE_STROKE
} Home_Fase_t;

typedef enum
{
  PRINT_SALIDA_STROKE = 0,
  PRINT_BAJA_SQUEEGEE,
  PRINT_REGRESO_STROKE,
  PRINT_SUBE_SQUEEGEE
} Print_Step_t;

static Axis_t stroke_axis;
static Axis_t squeegee_axis;
static volatile Machine_State_t machine_state = MAQ_SIN_HOME;
static Home_Fase_t home_fase = HOME_FASE_SQUEEGEE;
static Print_Step_t print_step = PRINT_SALIDA_STROKE;
static int8_t jog_dir = 0;
static uint32_t jog_last_request_ms = 0U;

static uint8_t prenda_activa = 0U;
static uint8_t prenda_count = 0U;
/* Garantiza un solo ciclo por prenda: se arma al ver el sensor libre y se consume al iniciar el ciclo. */
static uint8_t prenda_armada = 0U;

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

  squeegee_axis.pul_port          = SQUEEGEE_PUL_PORT;
  squeegee_axis.pul_pin           = SQUEEGEE_PUL_PIN;
  squeegee_axis.dir_port          = SQUEEGEE_DIR_PORT;
  squeegee_axis.dir_pin           = SQUEEGEE_DIR_PIN;
  squeegee_axis.home_port         = SQUEEGEE_HOME_PORT;
  squeegee_axis.home_pin          = SQUEEGEE_HOME_PIN;
  squeegee_axis.home_active_level = SQUEEGEE_HOME_ACTIVE;
  squeegee_axis.dir_to_home       = SQUEEGEE_HOME_DIR;
  squeegee_axis.activity_led_port = NULL;

  squeegee_axis.home_fast_sps      = SQUEEGEE_HOME_FAST_SPS;
  squeegee_axis.home_slow_sps      = SQUEEGEE_HOME_SLOW_SPS;
  squeegee_axis.home_backoff_steps = SQUEEGEE_HOME_BACKOFF_STEPS;
  squeegee_axis.home_max_steps     = SQUEEGEE_HOME_MAX_STEPS;
  squeegee_axis.max_travel_steps   = SQUEEGEE_MAX_TRAVEL;

  Axis_Init(&stroke_axis);
  Axis_Init(&squeegee_axis);
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
  Axis_Update(&squeegee_axis);
}

/* Antirrebote por contador: un cambio de nivel solo se acepta tras PRENDA_DEBOUNCE_MS lecturas iguales. */
static void Prenda_Update(void)
{
  uint8_t raw = (HAL_GPIO_ReadPin(PRENDA_PORT, PRENDA_PIN) == PRENDA_ACTIVE) ? 1U : 0U;

  if (raw != prenda_activa)
  {
    prenda_count++;
    if (prenda_count >= PRENDA_DEBOUNCE_MS)
    {
      prenda_activa = raw;
      prenda_count = 0U;
    }
  }
  else
  {
    prenda_count = 0U;
  }

  if (!prenda_activa)
  {
    prenda_armada = 1U;
  }
}

/* Solo se evalua el eje de la fase actual: el otro puede conservar el resultado de un home anterior. */
static void Home_Update(void)
{
  Axis_t *axis = (home_fase == HOME_FASE_SQUEEGEE) ? &squeegee_axis : &stroke_axis;

  if (Axis_HasError(axis))
  {
    machine_state = MAQ_ERROR;
  }
  else if (Axis_IsHomed(axis))
  {
    if (home_fase == HOME_FASE_SQUEEGEE)
    {
      home_fase = HOME_FASE_STROKE;
      Axis_StartHome(&stroke_axis);
    }
    else
    {
      machine_state = MAQ_EN_HOME;
    }
  }
}

static void Print_Start(void)
{
  if (Axis_MoveTo(&stroke_axis, PRINT_STROKE_STEPS, PRINT_STROKE_SPS))
  {
    print_step = PRINT_SALIDA_STROKE;
    machine_state = MAQ_IMPRIMIENDO;
  }
  else
  {
    machine_state = MAQ_ERROR;
  }
}

/* Cada paso espera a que el eje anterior llegue a su destino antes de mover el siguiente,
   para que el squeegee nunca baje ni suba con el stroke en movimiento. */
static void Print_Update(void)
{
  uint8_t ok = 1U;

  if (Axis_IsMoving(&stroke_axis) || Axis_IsMoving(&squeegee_axis))
  {
    return;
  }

  switch (print_step)
  {
    case PRINT_SALIDA_STROKE:
      ok = Axis_MoveTo(&squeegee_axis, PRINT_SQUEEGEE_STEPS, PRINT_SQUEEGEE_SPS);
      print_step = PRINT_BAJA_SQUEEGEE;
      break;

    case PRINT_BAJA_SQUEEGEE:
      ok = Axis_MoveTo(&stroke_axis, 0, PRINT_STROKE_SPS);
      print_step = PRINT_REGRESO_STROKE;
      break;

    case PRINT_REGRESO_STROKE:
      ok = Axis_MoveTo(&squeegee_axis, 0, PRINT_SQUEEGEE_SPS);
      print_step = PRINT_SUBE_SQUEEGEE;
      break;

    case PRINT_SUBE_SQUEEGEE:
    default:
      machine_state = MAQ_EN_HOME;
      break;
  }

  if (!ok)
  {
    machine_state = MAQ_ERROR;
  }
}

/* Machine_Update y los comandos de la HMI corren en tareas distintas y modifican machine_state;
   la seccion critica los serializa. Es corta y no enmascara la interrupcion de pulsos. */
void Machine_Update(void)
{
  taskENTER_CRITICAL();

  Prenda_Update();

  if (machine_state == MAQ_HACIENDO_HOME)
  {
    Home_Update();
  }
  else if (machine_state == MAQ_EN_HOME)
  {
    if (prenda_activa && prenda_armada)
    {
      prenda_armada = 0U;
      Print_Start();
    }
  }
  else if (machine_state == MAQ_IMPRIMIENDO)
  {
    Print_Update();
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

/* Se ignora si el home ya esta en curso, hay un jog activo o se esta imprimiendo.
   TODO: agregar un paro desde la HMI que cancele el ciclo de impresion. */
void Machine_RequestHome(void)
{
  taskENTER_CRITICAL();
  if (machine_state != MAQ_HACIENDO_HOME && machine_state != MAQ_JOG &&
      machine_state != MAQ_IMPRIMIENDO)
  {
    home_fase = HOME_FASE_SQUEEGEE;
    Axis_StartHome(&squeegee_axis);
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
