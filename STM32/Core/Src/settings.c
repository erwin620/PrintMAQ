/* Configuracion persistente en el sector 7 de la flash. Borrar el sector detiene la CPU ~1-2 s
   (la flash es de un solo banco), con ella la interrupcion de pulsos; por eso la grabacion
   es diferida y solo ocurre cuando quien llama indica que no hay ejes en movimiento. */
#include "settings.h"
#include "stm32f4xx_hal.h"

#define SETTINGS_SECTOR   FLASH_SECTOR_7
#define SETTINGS_ADDR     0x08060000UL
#define SETTINGS_MAGIC    0x31534D50UL   /* "PMS1" */

typedef struct
{
  uint32_t magic;
  uint32_t lang;
  uint32_t check;   /* magic XOR lang: descarta un registro a medio grabar */
} Settings_Flash_t;

static volatile Lang_t  lang = LANG_EN;
static volatile uint8_t dirty = 0U;

void Settings_Init(void)
{
  const Settings_Flash_t *f = (const Settings_Flash_t *)SETTINGS_ADDR;

  if (f->magic == SETTINGS_MAGIC && f->lang <= (uint32_t)LANG_ES && f->check == (f->magic ^ f->lang))
  {
    lang = (Lang_t)f->lang;
  }
  else
  {
    lang = LANG_EN;
  }
  dirty = 0U;
}

Lang_t Settings_GetLang(void)
{
  return lang;
}

void Settings_SetLang(Lang_t new_lang)
{
  if (new_lang > LANG_ES)
  {
    return;
  }
  if (new_lang != lang)
  {
    lang = new_lang;
    dirty = 1U;
  }
}

static void Settings_Save(void)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0U;
  uint32_t value = (uint32_t)lang;

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Sector = SETTINGS_SECTOR;
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  HAL_FLASH_Unlock();
  if (HAL_FLASHEx_Erase(&erase, &sector_error) == HAL_OK)
  {
    /* La firma va al final: si se corta la energia a mitad, el registro queda invalido y se usa el valor por defecto */
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, SETTINGS_ADDR + 4U, value);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, SETTINGS_ADDR + 8U, SETTINGS_MAGIC ^ value);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, SETTINGS_ADDR, SETTINGS_MAGIC);
  }
  HAL_FLASH_Lock();
}

/* Se llama en cada vuelta de la tarea de maquina; can_write = 0 mientras algun eje se mueve. */
void Settings_Update(uint8_t can_write)
{
  if (!dirty || !can_write)
  {
    return;
  }
  dirty = 0U;
  Settings_Save();
}
