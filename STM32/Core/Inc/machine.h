/* Estado general de la maquina e interfaz con la que la HMI la comanda. */
#ifndef MACHINE_H
#define MACHINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  MAQ_SIN_HOME = 0,
  MAQ_HACIENDO_HOME,
  MAQ_EN_HOME,
  MAQ_ERROR,
  MAQ_JOG
} Machine_State_t;

void Machine_Init(void);
void Machine_Update(void);
void Machine_RequestHome(void);
Machine_State_t Machine_GetState(void);
void Machine_SetHomeSpeed(uint32_t fast_sps, uint32_t slow_sps);
void Machine_GetHomeSpeed(uint32_t *fast_sps, uint32_t *slow_sps);
void Machine_JogRequest(int8_t dir);   /* +1 = hacia afuera del home, -1 = hacia el home */
void Machine_JogStop(void);

#ifdef __cplusplus
}
#endif

#endif /* MACHINE_H */
