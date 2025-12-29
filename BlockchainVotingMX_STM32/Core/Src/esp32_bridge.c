/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : esp32_bridge.c
* @brief          : ESP32 WiFi Bridge - UART Interrupt Mode
* ✅ Simple, robust, interrupt-driven RX
******************************************************************************
*/
/* USER CODE END Header */

#include "esp32_bridge.h"
#include <stdarg.h>

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
uint8_t uart_rx_byte; // Single byte for interrupt RX

/* Private function prototypes -----------------------------------------------*/
static bool ESP32_ParseHTTPResponse(const char *raw_response, HTTP_Response *response);
static bool ESP32_ValidateConnection(ESP32_Handle *dev);
static void ESP32_DebugPrint(const char *msg);

/* Private user code ---------------------------------------------------------*/

static void ESP32_DebugPrint(const char *msg) {
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
}

/**
 * @brief UART RX Callback - called from interrupt
 */
void ESP32_UART_RxCallback(ESP32_Handle *dev, uint8_t byte) {
    if (dev->rx_index < ESP32_RX_BUFFER_SIZE - 1) {
        dev->rx_buffer[dev->rx_index++] = (char)byte;
        dev->rx_buffer[dev->rx_index] = '\0';
    }
}

/* ========================================================================== */
/* INITIALIZATION FUNCTIONS */
/* ========================================================================== */

bool ESP32_Init(ESP32_Handle *dev, UART_HandleTypeDef *huart) {
    if (!dev || !huart) return false;

    dev->huart = huart;
    dev->rx_index = 0;
    dev->response_ready = false;
    dev->wifi_state = WIFI_DISCONNECTED;

    ESP32_ClearBuffer(dev);

    // ✅ Start interrupt RX for single byte
    HAL_UART_Receive_IT(dev->huart, &uart_rx_byte, 1);

    HAL_Delay(1000);
    return ESP32_TestConnection(dev);
}

bool ESP32_TestConnection(ESP32_Handle *dev) {
    if (!dev) return false;
    char response[64];
    memset(response, 0, sizeof(response));
    if (ESP32_SendCommandWithResponse(dev, "PING\n", response, ESP32_TIMEOUT_SHORT)) {
        return (strstr(response, "PONG") != NULL);
    }
    return false;
}

bool ESP32_Reset(ESP32_Handle *dev) {
    if (!dev) return false;
    char response[64];
    if (ESP32_SendCommandWithResponse(dev, "RESET\n", response, ESP32_TIMEOUT_MEDIUM)) {
        HAL_Delay(2000);
        dev->wifi_state = WIFI_DISCONNECTED;
        return ESP32_TestConnection(dev);
    }
    return false;
}

/* ========================================================================== */
/* COMMAND FUNCTIONS */
/* ========================================================================== */

bool ESP32_SendCommand(ESP32_Handle *dev, const char *cmd) {
    if (!dev || !cmd) return false;
    ESP32_ClearBuffer(dev);
    HAL_StatusTypeDef status = HAL_UART_Transmit(dev->huart, (uint8_t*)cmd, strlen(cmd), 1000);
    return (status == HAL_OK);
}

bool ESP32_SendCommandWithResponse(ESP32_Handle *dev, const char *cmd, char *response, uint32_t timeout) {
    if (!dev || !cmd || !response) return false;
    ESP32_ClearBuffer(dev);

    HAL_StatusTypeDef status = HAL_UART_Transmit(dev->huart, (uint8_t*)cmd, strlen(cmd), 1000);
    if (status != HAL_OK) return false;

    uint32_t start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < timeout) {
        if (strchr(dev->rx_buffer, '\n') != NULL) {
            strcpy(response, dev->rx_buffer);
            return true;
        }
        HAL_Delay(10);
    }

    if (dev->rx_index > 0) {
        strcpy(response, dev->rx_buffer);
        return true;
    }
    return false;
}

/**
 * @brief Clear ESP32 RX buffer safely (PRODUCTION READY)
 * ✅ Stops UART, clears atomically, restarts cleanly
 */
void ESP32_ClearBuffer(ESP32_Handle *dev) {
    if (!dev) return;

    // Stop UART reception completely (aborts any ongoing DMA/IT)
    HAL_UART_AbortReceive_IT(dev->huart);

    // Memory barrier - ensure all pending writes complete before clearing
    __DMB();

    // Clear buffer atomically
    memset((void*)dev->rx_buffer, 0, ESP32_RX_BUFFER_SIZE);
    dev->rx_index = 0;
    dev->response_ready = false;

    // Restart UART reception
    extern uint8_t uart_rx_byte;
    HAL_UART_Receive_IT(dev->huart, &uart_rx_byte, 1);
}


bool ESP32_WaitForResponse(ESP32_Handle *dev, const char *expected, uint32_t timeout) {
    if (!dev || !expected) return false;
    uint32_t start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < timeout) {
        if (strstr(dev->rx_buffer, expected) != NULL) {
            return true;
        }
        HAL_Delay(10);
    }
    return false;
}

/* ========================================================================== */
/* LED, WIFI, etc. - ALL UNCHANGED */
/* ========================================================================== */

bool ESP32_LED_On(ESP32_Handle *dev) {
    char response[32];
    if (ESP32_SendCommandWithResponse(dev, "LED_ON\n", response, ESP32_TIMEOUT_SHORT)) {
        return (strstr(response, "OK") != NULL);
    }
    return false;
}

bool ESP32_LED_Off(ESP32_Handle *dev) {
    char response[32];
    if (ESP32_SendCommandWithResponse(dev, "LED_OFF\n", response, ESP32_TIMEOUT_SHORT)) {
        return (strstr(response, "OK") != NULL);
    }
    return false;
}

bool ESP32_LED_Blink(ESP32_Handle *dev, uint8_t times) {
    char cmd[32];
    char response[32];
    snprintf(cmd, sizeof(cmd), "LED_BLINK,%d\n", times);
    if (ESP32_SendCommandWithResponse(dev, cmd, response, ESP32_TIMEOUT_MEDIUM)) {
        return (strstr(response, "OK") != NULL);
    }
    return false;
}

bool ESP32_ConnectWiFi(ESP32_Handle *dev, const char *ssid, const char *password) {
    if (!dev || !ssid || !password) return false;
    char cmd[256];
    dev->wifi_state = WIFI_CONNECTING;

    snprintf(cmd, sizeof(cmd), "WIFI_CONNECT,%s,%s\n", ssid, password);
    ESP32_ClearBuffer(dev);

    if (!ESP32_SendCommand(dev, cmd)) {
        dev->wifi_state = WIFI_ERROR;
        return false;
    }

    if (ESP32_WaitForResponse(dev, "CONNECTED", 15000)) {
        dev->wifi_state = WIFI_CONNECTED;
        return true;
    }

    dev->wifi_state = WIFI_ERROR;
    return false;
}

bool ESP32_DisconnectWiFi(ESP32_Handle *dev) {
    char response[64];
    if (ESP32_SendCommandWithResponse(dev, "WIFI_DISCONNECT\n", response, ESP32_TIMEOUT_SHORT)) {
        if (strstr(response, "OK") != NULL) {
            dev->wifi_state = WIFI_DISCONNECTED;
            return true;
        }
    }
    return false;
}

bool ESP32_CheckConnection(ESP32_Handle *dev) {
    char response[64];
    if (ESP32_SendCommandWithResponse(dev, "WIFI_STATUS\n", response, ESP32_TIMEOUT_SHORT)) {
        if (strstr(response, "CONNECTED") != NULL) {
            dev->wifi_state = WIFI_CONNECTED;
            return true;
        }
    }
    dev->wifi_state = WIFI_DISCONNECTED;
    return false;
}

bool ESP32_GetIP(ESP32_Handle *dev, char *ip_address) {
    if (!dev || !ip_address) return false;
    char response[64];
    if (!ESP32_SendCommandWithResponse(dev, "WIFI_IP\n", response, ESP32_TIMEOUT_SHORT)) {
        return false;
    }

    char *ip_start = strstr(response, "IP:");
    if (ip_start != NULL) {
        ip_start += 3;
        char *ip_end = strchr(ip_start, '\r');
        if (ip_end == NULL) ip_end = strchr(ip_start, '\n');
        if (ip_end != NULL) {
            size_t len = ip_end - ip_start;
            if (len < 16) {
                strncpy(ip_address, ip_start, len);
                ip_address[len] = '\0';
                return true;
            }
        }
    }
    return false;
}

/* ========================================================================== */
/* HTTP FUNCTIONS */
/* ========================================================================== */

static bool ESP32_ValidateConnection(ESP32_Handle *dev) {
    if (dev->wifi_state != WIFI_CONNECTED) {
        if (!ESP32_CheckConnection(dev)) {
            return false;
        }
    }
    return true;
}

bool ESP32_HTTP_GET(ESP32_Handle *dev, const char *host, uint16_t port,
                    const char *path, HTTP_Response *response) {
    if (!dev || !host || !path || !response) return false;
    if (!ESP32_ValidateConnection(dev)) {
        response->success = false;
        response->status_code = 0;
        return false;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "HTTP_GET,%s,%d,%s,%s,%s\n", host, port, path, API_KEY, TERMINAL_ID);

    ESP32_ClearBuffer(dev);
    if (!ESP32_SendCommand(dev, cmd)) {
        ESP32_DebugPrint("💬 [STM32] ❌ Failed to send GET\r\n");
        return false;
    }

    ESP32_DebugPrint("💬 [STM32] ⏳ Waiting...\r\n");

    uint32_t start_tick = HAL_GetTick();
    bool header_found = false;

    while ((HAL_GetTick() - start_tick) < ESP32_TIMEOUT_LONG) {
        if (!header_found && strstr(dev->rx_buffer, "HTTP_RESPONSE:") != NULL) {
            header_found = true;
            ESP32_DebugPrint("💬 [STM32] ✅ Found header\r\n");
        }

        if (header_found && strstr(dev->rx_buffer, "HTTP_END") != NULL) {
            char debug[128];
            snprintf(debug, sizeof(debug), "💬 [STM32] 📦 Got %d bytes\r\n", dev->rx_index);
            ESP32_DebugPrint(debug);
            return ESP32_ParseHTTPResponse(dev->rx_buffer, response);
        }

        HAL_Delay(50); // Check every 50ms
    }

    ESP32_DebugPrint("💬 [STM32] ⏱️ Timeout!\r\n");
    response->success = false;
    return false;
}

bool ESP32_HTTP_POST(ESP32_Handle *dev, const char *host, uint16_t port,
                     const char *path, const char *json_data, HTTP_Response *response) {
    if (!dev || !host || !path || !json_data || !response) return false;
    if (!ESP32_ValidateConnection(dev)) {
        response->success = false;
        response->status_code = 0;
        return false;
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "HTTP_POST,%s,%d,%s,%s,%s,%s\n", host, port, path, json_data, API_KEY, TERMINAL_ID);

    ESP32_ClearBuffer(dev);
    if (!ESP32_SendCommand(dev, cmd)) {
        ESP32_DebugPrint("💬 [STM32] ❌ Failed to send POST\r\n");
        return false;
    }

    ESP32_DebugPrint("💬 [STM32] ⏳ Waiting...\r\n");

    uint32_t start_tick = HAL_GetTick();
    bool header_found = false;

    while ((HAL_GetTick() - start_tick) < ESP32_TIMEOUT_LONG) {
        if (!header_found && strstr(dev->rx_buffer, "HTTP_RESPONSE:") != NULL) {
            header_found = true;
            ESP32_DebugPrint("💬 [STM32] ✅ Found header\r\n");
        }

        if (header_found && strstr(dev->rx_buffer, "HTTP_END") != NULL) {
            return ESP32_ParseHTTPResponse(dev->rx_buffer, response);
        }

        HAL_Delay(50);
    }

    ESP32_DebugPrint("💬 [STM32] ⏱️ Timeout!\r\n");
    response->success = false;
    return false;
}

/* ========================================================================== */
/* PARSER */
/* ========================================================================== */

static bool ESP32_ParseHTTPResponse(const char *raw_response, HTTP_Response *response) {
    if (!raw_response || !response) return false;

    response->success = false;
    response->status_code = 0;
    response->body_length = 0;
    memset(response->body, 0, sizeof(response->body));

    const char *status_start = strstr(raw_response, "HTTP_RESPONSE:");
    if (!status_start) {
        ESP32_DebugPrint("💬 [STM32] ❌ No HTTP_RESPONSE\r\n");
        return false;
    }

    status_start += strlen("HTTP_RESPONSE:");
    response->status_code = (uint16_t)atoi(status_start);
    response->success = (response->status_code >= 200 && response->status_code < 300);

    char debug[64];
    snprintf(debug, sizeof(debug), "💬 [STM32] Status: %d\r\n", response->status_code);
    ESP32_DebugPrint(debug);

    const char *body_start = strstr(raw_response, "BODY:");
    if (!body_start) {
        ESP32_DebugPrint("💬 [STM32] ❌ No BODY\r\n");
        return false;
    }

    body_start += strlen("BODY:");
    const char *end_marker = strstr(body_start, "HTTP_END");
    if (!end_marker) {
        ESP32_DebugPrint("💬 [STM32] ❌ No HTTP_END\r\n");
        return false;
    }

    const char *body_end = end_marker;
    while (body_end > body_start && (*(body_end - 1) == '\r' || *(body_end - 1) == '\n')) {
        body_end--;
    }

    size_t body_len = (size_t)(body_end - body_start);
    if (body_len >= sizeof(response->body)) {
        body_len = sizeof(response->body) - 1;
    }

    memcpy(response->body, body_start, body_len);
    response->body[body_len] = '\0';
    response->body_length = (uint16_t)body_len;

    snprintf(debug, sizeof(debug), "💬 [STM32] Body: %d bytes\r\n", response->body_length);
    ESP32_DebugPrint(debug);

    ESP32_DebugPrint("💬 [STM32] ✅ Parse OK!\r\n");
    return true;
}

/* ========================================================================== */
/* JSON FUNCTIONS - ALL UNCHANGED */
/* ========================================================================== */

bool JSON_GetString(const char *json, const char *key, char *value, uint16_t max_len) {
    if (!json || !key || !value) return false;
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    char *key_pos = strstr(json, search_key);
    if (key_pos == NULL) return false;

    char *value_start = strchr(key_pos + strlen(search_key), '"');
    if (value_start == NULL) return false;
    value_start++;

    char *value_end = strchr(value_start, '"');
    if (value_end == NULL) return false;

    size_t len = value_end - value_start;
    if (len >= max_len) len = max_len - 1;

    strncpy(value, value_start, len);
    value[len] = '\0';
    return true;
}

bool JSON_GetInt(const char *json, const char *key, int32_t *value) {
    if (!json || !key || !value) return false;
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    char *key_pos = strstr(json, search_key);
    if (key_pos == NULL) return false;

    char *value_start = key_pos + strlen(search_key);
    while (*value_start == ' ' || *value_start == '\t') value_start++;

    *value = atoi(value_start);
    return true;
}

bool JSON_GetBool(const char *json, const char *key, bool *value) {
    if (!json || !key || !value) return false;
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    char *key_pos = strstr(json, search_key);
    if (key_pos == NULL) return false;

    char *value_start = key_pos + strlen(search_key);
    while (*value_start == ' ' || *value_start == '\t') value_start++;

    if (strncmp(value_start, "true", 4) == 0) {
        *value = true;
        return true;
    } else if (strncmp(value_start, "false", 5) == 0) {
        *value = false;
        return true;
    }
    return false;
}

bool JSON_ArrayGetItem(const char *json, const char *array_key, uint16_t index,
                       char *item, uint16_t max_len) {
    if (!json || !array_key || !item) return false;
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":[", array_key);
    char *array_start = strstr(json, search_key);
    if (array_start == NULL) return false;
    array_start += strlen(search_key);

    uint16_t current_index = 0;
    char *current_pos = array_start;
    while (current_index < index) {
        current_pos = strchr(current_pos, ',');
        if (current_pos == NULL) return false;
        current_pos++;
        current_index++;
    }

    while (*current_pos == ' ' || *current_pos == '\t' || *current_pos == '\n')
        current_pos++;

    if (*current_pos == '{') {
        char *obj_end = strchr(current_pos, '}');
        if (obj_end == NULL) return false;
        size_t len = obj_end - current_pos + 1;
        if (len >= max_len) len = max_len - 1;
        strncpy(item, current_pos, len);
        item[len] = '\0';
        return true;
    }
    return false;
}

int16_t JSON_ArrayGetCount(const char *json, const char *array_key) {
    if (!json || !array_key) return -1;
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":[", array_key);
    char *array_start = strstr(json, search_key);
    if (array_start == NULL) return -1;
    array_start += strlen(search_key);

    int16_t count = 0;
    char *pos = array_start;
    bool in_item = false;
    while (*pos != ']' && *pos != '\0') {
        if (*pos == '{' || (*pos >= '0' && *pos <= '9') || *pos == '"') {
            if (!in_item) {
                count++;
                in_item = true;
            }
        }
        if (*pos == ',' || *pos == ']') {
            in_item = false;
        }
        pos++;
    }
    return count;
}

const char* ESP32_GetStatusString(ESP32_Status status) {
    switch (status) {
        case ESP32_OK: return "OK";
        case ESP32_ERROR: return "Error";
        case ESP32_TIMEOUT: return "Timeout";
        case ESP32_NOT_CONNECTED: return "Not Connected";
        case ESP32_INVALID_RESPONSE: return "Invalid Response";
        case ESP32_BUFFER_OVERFLOW: return "Buffer Overflow";
        default: return "Unknown";
    }
}

WiFi_State ESP32_GetWiFiState(ESP32_Handle *dev) {
    return dev ? dev->wifi_state : WIFI_DISCONNECTED;
}
