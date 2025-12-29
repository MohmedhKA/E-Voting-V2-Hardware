/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : esp32_bridge.h
* @brief          : Header for esp32_bridge.c file
******************************************************************************
*/
/* USER CODE END Header */

#ifndef ESP32_BRIDGE_H
#define ESP32_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
typedef enum {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_ERROR
} WiFi_State;

typedef enum {
    ESP32_OK = 0,
    ESP32_ERROR,
    ESP32_TIMEOUT,
    ESP32_NOT_CONNECTED,
    ESP32_INVALID_RESPONSE,
    ESP32_BUFFER_OVERFLOW
} ESP32_Status;

typedef struct {
    uint16_t status_code;
    bool success;
    char body[4096];
    uint16_t body_length;
} HTTP_Response;

/**
 * @brief ESP32 Handle Structure
 * ✅ SIMPLE: No DMA, just interrupt-driven
 */
typedef struct {
    UART_HandleTypeDef *huart;
    char rx_buffer[4096];
    volatile uint16_t rx_index;
    volatile bool response_ready;
    WiFi_State wifi_state;
} ESP32_Handle;

/* Exported constants --------------------------------------------------------*/
#define ESP32_TIMEOUT_SHORT   2000
#define ESP32_TIMEOUT_MEDIUM  5000
#define ESP32_TIMEOUT_LONG    30000
#define ESP32_RX_BUFFER_SIZE  4096
#define ESP32_TX_BUFFER_SIZE  2048


#ifndef API_KEY
#define API_KEY "voter-secret-key-456"
#endif

#ifndef TERMINAL_ID
#define TERMINAL_ID "TERM_001"
#endif

/* Exported functions --------------------------------------------------------*/
bool ESP32_Init(ESP32_Handle *dev, UART_HandleTypeDef *huart);
bool ESP32_TestConnection(ESP32_Handle *dev);
bool ESP32_Reset(ESP32_Handle *dev);
bool ESP32_SendCommand(ESP32_Handle *dev, const char *cmd);
bool ESP32_SendCommandWithResponse(ESP32_Handle *dev, const char *cmd, char *response, uint32_t timeout);
void ESP32_ClearBuffer(ESP32_Handle *dev);
bool ESP32_WaitForResponse(ESP32_Handle *dev, const char *expected, uint32_t timeout);
bool ESP32_LED_On(ESP32_Handle *dev);
bool ESP32_LED_Off(ESP32_Handle *dev);
bool ESP32_LED_Blink(ESP32_Handle *dev, uint8_t times);
bool ESP32_ConnectWiFi(ESP32_Handle *dev, const char *ssid, const char *password);
bool ESP32_DisconnectWiFi(ESP32_Handle *dev);
bool ESP32_CheckConnection(ESP32_Handle *dev);
bool ESP32_GetIP(ESP32_Handle *dev, char *ip_address);
bool ESP32_HTTP_GET(ESP32_Handle *dev, const char *host, uint16_t port, const char *path, HTTP_Response *response);
bool ESP32_HTTP_POST(ESP32_Handle *dev, const char *host, uint16_t port, const char *path, const char *json_data, HTTP_Response *response);
bool JSON_GetString(const char *json, const char *key, char *value, uint16_t max_len);
bool JSON_GetInt(const char *json, const char *key, int32_t *value);
bool JSON_GetBool(const char *json, const char *key, bool *value);
bool JSON_ArrayGetItem(const char *json, const char *array_key, uint16_t index, char *item, uint16_t max_len);
int16_t JSON_ArrayGetCount(const char *json, const char *array_key);
const char* ESP32_GetStatusString(ESP32_Status status);
WiFi_State ESP32_GetWiFiState(ESP32_Handle *dev);

// ✅ NEW: UART RX Callback (called from interrupt)
void ESP32_UART_RxCallback(ESP32_Handle *dev, uint8_t byte);

extern uint8_t uart_rx_byte;
#ifdef __cplusplus
}
#endif

#endif /* ESP32_BRIDGE_H */
