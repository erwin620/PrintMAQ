/* Comunicacion con la pantalla Nextion. USART1 por registros, con buffers circulares e
   interrupciones para que la comunicacion nunca bloquee al resto del firmware.
   La pantalla solo envia comandos; la logica y la seguridad viven en machine.c. */
#include "hmi.h"
#include "main.h"
#include "machine.h"

#define RX_SIZE   128U   /* potencia de 2: los indices se envuelven con una mascara */
#define TX_SIZE   256U
#define MSG_MAX   160U

/* Colores RGB565 del texto de estado (sistema Nothing, modo oscuro): el color va en el valor,
   no en el fondo. Error usa el rojo de acento del sistema.
   Deben coincidir con pantallas.md de la feature 007. */
#define COLOR_NEGRO           0U
#define COLOR_TEXTO_SECUND    40147U
#define COLOR_TEXTO_PRIMARIO  61277U
#define COLOR_TEXTO_DISPLAY   65535U
#define COLOR_EXITO           19691U
#define COLOR_AVISO           54600U
#define COLOR_ACENTO          53444U

static volatile uint8_t  rx_buf[RX_SIZE];
static volatile uint16_t rx_head, rx_tail;
static volatile uint8_t  tx_buf[TX_SIZE];
static volatile uint16_t tx_head, tx_tail;

static uint8_t pend_state, pend_speeds, pend_print_speeds, pend_saved;
static int16_t last_sent_state = -1;

void USART1_IRQHandler(void)
{
  uint32_t sr = USART1->SR;

  if (sr & (USART_SR_RXNE | USART_SR_ORE))
  {
    uint8_t b = (uint8_t)USART1->DR;   /* leer DR tambien limpia ORE */
    uint16_t next = (uint16_t)((rx_head + 1U) & (RX_SIZE - 1U));
    if (next != rx_tail)
    {
      rx_buf[rx_head] = b;
      rx_head = next;
    }
  }

  if ((sr & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE))
  {
    if (tx_tail != tx_head)
    {
      USART1->DR = tx_buf[tx_tail];
      tx_tail = (uint16_t)((tx_tail + 1U) & (TX_SIZE - 1U));
    }
    else
    {
      USART1->CR1 &= ~USART_CR1_TXEIE;
    }
  }
}

static int Rx_Pop(void)
{
  int b;

  if (rx_tail == rx_head)
  {
    return -1;
  }
  b = rx_buf[rx_tail];
  rx_tail = (uint16_t)((rx_tail + 1U) & (RX_SIZE - 1U));
  return b;
}

/* Encola el mensaje completo o nada. Devuelve 1 si cupo. */
static uint8_t Tx_Write(const uint8_t *data, uint16_t len)
{
  uint16_t used = (uint16_t)((tx_head - tx_tail) & (TX_SIZE - 1U));
  uint16_t i;

  if (len > (TX_SIZE - 1U - used))
  {
    return 0U;
  }
  for (i = 0; i < len; i++)
  {
    tx_buf[tx_head] = data[i];
    tx_head = (uint16_t)((tx_head + 1U) & (TX_SIZE - 1U));
  }
  USART1->CR1 |= USART_CR1_TXEIE;
  return 1U;
}

typedef struct
{
  uint8_t  data[MSG_MAX];
  uint16_t len;
} Msg_t;

static void Msg_Str(Msg_t *m, const char *s)
{
  while (*s != '\0' && m->len < (MSG_MAX - 4U))
  {
    m->data[m->len++] = (uint8_t)*s++;
  }
}

static void Msg_Num(Msg_t *m, uint32_t v)
{
  char tmp[11];
  uint8_t n = 0U;

  do
  {
    tmp[n++] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v > 0U);
  while (n > 0U && m->len < (MSG_MAX - 4U))
  {
    m->data[m->len++] = (uint8_t)tmp[--n];
  }
}

/* Toda instruccion de la Nextion termina con tres bytes 0xFF */
static void Msg_End(Msg_t *m)
{
  m->data[m->len++] = 0xFFU;
  m->data[m->len++] = 0xFFU;
  m->data[m->len++] = 0xFFU;
}

static void Msg_AttrNum(Msg_t *m, const char *name, uint32_t v)
{
  Msg_Str(m, name);
  Msg_Num(m, v);
  Msg_End(m);
}

static void Msg_AttrTxt(Msg_t *m, const char *name, const char *txt)
{
  Msg_Str(m, name);
  Msg_Str(m, "\"");
  Msg_Str(m, txt);
  Msg_Str(m, "\"");
  Msg_End(m);
}

static uint8_t Send_State(void)
{
  Msg_t m = {{0}, 0U};
  Machine_State_t st = Machine_GetState();
  const char *txt;
  uint32_t pco;
  const uint32_t bco = COLOR_NEGRO;

  switch (st)
  {
    case MAQ_HACIENDO_HOME: txt = "HACIENDO HOME"; pco = COLOR_AVISO;          break;
    case MAQ_EN_HOME:       txt = "EN HOME";       pco = COLOR_EXITO;          break;
    case MAQ_JOG:           txt = "MOVIENDO";      pco = COLOR_TEXTO_PRIMARIO; break;
    case MAQ_IMPRIMIENDO:   txt = "IMPRIMIENDO";   pco = COLOR_TEXTO_DISPLAY;  break;
    case MAQ_ERROR:         txt = "ERROR";         pco = COLOR_ACENTO;         break;
    case MAQ_SIN_HOME:
    default:                txt = "SIN HOME";      pco = COLOR_TEXTO_SECUND;   break;
  }

  Msg_AttrTxt(&m, "main.t_estado.txt=", txt);
  Msg_AttrNum(&m, "main.t_estado.bco=", bco);
  Msg_AttrNum(&m, "main.t_estado.pco=", pco);

  if (!Tx_Write(m.data, m.len))
  {
    return 0U;
  }
  last_sent_state = (int16_t)st;
  return 1U;
}

static uint8_t Send_Speeds(void)
{
  Msg_t m = {{0}, 0U};
  uint32_t fast, slow;

  Machine_GetHomeSpeed(&fast, &slow);
  Msg_AttrNum(&m, "ajustes.n_rap.val=", fast);
  Msg_AttrNum(&m, "ajustes.n_len.val=", slow);
  return Tx_Write(m.data, m.len);
}

static uint8_t Send_PrintSpeeds(void)
{
  Msg_t m = {{0}, 0U};
  uint32_t stroke, squeegee;

  Machine_GetPrintSpeed(&stroke, &squeegee);
  Msg_AttrNum(&m, "impresion.n_strk.val=", stroke);
  Msg_AttrNum(&m, "impresion.n_sqg.val=", squeegee);
  return Tx_Write(m.data, m.len);
}

static uint8_t Send_Saved(void)
{
  Msg_t m = {{0}, 0U};

  Msg_AttrTxt(&m, "t_msg.txt=", "[GUARDADO]");
  return Tx_Write(m.data, m.len);
}

typedef enum
{
  RX_IDLE = 0,
  RX_CMD,       /* esperando el byte de comando tras '#' */
  RX_PAYLOAD,   /* carga de longitud fija */
  RX_TERM,      /* FF FF FF del mensaje propio */
  RX_READY      /* FF FF FF tras 0x88 de Nextion */
} Rx_State_t;

static Rx_State_t rx_state = RX_IDLE;
static uint8_t rx_cmd, rx_len, rx_count, rx_payload[4], rx_ff;

static uint8_t Payload_Len(uint8_t cmd)
{
  switch (cmd)
  {
    case 'H': return 0U;
    case 'J': return 1U;
    case 'P': return 1U;
    case 'V': return 4U;
    case 'I': return 4U;
    default:  return 0xFFU;   /* comando desconocido */
  }
}

static void Dispatch(void)
{
  switch (rx_cmd)
  {
    case 'H':
      Machine_RequestHome();
      break;

    case 'J':
      if (rx_payload[0] == '+')      { Machine_JogRequest(1); }
      else if (rx_payload[0] == '-') { Machine_JogRequest(-1); }
      else if (rx_payload[0] == 'S') { Machine_JogStop(); }
      break;

    case 'V':
    {
      uint32_t fast = (uint32_t)rx_payload[0] | ((uint32_t)rx_payload[1] << 8);
      uint32_t slow = (uint32_t)rx_payload[2] | ((uint32_t)rx_payload[3] << 8);
      Machine_SetHomeSpeed(fast, slow);
      pend_speeds = 1U;   /* la pantalla muestra el valor realmente aplicado */
      pend_saved = 1U;
      break;
    }

    case 'I':
    {
      uint32_t stroke = (uint32_t)rx_payload[0] | ((uint32_t)rx_payload[1] << 8);
      uint32_t squeegee = (uint32_t)rx_payload[2] | ((uint32_t)rx_payload[3] << 8);
      Machine_SetPrintSpeed(stroke, squeegee);
      pend_print_speeds = 1U;   /* la pantalla muestra el valor realmente aplicado */
      pend_saved = 1U;
      break;
    }

    case 'P':
      if (rx_payload[0] == '0')      { pend_state = 1U; }
      else if (rx_payload[0] == '1') { pend_speeds = 1U; }
      else if (rx_payload[0] == '2') { pend_print_speeds = 1U; }
      break;

    default:
      break;
  }
}

static void Parse_Byte(uint8_t b)
{
  switch (rx_state)
  {
    case RX_IDLE:
      if (b == '#')       { rx_state = RX_CMD; }
      else if (b == 0x88) { rx_state = RX_READY; rx_ff = 0U; }
      break;

    case RX_CMD:
      rx_cmd = b;
      rx_len = Payload_Len(b);
      rx_count = 0U;
      if (rx_len == 0xFFU)     { rx_state = RX_IDLE; }
      else if (rx_len == 0U)   { rx_state = RX_TERM; rx_ff = 0U; }
      else                     { rx_state = RX_PAYLOAD; }
      break;

    case RX_PAYLOAD:
      rx_payload[rx_count++] = b;
      if (rx_count >= rx_len)
      {
        rx_state = RX_TERM;
        rx_ff = 0U;
      }
      break;

    case RX_TERM:
      if (b == 0xFFU)
      {
        if (++rx_ff >= 3U)
        {
          Dispatch();
          rx_state = RX_IDLE;
        }
      }
      else
      {
        /* Terminador invalido: se descarta y se resincroniza */
        rx_state = RX_IDLE;
        Parse_Byte(b);
      }
      break;

    case RX_READY:
      if (b == 0xFFU)
      {
        if (++rx_ff >= 3U)
        {
          Hmi_PushAll();
          rx_state = RX_IDLE;
        }
      }
      else
      {
        rx_state = RX_IDLE;
        Parse_Byte(b);
      }
      break;

    default:
      rx_state = RX_IDLE;
      break;
  }
}

void Hmi_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  gpio.Pin = HMI_TX_PIN | HMI_RX_PIN;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(HMI_PORT, &gpio);

  /* PCLK2 = SystemCoreClock (APB2 sin division). Sobremuestreo x16. */
  USART1->BRR = (SystemCoreClock + (HMI_BAUD / 2U)) / HMI_BAUD;
  USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

  HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(USART1_IRQn);

  Hmi_PushAll();
}

void Hmi_PushAll(void)
{
  pend_state = 1U;
  pend_speeds = 1U;
  pend_print_speeds = 1U;
}

void Hmi_Update(void)
{
  int b;

  while ((b = Rx_Pop()) >= 0)
  {
    Parse_Byte((uint8_t)b);
  }

  /* Si el mensaje no cabe en el buffer, queda pendiente y se reintenta en la siguiente llamada */
  if (pend_state || last_sent_state != (int16_t)Machine_GetState())
  {
    if (Send_State())
    {
      pend_state = 0U;
    }
  }
  if (pend_speeds && Send_Speeds())
  {
    pend_speeds = 0U;
  }
  if (pend_print_speeds && Send_PrintSpeeds())
  {
    pend_print_speeds = 0U;
  }
  if (pend_saved && !pend_speeds && !pend_print_speeds && Send_Saved())
  {
    pend_saved = 0U;
  }
}
