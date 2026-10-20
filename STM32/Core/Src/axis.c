/* Axis_Update corre en la interrupcion AXIS_TICK_IRQn; el resto de la API corre en tareas.
   El tiempo se mide con el contador de ciclos DWT, que debe estar activo. */
#include "axis.h"

/* Mantiene fuera la interrupcion de pulsos mientras una tarea modifica el eje.
   Devuelve el estado previo para que las llamadas anidadas solo rehabiliten en el nivel externo. */
static uint32_t Axis_Lock(void)
{
  uint32_t was_enabled = NVIC_GetEnableIRQ(AXIS_TICK_IRQn);

  NVIC_DisableIRQ(AXIS_TICK_IRQn);
  __DSB();
  __ISB();
  return was_enabled;
}

static void Axis_Unlock(uint32_t was_enabled)
{
  if (was_enabled)
  {
    NVIC_EnableIRQ(AXIS_TICK_IRQn);
  }
}

/* Antirrebote: el sensor se muestrea cada SENSOR_SAMPLE_US y un cambio solo se acepta
   tras SENSOR_FILTER_COUNT lecturas iguales seguidas. */
#define SENSOR_SAMPLE_US      200U
#define SENSOR_FILTER_COUNT   3U

static uint32_t Cycles_Now(void)
{
  return DWT->CYCCNT;
}

static uint8_t Sensor_ReadRaw(const Axis_t *axis)
{
  return (HAL_GPIO_ReadPin(axis->home_port, axis->home_pin) == axis->home_active_level) ? 1U : 0U;
}

static void Sensor_Update(Axis_t *axis)
{
  uint32_t sample_cycles = (SystemCoreClock / 1000000U) * SENSOR_SAMPLE_US;
  uint32_t now = Cycles_Now();

  if ((now - axis->last_sample_cycles) < sample_cycles)
  {
    return;
  }
  axis->last_sample_cycles = now;

  if (Sensor_ReadRaw(axis) != axis->sensor_active)
  {
    axis->sensor_count++;
    if (axis->sensor_count >= SENSOR_FILTER_COUNT)
    {
      axis->sensor_active = !axis->sensor_active;
      axis->sensor_count = 0U;
    }
  }
  else
  {
    axis->sensor_count = 0U;
  }
}

static void Move_Stop(Axis_t *axis)
{
  HAL_GPIO_WritePin(axis->pul_port, axis->pul_pin, GPIO_PIN_RESET);
  axis->pul_high = 0U;
  axis->moving = 0U;
}

static void Move_Start(Axis_t *axis, uint8_t toward_home, uint32_t sps)
{
  GPIO_PinState away_level = (axis->dir_to_home == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;

  if (sps == 0U)
  {
    sps = 1U;
  }

  HAL_GPIO_WritePin(axis->pul_port, axis->pul_pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(axis->dir_port, axis->dir_pin, toward_home ? axis->dir_to_home : away_level);

  axis->dir_sign = toward_home ? -1 : 1;
  axis->half_period_cycles = SystemCoreClock / (2U * sps);
  /* El primer flanco se retrasa medio periodo para que DIR se establezca antes del primer paso */
  axis->last_edge_cycles = Cycles_Now();
  axis->phase_steps = 0U;
  axis->pul_high = 0U;
  axis->moving = 1U;
}

/* Genera los flancos de PUL sin bloquear; cada flanco de subida cuenta un paso. */
static void Move_Tick(Axis_t *axis)
{
  uint32_t now = Cycles_Now();

  if (!axis->moving || (now - axis->last_edge_cycles) < axis->half_period_cycles)
  {
    return;
  }
  /* Se avanza un semiperiodo exacto en vez de tomar "now": asi el retraso de la interrupcion
     no se acumula y la velocidad promedio es la pedida. Si el atraso supera un semiperiodo,
     se resincroniza. */
  axis->last_edge_cycles += axis->half_period_cycles;
  if ((now - axis->last_edge_cycles) >= axis->half_period_cycles)
  {
    axis->last_edge_cycles = now;
  }

  if (axis->pul_high)
  {
    HAL_GPIO_WritePin(axis->pul_port, axis->pul_pin, GPIO_PIN_RESET);
    axis->pul_high = 0U;
  }
  else
  {
    HAL_GPIO_WritePin(axis->pul_port, axis->pul_pin, GPIO_PIN_SET);
    axis->pul_high = 1U;
    axis->position += axis->dir_sign;
    axis->phase_steps++;
    if (axis->activity_led_port != NULL)
    {
      HAL_GPIO_TogglePin(axis->activity_led_port, axis->activity_led_pin);
    }
  }
}

static void Home_Fail(Axis_t *axis)
{
  Move_Stop(axis);
  axis->home_state = AXIS_HOME_ERROR;
}

/* El jog hacia el home se bloquea con el sensor activo; hacia afuera, en el limite de recorrido. */
static uint8_t Jog_Blocked(const Axis_t *axis, uint8_t away)
{
  return away ? (axis->position >= axis->max_travel_steps) : axis->sensor_active;
}

void Axis_Init(Axis_t *axis)
{
  Move_Stop(axis);
  axis->jog_active = 0U;
  axis->position = 0;
  axis->home_state = AXIS_HOME_IDLE;
  axis->sensor_active = Sensor_ReadRaw(axis);
  axis->sensor_count = 0U;
  axis->last_sample_cycles = Cycles_Now();
}

static void Jog_Stop(Axis_t *axis)
{
  if (axis->jog_active)
  {
    Move_Stop(axis);
    axis->jog_active = 0U;
  }
}

void Axis_StartHome(Axis_t *axis)
{
  uint32_t lock = Axis_Lock();

  if (axis->sensor_active)
  {
    axis->home_state = AXIS_HOME_SALIR_DEL_SENSOR;
    Move_Start(axis, 0U, axis->home_slow_sps);
  }
  else
  {
    axis->home_state = AXIS_HOME_APROX_RAPIDA;
    Move_Start(axis, 1U, axis->home_fast_sps);
  }
  Axis_Unlock(lock);
}

/* Devuelve 1 si el jog quedo en marcha, 0 si esta bloqueado (sensor o limite). */
uint8_t Axis_JogStart(Axis_t *axis, uint8_t away_from_home, uint32_t sps)
{
  uint32_t lock = Axis_Lock();
  uint8_t started = 0U;

  if (Jog_Blocked(axis, away_from_home))
  {
    Jog_Stop(axis);
  }
  else
  {
    Move_Start(axis, away_from_home ? 0U : 1U, sps);
    axis->jog_active = 1U;
    axis->jog_away = away_from_home;
    started = 1U;
  }
  Axis_Unlock(lock);
  return started;
}

void Axis_JogStop(Axis_t *axis)
{
  uint32_t lock = Axis_Lock();

  Jog_Stop(axis);
  Axis_Unlock(lock);
}

uint8_t Axis_IsJogging(const Axis_t *axis)
{
  return axis->jog_active;
}

/* Debe llamarse a frecuencia fija desde la interrupcion AXIS_TICK_IRQn: su resolucion
   temporal limita la precision de los flancos. */
void Axis_Update(Axis_t *axis)
{
  Sensor_Update(axis);

  if (axis->jog_active && Jog_Blocked(axis, axis->jog_away))
  {
    Jog_Stop(axis);
  }

  switch (axis->home_state)
  {
    case AXIS_HOME_SALIR_DEL_SENSOR:
      if (!axis->sensor_active)
      {
        axis->home_state = AXIS_HOME_APROX_RAPIDA;
        Move_Start(axis, 1U, axis->home_fast_sps);
      }
      else if (axis->phase_steps >= axis->home_max_steps)
      {
        Home_Fail(axis);
      }
      break;

    case AXIS_HOME_APROX_RAPIDA:
      if (axis->sensor_active)
      {
        axis->home_state = AXIS_HOME_RETROCESO;
        Move_Start(axis, 0U, axis->home_fast_sps);
      }
      else if (axis->phase_steps >= axis->home_max_steps)
      {
        Home_Fail(axis);
      }
      break;

    case AXIS_HOME_RETROCESO:
      /* Retrocede el minimo configurado y ademas hasta liberar el sensor */
      if (axis->phase_steps >= axis->home_backoff_steps && !axis->sensor_active)
      {
        axis->home_state = AXIS_HOME_APROX_LENTA;
        Move_Start(axis, 1U, axis->home_slow_sps);
      }
      else if (axis->phase_steps >= axis->home_max_steps)
      {
        Home_Fail(axis);
      }
      break;

    case AXIS_HOME_APROX_LENTA:
      if (axis->sensor_active)
      {
        Move_Stop(axis);
        axis->position = 0;
        axis->home_state = AXIS_HOME_HECHO;
      }
      else if (axis->phase_steps >= axis->home_max_steps)
      {
        Home_Fail(axis);
      }
      break;

    case AXIS_HOME_IDLE:
    case AXIS_HOME_HECHO:
    case AXIS_HOME_ERROR:
    default:
      break;
  }

  Move_Tick(axis);
}

/* Los cambios se aplican al iniciar la siguiente fase del home. */
void Axis_SetHomeSpeed(Axis_t *axis, uint32_t fast_sps, uint32_t slow_sps)
{
  if (fast_sps > 0U)
  {
    axis->home_fast_sps = fast_sps;
  }
  if (slow_sps > 0U)
  {
    axis->home_slow_sps = slow_sps;
  }
}

uint8_t Axis_IsHoming(const Axis_t *axis)
{
  return (axis->home_state != AXIS_HOME_IDLE &&
          axis->home_state != AXIS_HOME_HECHO &&
          axis->home_state != AXIS_HOME_ERROR) ? 1U : 0U;
}

uint8_t Axis_IsHomed(const Axis_t *axis)
{
  return (axis->home_state == AXIS_HOME_HECHO) ? 1U : 0U;
}

uint8_t Axis_HasError(const Axis_t *axis)
{
  return (axis->home_state == AXIS_HOME_ERROR) ? 1U : 0U;
}
