/* Configuracion persistente en flash: idioma de la HMI. */
#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  LANG_EN = 0,
  LANG_ES = 1
} Lang_t;

void   Settings_Init(void);
Lang_t Settings_GetLang(void);
void   Settings_SetLang(Lang_t lang);
void   Settings_Update(uint8_t can_write);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
