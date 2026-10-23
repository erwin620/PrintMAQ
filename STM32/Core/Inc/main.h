/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */
/* Driver DM556 - eje stroke (cableado catodo comun: "+" al GPIO, "-" a GND) */
#define STROKE_PUL_PORT       GPIOC
#define STROKE_PUL_PIN        GPIO_PIN_0
#define STROKE_DIR_PORT       GPIOC
#define STROKE_DIR_PIN        GPIO_PIN_1

/* Sensor de home stroke: inductivo NPN normalmente abierto, activo en bajo */
#define STROKE_HOME_PORT      GPIOC
#define STROKE_HOME_PIN       GPIO_PIN_6
#define STROKE_HOME_ACTIVE    GPIO_PIN_RESET

/* Nivel de DIR que mueve el carro hacia el sensor de home.
   TODO: confirmar el sentido en banco. */
#define STROKE_HOME_DIR       GPIO_PIN_RESET

/* Driver DM556 - eje squeegee, mismo cableado que el stroke */
#define SQUEEGEE_PUL_PORT     GPIOC
#define SQUEEGEE_PUL_PIN      GPIO_PIN_3
#define SQUEEGEE_DIR_PORT     GPIOC
#define SQUEEGEE_DIR_PIN      GPIO_PIN_4

/* Sensor de home squeegee (arriba): inductivo NPN normalmente abierto, activo en bajo */
#define SQUEEGEE_HOME_PORT    GPIOC
#define SQUEEGEE_HOME_PIN     GPIO_PIN_7
#define SQUEEGEE_HOME_ACTIVE  GPIO_PIN_RESET

/* Nivel de DIR que sube el squeegee hacia su sensor de home.
   TODO: confirmar el sentido en banco. */
#define SQUEEGEE_HOME_DIR     GPIO_PIN_RESET

/* Sensor de prenda: NPN de colector abierto, activo en bajo */
#define PRENDA_PORT           GPIOB
#define PRENDA_PIN            GPIO_PIN_0
#define PRENDA_ACTIVE         GPIO_PIN_RESET

/* LEDs de la tarjeta (activos en bajo) */
#define LED_D2_PORT           GPIOA
#define LED_D2_PIN            GPIO_PIN_6
#define LED_D3_PORT           GPIOA
#define LED_D3_PIN            GPIO_PIN_7

/* HMI Nextion en USART1: PA9 = TX (al RX de la pantalla), PA10 = RX (al TX de la pantalla) */
#define HMI_PORT              GPIOA
#define HMI_TX_PIN            GPIO_PIN_9
#define HMI_RX_PIN            GPIO_PIN_10
#define HMI_BAUD              9600U

/* Modo de banco sin HMI: K_UP pide el home para probar sin la pantalla Nextion.
   TODO: comentar este define cuando la HMI este cargada y probada. */
#define PRUEBA_BANCO_SIN_HMI

/* Boton K_UP de la tarjeta: conecta a 3.3 V al presionarse, activo en alto */
#define KEY_UP_PORT           GPIOA
#define KEY_UP_PIN            GPIO_PIN_0
#define KEY_UP_ACTIVE         GPIO_PIN_SET
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
