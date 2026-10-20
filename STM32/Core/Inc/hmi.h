/* Comunicacion con la pantalla Nextion por USART1 (9600 8N1). */
#ifndef HMI_H
#define HMI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void Hmi_Init(void);
void Hmi_Update(void);
void Hmi_PushAll(void);

#ifdef __cplusplus
}
#endif

#endif /* HMI_H */
