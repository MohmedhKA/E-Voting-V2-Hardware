/*******************************************************************************
  * @file           r307.h
  * @brief          R307 Optical Fingerprint Sensor Driver for STM32L4
  * @author         E-Voting Terminal Project
  * @date           December 2025 (UART Buffer Fix Edition)
  * @note           Compatible with R305/R306/R307/R30X series
  ******************************************************************************/

#ifndef R307_H
#define R307_H

#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* R307 Package Identifiers */
#define R307_COMMAND_PACKET      0x01
#define R307_DATA_PACKET         0x02
#define R307_ACK_PACKET          0x07
#define R307_END_DATA_PACKET     0x08

/* R307 Commands */
#define R307_CMD_GEN_IMAGE       0x01  // Capture fingerprint image
#define R307_CMD_IMG_2TZ         0x02  // Generate character file from image
#define R307_CMD_MATCH           0x03  // Match two templates
#define R307_CMD_SEARCH          0x04  // Search fingerprint library
#define R307_CMD_REG_MODEL       0x05  // Combine character files into template
#define R307_CMD_STORE           0x06  // Store template
#define R307_CMD_LOAD            0x07  // Load template
#define R307_CMD_UP_CHAR         0x08  // Upload character file
#define R307_CMD_DOWN_CHAR       0x09  // Download character file
#define R307_CMD_UP_IMAGE        0x0A  // Upload image
#define R307_CMD_DOWN_IMAGE      0x0B  // Download image
#define R307_CMD_DELETE_CHAR     0x0C  // Delete templates
#define R307_CMD_EMPTY           0x0D  // Empty library
#define R307_CMD_VERIFY_PWD      0x13  // Verify password
#define R307_CMD_GET_RANDOM      0x14  // Get random number
#define R307_CMD_TEMPLATE_NUM    0x1D  // Get template count
#define R307_CMD_READ_SYS_PARA   0x0F  // Read system parameters

/* R307 Response Codes */
#define R307_OK                  0x00  // Success
#define R307_ERR_RECV            0x01  // Error receiving packet
#define R307_ERR_NO_FINGER       0x02  // No finger detected
#define R307_ERR_ENROLL_FAIL     0x03  // Failed to enroll finger
#define R307_ERR_DISORDERLY      0x06  // Failed to generate character file
#define R307_ERR_FEATURE_FAIL    0x07  // Failed to generate features
#define R307_ERR_NO_MATCH        0x08  // Fingers don't match
#define R307_ERR_NOT_FOUND       0x09  // No matching template found
#define R307_ERR_COMBINE_FAIL    0x0A  // Failed to combine templates
#define R307_ERR_BAD_LOCATION    0x0B  // Bad storage location
#define R307_ERR_FLASH_WRITE     0x18  // Error writing to flash
#define R307_ERR_INVALID_REG     0x1A  // Invalid register number
#define R307_ERR_TIMEOUT         0xFE  // Communication timeout
#define R307_ERR_COMM            0xFF  // Communication error

/* R307 Buffer IDs */
#define R307_BUFFER_1            0x01
#define R307_BUFFER_2            0x02

/* R307 Configuration */
#define R307_DEFAULT_ADDR        0xFFFFFFFF
#define R307_DEFAULT_PASSWORD    0x00000000
#define R307_START_CODE          0xEF01
#define R307_TIMEOUT_MS          1000
#define R307_MAX_PACKET_SIZE     256

/* R307 Handle Structure */
typedef struct {
    UART_HandleTypeDef *huart;
    uint32_t address;
    uint32_t password;
    uint16_t template_count;
    uint16_t library_size;
    uint8_t security_level;
    uint32_t baud_rate;
} R307_Handle;

/* Public Functions */
bool R307_Init(R307_Handle *dev, UART_HandleTypeDef *huart);
bool R307_VerifyPassword(R307_Handle *dev);
bool R307_GetImage(R307_Handle *dev);
bool R307_Image2Tz(R307_Handle *dev, uint8_t buffer_id);
bool R307_CreateModel(R307_Handle *dev);
bool R307_StoreModel(R307_Handle *dev, uint8_t buffer_id, uint16_t location);
bool R307_Search(R307_Handle *dev, uint8_t buffer_id, uint16_t *found_id, uint16_t *score);
bool R307_Match(R307_Handle *dev, uint16_t *score);
bool R307_DeleteModel(R307_Handle *dev, uint16_t id, uint16_t count);
bool R307_EmptyLibrary(R307_Handle *dev);
bool R307_GetTemplateCount(R307_Handle *dev, uint16_t *count);
bool R307_ReadSystemParameters(R307_Handle *dev);

/* High-Level Functions */
bool R307_CaptureAndSearch(R307_Handle *dev, uint16_t *finger_id, uint16_t *score);
bool R307_EnrollFingerprint(R307_Handle *dev, uint16_t location);
uint8_t R307_GetLastError(R307_Handle *dev);

/* Template Transfer Functions */
bool R307_UploadTemplate(R307_Handle *dev, uint8_t buffer_id, uint8_t *template_data);
bool R307_DownloadTemplate(R307_Handle *handle, uint8_t buffer_id, uint8_t *template_data);

/* 🔧 UART Buffer Management - THE FIX! */
void R307_FlushUART(R307_Handle *dev);

#endif /* R307_H */
