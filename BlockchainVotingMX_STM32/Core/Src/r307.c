/*******************************************************************************
  * @file           r307.c
  * @brief          R307 Fingerprint Sensor Implementation (UART Buffer Fix Edition)
  ******************************************************************************/

#include "r307.h"
#include <string.h>

/* Private Variables */
static uint8_t last_error = R307_OK;
static uint8_t rx_buffer[R307_MAX_PACKET_SIZE];

/* Private Function Prototypes */
static bool R307_SendPacket(R307_Handle *dev, uint8_t type, uint8_t *data, uint16_t len);
static bool R307_ReceivePacket(R307_Handle *dev, uint8_t *data, uint16_t *len);
static uint16_t R307_CalculateChecksum(uint8_t *data, uint16_t len);
static bool R307_VerifyChecksum(uint8_t *packet, uint16_t len);
/* At the top with other externs */
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;


/*******************************************************************************
  * @brief  Flush UART RX buffer to remove stray bytes
  * @param  dev: Pointer to R307 handle
  * @note   THIS IS THE KEY FIX! Prevents buffer corruption
  ******************************************************************************/
void R307_FlushUART(R307_Handle *dev)
{
    uint8_t dummy;
    uint32_t start_tick = HAL_GetTick();

    // Read and discard all pending bytes (max 100ms timeout)
    while ((HAL_GetTick() - start_tick) < 100) {
        if (HAL_UART_Receive(dev->huart, &dummy, 1, 1) != HAL_OK) {
            break; // No more data available
        }
    }

    // Small delay to ensure UART peripheral stabilizes
    HAL_Delay(10);
}

/*******************************************************************************
  * @brief  Initialize R307 sensor
  * @param  dev: Pointer to R307 handle
  * @param  huart: Pointer to UART handle
  * @retval true if successful
  ******************************************************************************/
bool R307_Init(R307_Handle *dev, UART_HandleTypeDef *huart)
{
    dev->huart = huart;
    dev->address = R307_DEFAULT_ADDR;
    dev->password = R307_DEFAULT_PASSWORD;

    /* Small delay for sensor initialization */
    HAL_Delay(500);

    /* 🔧 FLUSH BUFFER BEFORE FIRST COMMAND */
    R307_FlushUART(dev);

    /* Verify password */
    if (!R307_VerifyPassword(dev)) {
        last_error = R307_ERR_COMM;
        return false;
    }

    /* Read system parameters */
    if (!R307_ReadSystemParameters(dev)) {
        return false;
    }

    return true;
}

/*******************************************************************************
  * @brief  Verify sensor password
  * @param  dev: Pointer to R307 handle
  * @retval true if password correct
  ******************************************************************************/
bool R307_VerifyPassword(R307_Handle *dev)
{
    uint8_t data[5];
    data[0] = R307_CMD_VERIFY_PWD;
    data[1] = (dev->password >> 24) & 0xFF;
    data[2] = (dev->password >> 16) & 0xFF;
    data[3] = (dev->password >> 8) & 0xFF;
    data[4] = dev->password & 0xFF;

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 5)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Capture fingerprint image
  * @param  dev: Pointer to R307 handle
  * @retval true if image captured
  ******************************************************************************/
bool R307_GetImage(R307_Handle *dev)
{
    /* 🔧 FLUSH BEFORE COMMAND - Prevents stray byte corruption */
    R307_FlushUART(dev);

    uint8_t data[1] = {R307_CMD_GEN_IMAGE};

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 1)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Convert image to character file
  * @param  dev: Pointer to R307 handle
  * @param  buffer_id: R307_BUFFER_1 or R307_BUFFER_2
  * @retval true if successful
  ******************************************************************************/
bool R307_Image2Tz(R307_Handle *dev, uint8_t buffer_id)
{
    uint8_t data[2];
    data[0] = R307_CMD_IMG_2TZ;
    data[1] = buffer_id;

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 2)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Create template model from two buffers
  * @param  dev: Pointer to R307 handle
  * @retval true if successful
  ******************************************************************************/
bool R307_CreateModel(R307_Handle *dev)
{
    uint8_t data[1] = {R307_CMD_REG_MODEL};

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 1)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Store template to library
  * @param  dev: Pointer to R307 handle
  * @param  buffer_id: R307_BUFFER_1 or R307_BUFFER_2
  * @param  location: Storage location (0-999)
  * @retval true if successful
  ******************************************************************************/
bool R307_StoreModel(R307_Handle *dev, uint8_t buffer_id, uint16_t location)
{
    uint8_t data[4];
    data[0] = R307_CMD_STORE;
    data[1] = buffer_id;
    data[2] = (location >> 8) & 0xFF;
    data[3] = location & 0xFF;

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 4)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Search fingerprint library
  * @param  dev: Pointer to R307 handle
  * @param  buffer_id: R307_BUFFER_1 or R307_BUFFER_2
  * @param  found_id: Pointer to store matched ID
  * @param  score: Pointer to store matching score
  * @retval true if match found
  ******************************************************************************/
bool R307_Search(R307_Handle *dev, uint8_t buffer_id, uint16_t *found_id, uint16_t *score)
{
    uint8_t data[6];
    data[0] = R307_CMD_SEARCH;
    data[1] = buffer_id;
    data[2] = 0x00;  // Start page (high byte)
    data[3] = 0x00;  // Start page (low byte)
    data[4] = 0x03;  // Number of pages (high byte) - 1000 templates
    data[5] = 0xE8;  // Number of pages (low byte)

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 6)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    if (rx_buffer[0] == R307_OK) {
        *found_id = (rx_buffer[1] << 8) | rx_buffer[2];
        *score = (rx_buffer[3] << 8) | rx_buffer[4];
        return true;
    }

    return false;
}

/*******************************************************************************
  * @brief  Match two templates in buffers
  * @param  dev: Pointer to R307 handle
  * @param  score: Pointer to store matching score
  * @retval true if templates match
  ******************************************************************************/
bool R307_Match(R307_Handle *dev, uint16_t *score)
{
    uint8_t data[1] = {R307_CMD_MATCH};

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 1)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    if (rx_buffer[0] == R307_OK) {
        *score = (rx_buffer[1] << 8) | rx_buffer[2];
        return true;
    }

    return false;
}

/*******************************************************************************
  * @brief  Delete template(s) from library
  * @param  dev: Pointer to R307 handle
  * @param  id: Starting ID
  * @param  count: Number of templates to delete
  * @retval true if successful
  ******************************************************************************/
bool R307_DeleteModel(R307_Handle *dev, uint16_t id, uint16_t count)
{
    uint8_t data[5];
    data[0] = R307_CMD_DELETE_CHAR;
    data[1] = (id >> 8) & 0xFF;
    data[2] = id & 0xFF;
    data[3] = (count >> 8) & 0xFF;
    data[4] = count & 0xFF;

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 5)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Empty entire fingerprint library
  * @param  dev: Pointer to R307 handle
  * @retval true if successful
  ******************************************************************************/
bool R307_EmptyLibrary(R307_Handle *dev)
{
    uint8_t data[1] = {R307_CMD_EMPTY};

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 1)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    return (rx_buffer[0] == R307_OK);
}

/*******************************************************************************
  * @brief  Get number of stored templates
  * @param  dev: Pointer to R307 handle
  * @param  count: Pointer to store template count
  * @retval true if successful
  ******************************************************************************/
bool R307_GetTemplateCount(R307_Handle *dev, uint16_t *count)
{
    uint8_t data[1] = {R307_CMD_TEMPLATE_NUM};

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 1)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    if (rx_buffer[0] == R307_OK) {
        *count = (rx_buffer[1] << 8) | rx_buffer[2];
        return true;
    }

    return false;
}

/*******************************************************************************
  * @brief  Read system parameters
  * @param  dev: Pointer to R307 handle
  * @retval true if successful
  ******************************************************************************/
bool R307_ReadSystemParameters(R307_Handle *dev)
{
    uint8_t data[1] = {R307_CMD_READ_SYS_PARA};

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 1)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    if (rx_buffer[0] == R307_OK && len >= 16) {
        dev->library_size = (rx_buffer[5] << 8) | rx_buffer[6];
        dev->security_level = rx_buffer[7];
        dev->baud_rate = ((rx_buffer[11] << 8) | rx_buffer[12]) * 9600;
        return true;
    }

    return false;
}

/*******************************************************************************
  * @brief  Capture and search in one operation (HIGH-LEVEL)
  * @param  dev: Pointer to R307 handle
  * @param  finger_id: Pointer to store matched ID
  * @param  score: Pointer to store matching score
  * @retval true if finger found
  ******************************************************************************/
bool R307_CaptureAndSearch(R307_Handle *dev, uint16_t *finger_id, uint16_t *score)
{
    /* Get image */
    if (!R307_GetImage(dev)) {
        return false;
    }

    /* Convert to template */
    if (!R307_Image2Tz(dev, R307_BUFFER_1)) {
        return false;
    }

    /* Search library */
    return R307_Search(dev, R307_BUFFER_1, finger_id, score);
}

/*******************************************************************************
  * @brief  Enroll new fingerprint (HIGH-LEVEL)
  * @param  dev: Pointer to R307 handle
  * @param  location: Storage location (0-999)
  * @retval true if successful
  ******************************************************************************/
bool R307_EnrollFingerprint(R307_Handle *dev, uint16_t location)
{
    /* First scan */
    if (!R307_GetImage(dev)) {
        return false;
    }

    if (!R307_Image2Tz(dev, R307_BUFFER_1)) {
        return false;
    }

    /* Wait for finger removal */
    HAL_Delay(1000);

    /* Second scan */
    if (!R307_GetImage(dev)) {
        return false;
    }

    if (!R307_Image2Tz(dev, R307_BUFFER_2)) {
        return false;
    }

    /* Create model */
    if (!R307_CreateModel(dev)) {
        return false;
    }

    /* Store template */
    return R307_StoreModel(dev, R307_BUFFER_1, location);
}

/*******************************************************************************
  * @brief  Get last error code
  * @param  dev: Pointer to R307 handle
  * @retval Error code
  ******************************************************************************/
uint8_t R307_GetLastError(R307_Handle *dev)
{
    return last_error;
}

/*******************************************************************************
  * PRIVATE FUNCTIONS
  ******************************************************************************/
static bool R307_SendPacket(R307_Handle *dev, uint8_t type, uint8_t *data, uint16_t len)
{
    uint8_t packet[R307_MAX_PACKET_SIZE];
    uint16_t idx = 0;

    /* Header (2 bytes) */
    packet[idx++] = (R307_START_CODE >> 8) & 0xFF;
    packet[idx++] = R307_START_CODE & 0xFF;

    /* Address (4 bytes) */
    packet[idx++] = (dev->address >> 24) & 0xFF;
    packet[idx++] = (dev->address >> 16) & 0xFF;
    packet[idx++] = (dev->address >> 8) & 0xFF;
    packet[idx++] = dev->address & 0xFF;

    /* Package identifier (1 byte) */
    packet[idx++] = type;

    /* Package length (2 bytes) - includes data + checksum */
    uint16_t pkg_len = len + 2;
    packet[idx++] = (pkg_len >> 8) & 0xFF;
    packet[idx++] = pkg_len & 0xFF;

    /* Data */
    memcpy(&packet[idx], data, len);
    idx += len;

    /* Checksum (2 bytes) */
    uint16_t checksum = R307_CalculateChecksum(&packet[6], len + 3);
    packet[idx++] = (checksum >> 8) & 0xFF;
    packet[idx++] = checksum & 0xFF;

    /* Send packet */
    HAL_StatusTypeDef status = HAL_UART_Transmit(dev->huart, packet, idx, R307_TIMEOUT_MS);
    return (status == HAL_OK);
}

static bool R307_ReceivePacket(R307_Handle *dev, uint8_t *data, uint16_t *len)
{
    uint8_t header[9];

    /* Receive header */
    if (HAL_UART_Receive(dev->huart, header, 9, R307_TIMEOUT_MS) != HAL_OK) {
        return false;
    }

    /* Verify start code */
    uint16_t start = (header[0] << 8) | header[1];
    if (start != R307_START_CODE) {
        return false;
    }

    /* Get package length */
    uint16_t pkg_len = (header[7] << 8) | header[8];
    if (pkg_len > R307_MAX_PACKET_SIZE - 9) {
        return false;
    }

    /* Receive data + checksum */
    if (HAL_UART_Receive(dev->huart, rx_buffer, pkg_len, R307_TIMEOUT_MS) != HAL_OK) {
        return false;
    }

    /* Verify checksum */
    uint16_t calc_checksum = R307_CalculateChecksum(&header[6], 3);
    calc_checksum += R307_CalculateChecksum(rx_buffer, pkg_len - 2);
    uint16_t recv_checksum = (rx_buffer[pkg_len - 2] << 8) | rx_buffer[pkg_len - 1];

    if (calc_checksum != recv_checksum) {
        return false;
    }

    /* Copy data (excluding checksum) */
    *len = pkg_len - 2;
    memcpy(data, rx_buffer, *len);
    return true;
}

static uint16_t R307_CalculateChecksum(uint8_t *data, uint16_t len)
{
    uint16_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static bool R307_VerifyChecksum(uint8_t *packet, uint16_t len)
{
    if (len < 11) return false;

    uint16_t calc_checksum = R307_CalculateChecksum(&packet[6], len - 8);
    uint16_t recv_checksum = (packet[len - 2] << 8) | packet[len - 1];

    return (calc_checksum == recv_checksum);
}

/*******************************************************************************
  * @brief  Upload template from sensor buffer to MCU
  * @param  dev: Pointer to R307 handle
  * @param  buffer_id: R307_BUFFER_1 or R307_BUFFER_2
  * @param  template_data: Pointer to store 512-byte template
  * @retval true if successful
  ******************************************************************************/
bool R307_UploadTemplate(R307_Handle *dev, uint8_t buffer_id, uint8_t *template_data)
{
    /* 🔧 FLUSH BEFORE MULTI-PACKET OPERATION */
    R307_FlushUART(dev);

    uint8_t data[2];
    data[0] = R307_CMD_UP_CHAR;
    data[1] = buffer_id;

    if (!R307_SendPacket(dev, R307_COMMAND_PACKET, data, 2)) {
        return false;
    }

    uint16_t len;
    if (!R307_ReceivePacket(dev, rx_buffer, &len)) {
        return false;
    }

    last_error = rx_buffer[0];
    if (rx_buffer[0] != R307_OK) {
        return false;
    }

    /* Now receive data packets containing the template */
    uint16_t template_index = 0;
    bool receiving = true;

    while (receiving && template_index < 512) {  // Changed from 256 to 512
        uint8_t header[9];

        /* Receive packet header */
        if (HAL_UART_Receive(dev->huart, header, 9, R307_TIMEOUT_MS) != HAL_OK) {
            /* 🔧 FLUSH ON ERROR */
            R307_FlushUART(dev);
            return false;
        }

        /* Get package type and length */
        uint8_t pkg_type = header[6];
        uint16_t pkg_len = (header[7] << 8) | header[8];

        /* Receive data */
        uint8_t data_buffer[256];
        if (HAL_UART_Receive(dev->huart, data_buffer, pkg_len, R307_TIMEOUT_MS) != HAL_OK) {
            /* 🔧 FLUSH ON ERROR */
            R307_FlushUART(dev);
            return false;
        }

        /* Copy template data (excluding checksum) */
        uint16_t data_len = pkg_len - 2;
        if (template_index + data_len > 512) {
            data_len = 512 - template_index;
        }

        memcpy(&template_data[template_index], data_buffer, data_len);
        template_index += data_len;

        /* Check if this is the end packet */
        if (pkg_type == R307_END_DATA_PACKET) {
            receiving = false;
        }
    }

    /* 🔧 CRITICAL: FLUSH AFTER MULTI-PACKET OPERATION */
    R307_FlushUART(dev);

    return (template_index > 0);
}

/**
 * @brief Download template to sensor buffer (DMA + AGGRESSIVE BUFFER CLEAR)
 * @param handle: Pointer to R307 handle
 * @param buffer_id: R307_BUFFER_1 or R307_BUFFER_2
 * @param template_data: 512-byte template from backend
 * @retval true if success
 */
bool R307_DownloadTemplate(R307_Handle *handle, uint8_t buffer_id, uint8_t *template_data)
{
    extern UART_HandleTypeDef huart2; // For debug

    /* 🔧 AGGRESSIVE FLUSH - Clear everything */
    HAL_UART_AbortReceive(handle->huart);  // Stop any ongoing reception
    R307_FlushUART(handle);
    HAL_Delay(200);  // Longer stabilization time

    uint8_t packet[650];
    uint16_t idx = 0;

    // ═══════════════════════════════════════════════════════
    // STEP 1: Build and send DownChar command
    // ═══════════════════════════════════════════════════════

    packet[idx++] = (R307_START_CODE >> 8) & 0xFF;  // 0xEF
    packet[idx++] = R307_START_CODE & 0xFF;          // 0x01

    packet[idx++] = (handle->address >> 24) & 0xFF;
    packet[idx++] = (handle->address >> 16) & 0xFF;
    packet[idx++] = (handle->address >> 8) & 0xFF;
    packet[idx++] = handle->address & 0xFF;

    packet[idx++] = R307_COMMAND_PACKET;  // 0x01

    uint16_t pkg_len = 3;
    packet[idx++] = (pkg_len >> 8) & 0xFF;
    packet[idx++] = pkg_len & 0xFF;

    packet[idx++] = R307_CMD_DOWN_CHAR;  // 0x09
    packet[idx++] = buffer_id;

    uint16_t checksum = R307_COMMAND_PACKET + pkg_len + R307_CMD_DOWN_CHAR + buffer_id;
    packet[idx++] = (checksum >> 8) & 0xFF;
    packet[idx++] = checksum & 0xFF;

    // Send command via DMA
    if (HAL_UART_Transmit_DMA(handle->huart, packet, idx) != HAL_OK) {
        HAL_UART_Transmit(&huart2, (uint8_t*)"[R307] TX DMA fail\r\n", 20, 100);
        return false;
    }

    // Wait for TX to complete
    uint32_t start_tick = HAL_GetTick();
    while (handle->huart->gState != HAL_UART_STATE_READY) {
        if ((HAL_GetTick() - start_tick) > 1000) {
            HAL_UART_Transmit(&huart2, (uint8_t*)"[R307] TX timeout\r\n", 19, 100);
            return false;
        }
        HAL_Delay(5);
    }

    HAL_Delay(300); // INCREASED: Give sensor more time to prepare response

    // ═══════════════════════════════════════════════════════
    // STEP 2: RECEIVE ACK VIA DMA WITH VERIFICATION
    // ═══════════════════════════════════════════════════════

    // Clear buffer completely
    static uint8_t ack[12] __attribute__((aligned(4)));  // DMA-friendly alignment
    memset(ack, 0xFF, sizeof(ack));  // Fill with 0xFF to detect partial reads

    // Abort any pending reception
    HAL_UART_AbortReceive(handle->huart);
    HAL_Delay(20);

    // Start DMA reception of exactly 12 bytes
    if (HAL_UART_Receive_DMA(handle->huart, ack, 12) != HAL_OK) {
        HAL_UART_Transmit(&huart2, (uint8_t*)"[R307] DMA start fail\r\n", 23, 100);
        return false;
    }

    // Wait for DMA to complete with detailed monitoring
    start_tick = HAL_GetTick();
    uint32_t last_count = 0xFFFF;

    while (handle->huart->RxState != HAL_UART_STATE_READY) {
        uint32_t elapsed = HAL_GetTick() - start_tick;

        // Check how many bytes DMA has received so far
        uint32_t bytes_remaining = __HAL_DMA_GET_COUNTER(handle->huart->hdmarx);
        uint32_t bytes_received = 12 - bytes_remaining;

        // Debug: Print progress if changed
        if (bytes_received != last_count) {
            char progress[64];
            sprintf(progress, "[R307] RX: %lu/12 bytes\r\n", bytes_received);
            HAL_UART_Transmit(&huart2, (uint8_t*)progress, strlen(progress), 100);
            last_count = bytes_received;
        }

        if (elapsed > 3000) {  // 3 second timeout
            char debug[80];
            sprintf(debug, "[R307] Timeout! Got %lu/12 bytes\r\n", bytes_received);
            HAL_UART_Transmit(&huart2, (uint8_t*)debug, strlen(debug), 100);

            // Print what we received
            sprintf(debug, "[R307] Buffer: %02X %02X %02X %02X %02X %02X...\r\n",
                    ack[0], ack[1], ack[2], ack[3], ack[4], ack[5]);
            HAL_UART_Transmit(&huart2, (uint8_t*)debug, strlen(debug), 100);

            HAL_UART_AbortReceive(handle->huart);
            R307_FlushUART(handle);
            return false;
        }

        HAL_Delay(10);
    }

    // ═══════════════════════════════════════════════════════
    // STEP 3: VALIDATE RECEIVED ACK
    // ═══════════════════════════════════════════════════════

    // Print full ACK for debugging
    char ack_debug[128];
    sprintf(ack_debug, "[R307] ACK: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
            ack[0], ack[1], ack[2], ack[3], ack[4], ack[5],
            ack[6], ack[7], ack[8], ack[9], ack[10], ack[11]);
    HAL_UART_Transmit(&huart2, (uint8_t*)ack_debug, strlen(ack_debug), 100);

    // Check for start code
    if (ack[0] != 0xEF || ack[1] != 0x01) {
        sprintf(ack_debug, "[R307] Bad header: %02X %02X (expected EF 01)\r\n", ack[0], ack[1]);
        HAL_UART_Transmit(&huart2, (uint8_t*)ack_debug, strlen(ack_debug), 100);
        R307_FlushUART(handle);
        return false;
    }

    // Check packet type
    if (ack[6] != R307_ACK_PACKET) {
        sprintf(ack_debug, "[R307] Wrong type: 0x%02X (expected 0x%02X)\r\n",
                ack[6], R307_ACK_PACKET);
        HAL_UART_Transmit(&huart2, (uint8_t*)ack_debug, strlen(ack_debug), 100);
        R307_FlushUART(handle);
        return false;
    }

    // Check confirmation code
    if (ack[9] != R307_OK) {
        sprintf(ack_debug, "[R307] Sensor error: 0x%02X\r\n", ack[9]);
        HAL_UART_Transmit(&huart2, (uint8_t*)ack_debug, strlen(ack_debug), 100);

        // Decode error
        switch (ack[9]) {
            case 0x01: HAL_UART_Transmit(&huart2, (uint8_t*)"  -> Packet receive error\r\n", 28, 100); break;
            case 0x06: HAL_UART_Transmit(&huart2, (uint8_t*)"  -> Failed to generate char file\r\n", 36, 100); break;
            case 0x0D: HAL_UART_Transmit(&huart2, (uint8_t*)"  -> Failed to transfer/download\r\n", 35, 100); break;
            default: break;
        }

        last_error = ack[9];
        R307_FlushUART(handle);
        return false;
    }

    HAL_UART_Transmit(&huart2, (uint8_t*)"[R307] ✅ ACK verified!\r\n", 27, 100);

    // ═══════════════════════════════════════════════════════
    // STEP 4: Send template data in packets via DMA
    // ═══════════════════════════════════════════════════════

    uint16_t bytes_sent = 0;
    const uint16_t CHUNK_SIZE = 128;
    uint8_t packet_num = 1;

    while (bytes_sent < 512) {
        idx = 0;
        uint16_t chunk_len = (512 - bytes_sent < CHUNK_SIZE) ? (512 - bytes_sent) : CHUNK_SIZE;
        bool is_last_packet = (bytes_sent + chunk_len >= 512);

        // Build data packet
        packet[idx++] = 0xEF;
        packet[idx++] = 0x01;

        packet[idx++] = (handle->address >> 24) & 0xFF;
        packet[idx++] = (handle->address >> 16) & 0xFF;
        packet[idx++] = (handle->address >> 8) & 0xFF;
        packet[idx++] = handle->address & 0xFF;

        packet[idx++] = is_last_packet ? R307_END_DATA_PACKET : R307_DATA_PACKET;

        uint16_t data_pkg_len = chunk_len + 2;
        packet[idx++] = (data_pkg_len >> 8) & 0xFF;
        packet[idx++] = data_pkg_len & 0xFF;

        memcpy(&packet[idx], &template_data[bytes_sent], chunk_len);
        idx += chunk_len;

        uint8_t pkg_type = is_last_packet ? R307_END_DATA_PACKET : R307_DATA_PACKET;
        checksum = pkg_type + (data_pkg_len >> 8) + (data_pkg_len & 0xFF);
        for (uint16_t i = 0; i < chunk_len; i++) {
            checksum += template_data[bytes_sent + i];
        }

        packet[idx++] = (checksum >> 8) & 0xFF;
        packet[idx++] = checksum & 0xFF;

        // Send via DMA
        char pkt_debug[64];
        sprintf(pkt_debug, "[R307] Sending pkt %d (%d bytes)...\r\n", packet_num, chunk_len);
        HAL_UART_Transmit(&huart2, (uint8_t*)pkt_debug, strlen(pkt_debug), 100);

        if (HAL_UART_Transmit_DMA(handle->huart, packet, idx) != HAL_OK) {
            HAL_UART_Transmit(&huart2, (uint8_t*)"[R307] Data TX fail\r\n", 21, 100);
            R307_FlushUART(handle);
            return false;
        }

        // Wait for TX to complete
        start_tick = HAL_GetTick();
        while (handle->huart->gState != HAL_UART_STATE_READY) {
            if ((HAL_GetTick() - start_tick) > 1000) {
                R307_FlushUART(handle);
                return false;
            }
            HAL_Delay(5);
        }

        bytes_sent += chunk_len;
        packet_num++;
        HAL_Delay(40);  // Increased inter-packet delay
    }

    HAL_Delay(250);
    R307_FlushUART(handle);

    HAL_UART_Transmit(&huart2, (uint8_t*)"[R307] ✅ Download complete!\r\n", 33, 100);

    return true;
}
