/*******************************************************************************
 * @file    keypad.c
 * @brief   4x3 Keypad - REVERSED PINOUT (Columns first, then Rows)
 ******************************************************************************/

#include "keypad.h"

/* Keypad mapping */
static const char KEYPAD_MAP[4][3] = {
    {'#', '0', '*'},
    {'9', '8', '7'},
    {'6', '5', '4'},
    {'3', '2', '1'}
};

/* YOUR KEYPAD IS REVERSED!
 * Physical Pins 1-3 = COLUMNS (not rows!)
 * Physical Pins 4-7 = ROWS (not columns!)
 */

/* Column pins (Physical 1-3 → PA8, PA11, PA12) */
static GPIO_TypeDef* const COL_PORTS[] = {
    KEYPAD_COL1_PORT,  // PA8  → Physical pin 1
    KEYPAD_COL2_PORT,  // PA11 → Physical pin 2
    KEYPAD_COL3_PORT   // PA12 → Physical pin 3
};

static const uint16_t COL_PINS[] = {
    KEYPAD_COL1_PIN,
    KEYPAD_COL2_PIN,
    KEYPAD_COL3_PIN
};

/* Row pins (Physical 4-7 → PB0, PB1, PB3, PB4) */
static GPIO_TypeDef* const ROW_PORTS[] = {
    KEYPAD_ROW1_PORT,  // PB0 → Physical pin 4
    KEYPAD_ROW2_PORT,  // PB1 → Physical pin 5
    KEYPAD_ROW3_PORT,  // PB3 → Physical pin 6
    KEYPAD_ROW4_PORT   // PB4 → Physical pin 7
};

static const uint16_t ROW_PINS[] = {
    KEYPAD_ROW1_PIN,
    KEYPAD_ROW2_PIN,
    KEYPAD_ROW3_PIN,
    KEYPAD_ROW4_PIN
};

/*******************************************************************************
 * @brief  Initialize keypad (CORRECTED for reversed pinout)
 ******************************************************************************/
void Keypad_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Columns as INPUT with PULLUP (physical pins 1-3) */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < 3; i++) {
        GPIO_InitStruct.Pin = COL_PINS[i];
        HAL_GPIO_Init(COL_PORTS[i], &GPIO_InitStruct);
    }

    /* Rows as OUTPUT (physical pins 4-7, idle HIGH) */
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    for (uint8_t i = 0; i < 4; i++) {
        GPIO_InitStruct.Pin = ROW_PINS[i];
        HAL_GPIO_Init(ROW_PORTS[i], &GPIO_InitStruct);
        HAL_GPIO_WritePin(ROW_PORTS[i], ROW_PINS[i], GPIO_PIN_SET);
    }
}

/*******************************************************************************
 * @brief  Get key (scan rows, read columns)
 ******************************************************************************/
char Keypad_GetKey(void)
{
    for (uint8_t row = 0; row < 4; row++) {
        /* Drive this row LOW */
        HAL_GPIO_WritePin(ROW_PORTS[row], ROW_PINS[row], GPIO_PIN_RESET);
        HAL_Delay(2);

        /* Check all columns */
        for (uint8_t col = 0; col < 3; col++) {
            if (HAL_GPIO_ReadPin(COL_PORTS[col], COL_PINS[col]) == GPIO_PIN_RESET) {
                /* Key pressed! Wait for release */
                while (HAL_GPIO_ReadPin(COL_PORTS[col], COL_PINS[col]) == GPIO_PIN_RESET) {
                    HAL_Delay(10);
                }
                HAL_Delay(KEYPAD_DEBOUNCE);

                /* Restore row HIGH */
                HAL_GPIO_WritePin(ROW_PORTS[row], ROW_PINS[row], GPIO_PIN_SET);

                return KEYPAD_MAP[row][col];
            }
        }

        /* Restore row HIGH */
        HAL_GPIO_WritePin(ROW_PORTS[row], ROW_PINS[row], GPIO_PIN_SET);
    }

    return '\0';
}

/*******************************************************************************
 * @brief  Wait for key press
 ******************************************************************************/
void Keypad_WaitForKey(char *key)
{
    do {
        *key = Keypad_GetKey();
        HAL_Delay(50);
    } while (*key == '\0');
}
