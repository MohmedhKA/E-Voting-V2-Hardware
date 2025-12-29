/*******************************************************************************
 * @file    keypad.h
 * @brief   4x3 Matrix Keypad Header
 ******************************************************************************/

#ifndef KEYPAD_H
#define KEYPAD_H

#include "stm32l4xx_hal.h"

/* Keypad Pin Definitions */
// Rows (Output) - Using PB0, PB1, PB3, PB4
#define KEYPAD_ROW1_PORT    GPIOB
#define KEYPAD_ROW1_PIN     GPIO_PIN_0

#define KEYPAD_ROW2_PORT    GPIOB
#define KEYPAD_ROW2_PIN     GPIO_PIN_1

#define KEYPAD_ROW3_PORT    GPIOB
#define KEYPAD_ROW3_PIN     GPIO_PIN_3

#define KEYPAD_ROW4_PORT    GPIOB
#define KEYPAD_ROW4_PIN     GPIO_PIN_4

// Columns (Input) - Using PA8, PA11, PA12
#define KEYPAD_COL1_PORT    GPIOA
#define KEYPAD_COL1_PIN     GPIO_PIN_8

#define KEYPAD_COL2_PORT    GPIOA
#define KEYPAD_COL2_PIN     GPIO_PIN_11

#define KEYPAD_COL3_PORT    GPIOA
#define KEYPAD_COL3_PIN     GPIO_PIN_12

/* Debounce delay (ms) */
#define KEYPAD_DEBOUNCE     50

/* Function Prototypes */
void Keypad_Init(void);
char Keypad_GetKey(void);
void Keypad_WaitForKey(char *key);
uint8_t Keypad_GetString(char *buffer, uint8_t max_len, uint8_t echo_row);
void Keypad_GetNumberString(char *buffer, uint8_t max_len, uint8_t echo_row);
uint8_t Keypad_GetRawPosition(uint8_t *row_out, uint8_t *col_out);  // ← ADD THIS!
char Keypad_GetCharFromPosition(uint8_t row, uint8_t col);          // ← ADD THIS!

#endif /* KEYPAD_H */
