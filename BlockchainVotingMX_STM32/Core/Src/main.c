/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : E-VOTING TERMINAL - PRODUCTION READY v3.0
  * @author         : Blockchain Voting Team
  * @date           : December 2025
  * @note           : FULLY INTEGRATED WITH BACKEND + FINGERPRINT MATCHING
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "r307.h"
#include "esp32_bridge.h"
#include "keypad.h"
#include "sha256.h"  // ✅ SHA256 for hashing

/* 🔧 WiFi & Backend Configuration */
#define WIFI_SSID       "Redmi Note 10"       // ⚠️ CHANGE THIS!
#define WIFI_PASSWORD   "123456789"               // ⚠️ CHANGE THIS!
#define BACKEND_HOST    "automatic-space-garbanzo-x5wjwqpqqjgqhv4w9-3000.app.github.dev"            // ⚠️ YOUR PC IP!
#define BACKEND_PORT    443
#define TERMINAL_ID     "TERM_001"
#define API_KEY         "voter-secret-key-456"

/* API Endpoints */
#define API_BASE        "/api/v1/terminal"
#define API_ELECTIONS   "/api/v1/terminal/elections"
#define API_VERIFY      "/api/v1/terminal/verify-identity"
#define API_SEND_OTP    "/api/v1/terminal/send-otp"
#define API_VERIFY_OTP  "/api/v1/terminal/verify-otp"
#define API_CAST_VOTE   "/api/v1/terminal/cast-vote"
#define API_GET_RECEIPT "/api/v1/terminal/receipt"
#define API_SEND_EMAIL  "/api/v1/terminal/send-receipt-email"

/* Voting Flow States */
typedef enum {
    STATE_SELECT_ELECTION = 0,
    STATE_ENTER_AADHAAR,
    STATE_ENTER_VOTER_ID,
    STATE_SCAN_FINGERPRINT,
    STATE_VERIFY_IDENTITY,
    STATE_SEND_OTP,
    STATE_ENTER_OTP,
    STATE_VERIFY_OTP,
    STATE_SELECT_CANDIDATE,
    STATE_CONFIRM_VOTE,
    STATE_CAST_VOTE,
    STATE_WAIT_RECEIPT,
    STATE_SHOW_RECEIPT,
    STATE_COMPLETE
} VotingState;

/* Election Structure */
typedef struct {
    char id[32];
    char name[64];
    char description[128];
} Election;

/* Candidate Structure */
typedef struct {
    char id[32];
    char name[64];
    char party[64];
} Candidate;

/* Voting Session */
typedef struct {
    VotingState state;

    // Election Data
    Election elections[10];
    uint8_t election_count;
    uint8_t selected_election_idx;

    // Voter Data
    char aadhaar[13];
    char voter_id[16];
    uint8_t fingerprint_template[512];

    // Backend Response Data
    char voter_name[64];
    char masked_email[64];
    char auth_token[65];
    uint16_t match_score;

    // Candidate Data
    Candidate candidates[5];
    uint8_t candidate_count;
    uint8_t selected_candidate_idx;

    // OTP Data
    char otp[7];

    // Receipt Data
    char transaction_id[128];
    char aadhaar_hash[65];

    // State Control
    bool fingerprint_matched;
    bool otp_verified;
    uint8_t retry_count;
} VotingSession;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RNG_HandleTypeDef hrng;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
R307_Handle fingerprint;
ESP32_Handle esp32;
VotingSession session;

/* Large buffers for JSON */
char json_buffer[512];
char response_buffer[512];
char debug_buffer[256];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RNG_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
// Utility Functions
void Debug_Printf(const char *format, ...);
void LCD_Print(const char *format, ...);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void Reset_Session(void);
void SHA256_Hash_Hex(const char *input, char *output_hex);

// Voting Flow Functions
void State_SelectElection(void);
void State_EnterAadhaar(void);
void State_EnterVoterID(void);
void State_ScanFingerprint(void);
void State_VerifyIdentity(void);
void State_SendOTP(void);
void State_EnterOTP(void);
void State_VerifyOTP(void);
void State_SelectCandidate(void);
void State_ConfirmVote(void);
void State_CastVote(void);
void State_WaitReceipt(void);
void State_ShowReceipt(void);

// Backend API Functions
bool Backend_GetElections(void);
bool Backend_VerifyIdentity(void);
bool Backend_SendOTP(void);
bool Backend_VerifyOTP(void);
bool Backend_GetCandidates(void);
bool Backend_CastVote(void);
bool Backend_GetReceipt(void);
bool Backend_SendReceiptEmail(void);

// UI Helper Functions
bool Get_Number_Input(char *buffer, uint8_t max_len, const char *prompt);
bool Get_String_Input(char *buffer, uint8_t max_len, const char *prompt);
uint8_t Show_Scrolling_List(const char items[][64], uint8_t count, const char *title);
void Show_Loading(const char *message);
void Show_Error(const char *message);
void Show_Success(const char *message);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  Debug printf via UART2
  */
void Debug_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(debug_buffer, sizeof(debug_buffer), format, args);
    va_end(args);
    HAL_UART_Transmit(&huart2, (uint8_t*)debug_buffer, strlen(debug_buffer), 100);
}

/**
  * @brief  Send LCD Print command
  */
void LCD_Print(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    char cmd[150];
    snprintf(cmd, sizeof(cmd), "LCD_PRINT,%s\n", buffer);
    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 100);
    HAL_Delay(50);
}

/**
  * @brief  Clear LCD
  */
void LCD_Clear(void)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)"LCD_CLEAR\n", 10, 100);
    HAL_Delay(50);
}

/**
  * @brief  Set LCD cursor position
  */
void LCD_SetCursor(uint8_t row, uint8_t col)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "LCD_CURSOR,%d,%d\n", row, col);
    HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 100);
    HAL_Delay(30);
}

/**
  * @brief  SHA256 hash to hex string
  */
void SHA256_Hash_Hex(const char *input, char *output_hex)
{
    BYTE hash[SHA256_BLOCK_SIZE];
    sha256_hash((const BYTE*)input, strlen(input), hash);

    // Convert to hex string
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        sprintf(&output_hex[i * 2], "%02x", hash[i]);
    }
    output_hex[64] = '\0';
}

/**
  * @brief  Reset voting session
  */
void Reset_Session(void)
{
    memset(&session, 0, sizeof(VotingSession));
    session.state = STATE_SELECT_ELECTION;
    session.retry_count = 0;

    Debug_Printf("\r\n🔄 Session Reset\r\n\r\n");
}

/**
  * @brief  Get number input from keypad
  */
bool Get_Number_Input(char *buffer, uint8_t max_len, const char *prompt)
{
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print(prompt);
    LCD_SetCursor(1, 0);

    uint8_t pos = 0;
    char display[17] = {0};

    while (1) {
        char key = Keypad_GetKey();
        if (key != '\0') {
            Debug_Printf("Key: %c\r\n", key);

            if (key == '#') {
                // Confirm
                if (pos == max_len) {
                    buffer[pos] = '\0';
                    return true;
                } else {
                    Show_Error("Invalid length!");
                    return false;
                }
            } else if (key == '*') {
                // Backspace
                if (pos > 0) {
                    pos--;
                    display[pos] = '_';
                    buffer[pos] = '\0';
                    LCD_SetCursor(1, 0);
                    LCD_Print(display);
                }
            } else if (key >= '0' && key <= '9') {
                // Add digit
                if (pos < max_len) {
                    buffer[pos] = key;
                    display[pos] = key;
                    pos++;
                    LCD_SetCursor(1, 0);
                    LCD_Print(display);
                }
            }
        }
        HAL_Delay(100);
    }
}

/**
  * @brief  Get alphanumeric string input
  */
bool Get_String_Input(char *buffer, uint8_t max_len, const char *prompt)
{
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print(prompt);
    LCD_SetCursor(1, 0);

    uint8_t pos = 0;

    while (1) {
        char key = Keypad_GetKey();
        if (key != '\0') {
            if (key == '#') {
                // Confirm
                buffer[pos] = '\0';
                return (pos > 0);
            } else if (key == '*') {
                // Backspace
                if (pos > 0) {
                    pos--;
                    buffer[pos] = '\0';
                    LCD_SetCursor(1, 0);
                    LCD_Print("                ");
                    LCD_SetCursor(1, 0);
                    LCD_Print(buffer);
                }
            } else if (pos < max_len) {
                // Add character
                buffer[pos++] = key;
                LCD_SetCursor(1, 0);
                LCD_Print(buffer);
            }
        }
        HAL_Delay(100);
    }
}

/**
  * @brief  Show scrolling list (2=DOWN, 8=UP, #=SELECT, *=BACK)
  */
uint8_t Show_Scrolling_List(const char items[][64], uint8_t count, const char *title)
{
    uint8_t selected = 0;

    while (1) {
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print(title);

        // Show current item (truncate to 16 chars)
        char display[17];
        snprintf(display, 17, "%d.%s", selected + 1, items[selected]);
        LCD_SetCursor(1, 0);
        LCD_Print(display);

        Debug_Printf("Showing: [%d/%d] %s\r\n", selected + 1, count, items[selected]);

        char key = '\0';
        while (key == '\0') {
            key = Keypad_GetKey();
            HAL_Delay(50);
        }

        Debug_Printf("Key: %c\r\n", key);

        if (key == '2') {
            // DOWN
            selected = (selected + 1) % count;
        } else if (key == '8') {
            // UP
            selected = (selected == 0) ? count - 1 : selected - 1;
        } else if (key == '#') {
            // SELECT
            return selected;
        } else if (key == '*') {
            // BACK
            return 255;  // Signal to go back
        }
    }
}

/**
 * @brief HAL UART RX Complete Callback - Routes ESP32 data
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // ✅ Pass received byte to ESP32 handler
        extern uint8_t uart_rx_byte;
        extern ESP32_Handle esp32;

        ESP32_UART_RxCallback(&esp32, uart_rx_byte);

        // ✅ Restart reception for next byte
        HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);
    }
    else if (huart->Instance == USART1) {
        // R307 handling (if you have any)
    }
}

/**
  * @brief  Show loading screen
  */
void Show_Loading(const char *message)
{
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Loading...");
    LCD_SetCursor(1, 0);
    LCD_Print(message);
    Debug_Printf("⏳ %s\r\n", message);
}

/**
  * @brief  Show error message
  */
void Show_Error(const char *message)
{
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("ERROR!");
    LCD_SetCursor(1, 0);
    LCD_Print(message);
    Debug_Printf("❌ ERROR: %s\r\n", message);
    HAL_Delay(3000);
}

/**
  * @brief  Show success message
  */
void Show_Success(const char *message)
{
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("SUCCESS!");
    LCD_SetCursor(1, 0);
    LCD_Print(message);
    Debug_Printf("✅ SUCCESS: %s\r\n", message);
    ESP32_LED_Blink(&esp32, 3);
    HAL_Delay(2000);
}

/* ========================================================================== */
/* VOTING STATE MACHINE                                                        */
/* ========================================================================== */

/**
  * @brief  STATE 1: Select Election
  */
void State_SelectElection(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 1: SELECT ELECTION        \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    Show_Loading("Fetching...");

    if (!Backend_GetElections()) {
        Show_Error("Backend Failed!");
        HAL_Delay(2000);
        return;
    }

    if (session.election_count == 0) {
        Show_Error("No Elections!");
        HAL_Delay(2000);
        return;
    }

    // Prepare election list for scrolling
    static char election_names[10][64];
    for (uint8_t i = 0; i < session.election_count; i++) {
        strncpy(election_names[i], session.elections[i].name, 63);
    }

    uint8_t result = Show_Scrolling_List(election_names, session.election_count, "Select Election:");

    if (result == 255) {
        // User pressed * to go back - reset
        Reset_Session();
        return;
    }

    session.selected_election_idx = result;
    Debug_Printf("✅ Selected: %s\r\n", session.elections[result].name);
    /* Move to next state */
    session.state = STATE_ENTER_AADHAAR;
}

/**
  * @brief  STATE 2: Enter Aadhaar Number (12 digits)
  */
void State_EnterAadhaar(void)
{
	Debug_Printf("═══════════════════════════\r\n");
	Debug_Printf("STEP 2: ENTER AADHAAR\r\n");
	Debug_Printf("═══════════════════════════\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)"💬 [STM32] 🔹 Calling GetNumberInput...\r\n", 50, 100);
    if (Get_Number_Input(session.aadhaar, 12, "Enter Aadhaar:")) {
        Debug_Printf("✅ Aadhaar: %s\r\n", session.aadhaar);
        session.state = STATE_ENTER_VOTER_ID;
    } else {
        Show_Error("Invalid Aadhaar!");
    }
}

/**
  * @brief  STATE 3: Enter Voter ID
  */
void State_EnterVoterID(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 3: ENTER VOTER ID         \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    if (Get_String_Input(session.voter_id, 15, "Enter Voter ID:")) {
        Debug_Printf("✅ Voter ID: %s\r\n", session.voter_id);
        session.state = STATE_SCAN_FINGERPRINT;
    } else {
        Show_Error("Invalid ID!");
    }
}

/**
  * @brief  STATE 4: Scan Fingerprint
  */
void State_ScanFingerprint(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 4: SCAN FINGERPRINT       \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Place Finger");
    LCD_SetCursor(1, 0);
    LCD_Print("On Scanner...");

    HAL_Delay(2000);

    // Capture fingerprint
    if (!R307_GetImage(&fingerprint)) {
        Show_Error("No Finger!");
        session.retry_count++;
        if (session.retry_count > 3) {
            Reset_Session();
        }
        return;
    }

    Debug_Printf("✅ Image Captured\r\n");

    // Convert to template
    if (!R307_Image2Tz(&fingerprint, R307_BUFFER_1)) {
        Show_Error("Template Fail!");
        return;
    }

    Debug_Printf("✅ Template Generated\r\n");

    // Upload template to MCU
    LCD_SetCursor(1, 0);
    LCD_Print("Processing...  ");

    if (!R307_UploadTemplate(&fingerprint, R307_BUFFER_1, session.fingerprint_template)) {
        Show_Error("Upload Failed!");
        return;
    }

    Debug_Printf("✅ Template Uploaded (512 bytes)\r\n");
    Show_Success("Scanned!");

    session.state = STATE_VERIFY_IDENTITY;
}

void State_VerifyIdentity(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("  STEP 5: VERIFY IDENTITY  \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    Show_Loading("Verifying...");

    // Use backend upload method instead of DownChar
    if (Backend_VerifyIdentity()) {
        Debug_Printf("✅ Identity Verified!\r\n");
        Debug_Printf("   Name: %s\r\n", session.voter_name);
        Debug_Printf("   Match Score: %d%%\r\n", session.match_score);

        Show_Success("Identity OK!");
        session.state = STATE_SEND_OTP;
    } else {
        Show_Error("Verify Failed!");
        session.retry_count++;

        if (session.retry_count > 3) {
            Reset_Session();
        } else {
            session.state = STATE_SCAN_FINGERPRINT;
        }
    }
}


/**
  * @brief  STATE 6: Send OTP
  */
void State_SendOTP(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 6: SEND OTP               \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    Show_Loading("Sending OTP...");

    if (Backend_SendOTP()) {
        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_Print("OTP Sent!");
        LCD_SetCursor(1, 0);
        LCD_Print(session.masked_email);

        Debug_Printf("✅ OTP sent to: %s\r\n", session.masked_email);
        HAL_Delay(2000);

        session.state = STATE_ENTER_OTP;
    } else {
        Show_Error("OTP Send Failed!");
    }
}

/**
  * @brief  STATE 7: Enter OTP
  */
void State_EnterOTP(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 7: ENTER OTP              \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    if (Get_Number_Input(session.otp, 6, "Enter 6-Dig OTP:")) {
        Debug_Printf("OTP Entered: %s\r\n", session.otp);
        session.state = STATE_VERIFY_OTP;
    } else {
        Show_Error("Invalid OTP!");
    }
}

/**
 * @brief STATE 8: Verify OTP
 */
void State_VerifyOTP(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("  STEP 8: VERIFY OTP  \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    Show_Loading("Verifying...");

    if (Backend_VerifyOTP()) {
        Debug_Printf("✅ OTP Verified!\r\n");
        Debug_Printf("   Auth Token: %.16s...\r\n", session.auth_token);

        Show_Success("OTP Correct!");

        // ✅ IMPROVED: Fetch candidates from backend
        Debug_Printf("📋 Fetching candidates...\r\n");
        Show_Loading("Loading...");

        if (Backend_GetCandidates()) {
            Debug_Printf("✅ Got %d candidates:\r\n", session.candidate_count);

            // ✅ NEW: Display parsed candidates for verification
            for (uint8_t i = 0; i < session.candidate_count; i++) {
                Debug_Printf("   [%d] %s (%s)\r\n",
                            i + 1,
                            session.candidates[i].name,
                            session.candidates[i].party);
            }

            session.state = STATE_SELECT_CANDIDATE;
        } else {
            Show_Error("No Candidates!");
            Debug_Printf("❌ Failed to fetch candidates\r\n");
            HAL_Delay(2000);
            Reset_Session();
        }
    } else {
        Show_Error("Wrong OTP!");
        session.retry_count++;

        if (session.retry_count > 3) {
            Reset_Session();
        } else {
            session.state = STATE_ENTER_OTP;
        }
    }
}

/**
 * @brief STATE 9: Select Candidate
 */
void State_SelectCandidate(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("  STEP 9: SELECT CANDIDATE  \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    // ✅ FIX: Declare as non-static and initialize with zeros
    char candidate_names[10][64];
    memset(candidate_names, 0, sizeof(candidate_names));

    // Build display strings (Name - Party)
    for (uint8_t i = 0; i < session.candidate_count && i < 10; i++) {
        snprintf(candidate_names[i], 64, "%s - %s",
                 session.candidates[i].name,
                 session.candidates[i].party);

        Debug_Printf("  Display[%d]: %s\r\n", i, candidate_names[i]);
    }

    uint8_t result = Show_Scrolling_List(candidate_names, session.candidate_count, "Vote For:");

    if (result == 255) {
        // Back pressed
        Debug_Printf("❌ User went back\r\n");
        session.state = STATE_ENTER_OTP;
        return;
    }

    session.selected_candidate_idx = result;
    Debug_Printf("✅ Selected: %s\r\n", session.candidates[result].name);

    session.state = STATE_CONFIRM_VOTE;
}

/**
 * @brief Upload scanned fingerprint template to backend for matching (CHUNKED)
 * @retval true if match found
 */
bool Backend_VerifyIdentity(void)
{
    static HTTP_Response response;

    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("  BACKEND TEMPLATE MATCHING  \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    // Convert scanned template to hex string
    char template_hex[1025];
    for (int i = 0; i < 512; i++) {
        sprintf(&template_hex[i * 2], "%02X", session.fingerprint_template[i]);
    }
    template_hex[1024] = '\0';

    Debug_Printf("✅ Template converted to hex\r\n");

    // Generate unique chunk ID
    char chunk_id[64];
    sprintf(chunk_id, "%s_%s_%lu", session.aadhaar, session.voter_id, HAL_GetTick());

    // Upload in 4 chunks (256 chars each = 128 bytes)
    Debug_Printf("📤 Uploading template in 4 chunks...\r\n");

    for (int chunk = 0; chunk < 4; chunk++) {
        // Build JSON for this chunk
        snprintf(json_buffer, sizeof(json_buffer),
                 "{\"aadhaar\":\"%s\",\"voterId\":\"%s\",\"chunkId\":\"%s\",\"chunkNum\":%d,\"chunkData\":\"%.256s\"}",
                 session.aadhaar,
                 session.voter_id,
                 chunk_id,
                 chunk,
                 &template_hex[chunk * 256]);

        Debug_Printf("  Chunk %d/4... (%d bytes)\r\n", chunk + 1, strlen(json_buffer));

        if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT,
                             "/api/v1/terminal/upload-template-chunk",
                             json_buffer, &response)) {
            Debug_Printf("❌ Chunk %d upload failed!\r\n", chunk + 1);
            return false;
        }

        if (!response.success) {
            Debug_Printf("❌ Chunk %d rejected!\r\n", chunk + 1);
            return false;
        }

        HAL_Delay(100);
    }

    Debug_Printf("✅ All chunks uploaded!\r\n");

    // Request backend to match
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"aadhaar\":\"%s\",\"voterId\":\"%s\",\"chunkId\":\"%s\"}",
             session.aadhaar, session.voter_id, chunk_id);

    Debug_Printf("🔍 Requesting backend matching...\r\n");

    if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT,
                         "/api/v1/terminal/match-fingerprint",
                         json_buffer, &response)) {
        Debug_Printf("❌ Match request failed!\r\n");
        return false;
    }

    if (!response.success) {
        Debug_Printf("❌ HTTP %d\r\n", response.status_code);
        return false;
    }

    // Parse match result
    char *matched_marker = strstr(response.body, "\"matched\":");
    if (!matched_marker) {
        Debug_Printf("❌ Invalid response\r\n");
        return false;
    }

    bool matched = (strstr(matched_marker, "true") != NULL);

    if (!matched) {
        Debug_Printf("❌ No match found\r\n");
        return false;
    }

    // Extract match score
    char *score_marker = strstr(response.body, "\"score\":");
    if (score_marker) {
        score_marker += 8;
        session.match_score = (uint16_t)atoi(score_marker);
        Debug_Printf("📊 Match Score: %d%%\r\n", session.match_score);
    }

    // Extract voter info
    char *name_marker = strstr(response.body, "\"name\":\"");
    if (name_marker) {
        name_marker += 8;
        char *name_end = strchr(name_marker, '\"');
        if (name_end) {
            size_t name_len = name_end - name_marker;
            if (name_len < sizeof(session.voter_name)) {
                strncpy(session.voter_name, name_marker, name_len);
                session.voter_name[name_len] = '\0';
            }
        }
    }

    Debug_Printf("✅ MATCH FOUND!\r\n");
    Debug_Printf("   Name: %s\r\n", session.voter_name);

    session.fingerprint_matched = true;
    return true;
}

/**
 * @brief Fetch candidates for selected election
 * Handles BOTH string arrays and object arrays
 */
bool Backend_GetCandidates(void)
{
    static HTTP_Response response;

    snprintf(json_buffer, sizeof(json_buffer),
             "{\"electionId\":\"%s\",\"authToken\":\"%s\"}",
             session.elections[session.selected_election_idx].id,
             session.auth_token);

    Debug_Printf("📡 POST /api/v1/terminal/get-candidates\r\n");

    if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT,
                         "/api/v1/terminal/get-candidates",
                         json_buffer, &response)) {
        Debug_Printf("❌ HTTP POST failed!\r\n");
        return false;
    }

    if (!response.success) {
        Debug_Printf("❌ HTTP %d\r\n", response.status_code);
        return false;
    }

    Debug_Printf("📥 Response (%d bytes): %s\r\n", strlen(response.body), response.body);

    // ✅ RESET: Clear candidate array first
    memset(session.candidates, 0, sizeof(session.candidates));
    session.candidate_count = 0;

    // Find the "data" array
    char *data_array = strstr(response.body, "\"data\":[");
    if (!data_array) {
        Debug_Printf("❌ No 'data' array found\r\n");
        return false;
    }

    char *search_pos = data_array + 8; // Skip past '"data":['
    Debug_Printf("🔍 Starting parse...\r\n");

    // Parse each candidate object (max 10)
    while (session.candidate_count < 10 && *search_pos != '\0') {

        // Look for opening brace of next object
        char *obj_start = strchr(search_pos, '{');
        if (!obj_start || (obj_start - search_pos) > 50) {
            Debug_Printf("⏹ No more candidates (count: %d)\r\n", session.candidate_count);
            break;
        }

        search_pos = obj_start + 1; // Move past '{'

        // ✅ NEW: Look for "id" field (REQUIRED)
        char *id_marker = strstr(search_pos, "\"id\":\"");
        if (!id_marker || (id_marker - search_pos) > 200) {
            // Skip this object if no id found
            char *closing = strchr(search_pos, '}');
            if (closing) {
                search_pos = closing + 1;
            }
            continue;
        }

        // Extract ID
        char *id_start = id_marker + 6;
        char *id_end = strchr(id_start, '\"');
        if (id_end && (id_end - id_start) > 0 && (id_end - id_start) < 32) {
            strncpy(session.candidates[session.candidate_count].id, id_start, id_end - id_start);
            session.candidates[session.candidate_count].id[id_end - id_start] = '\0';
        } else {
            // Skip if ID extraction failed
            search_pos = id_end ? id_end : search_pos + 50;
            continue;
        }

        // ✅ NEW: Look for "name" field (REQUIRED)
        char *name_marker = strstr(id_end, "\"name\":\"");
        if (!name_marker || (name_marker - id_end) > 100) {
            Debug_Printf("⚠️ No 'name' in object, skipping\r\n");
            search_pos = strchr(id_end, '}');
            if (search_pos) search_pos++;
            continue;
        }

        // Extract Name
        char *name_start = name_marker + 8;
        char *name_end = strchr(name_start, '\"');
        if (name_end && (name_end - name_start) > 0 && (name_end - name_start) < 64) {
            strncpy(session.candidates[session.candidate_count].name, name_start, name_end - name_start);
            session.candidates[session.candidate_count].name[name_end - name_start] = '\0';
        } else {
            strcpy(session.candidates[session.candidate_count].name, "Unknown");
        }

        // Extract Party (OPTIONAL)
        char *party_marker = strstr(name_end, "\"party\":\"");
        if (party_marker && (party_marker - name_end) < 100) {
            char *party_start = party_marker + 9;
            char *party_end = strchr(party_start, '\"');
            if (party_end && (party_end - party_start) < 64) {
                strncpy(session.candidates[session.candidate_count].party, party_start, party_end - party_start);
                session.candidates[session.candidate_count].party[party_end - party_start] = '\0';
                search_pos = party_end;
            } else {
                strcpy(session.candidates[session.candidate_count].party, "Independent");
                search_pos = name_end;
            }
        } else {
            strcpy(session.candidates[session.candidate_count].party, "Independent");
            search_pos = name_end;
        }

        Debug_Printf("  ✅ [%d] ID: %s | Name: %s | Party: %s\r\n",
                     session.candidate_count + 1,
                     session.candidates[session.candidate_count].id,
                     session.candidates[session.candidate_count].name,
                     session.candidates[session.candidate_count].party);

        session.candidate_count++;

        // Move to next object
        char *closing_brace = strchr(search_pos, '}');
        if (closing_brace) {
            search_pos = closing_brace + 1;
        } else {
            break;
        }
    }

    if (session.candidate_count == 0) {
        Debug_Printf("❌ No candidates parsed\r\n");
        return false;
    }

    Debug_Printf("✅ Parsed %d candidates successfully\r\n", session.candidate_count);
    return true;
}

/**
  * @brief  STATE 10: Confirm Vote
  */
void State_ConfirmVote(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 10: CONFIRM VOTE          \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Confirm Vote?");

    char display[17];
    snprintf(display, 17, "%s", session.candidates[session.selected_candidate_idx].name);
    LCD_SetCursor(1, 0);
    LCD_Print(display);

    Debug_Printf("Press # to confirm, * to cancel\r\n");

    char key = '\0';
    while (key == '\0') {
        key = Keypad_GetKey();
        HAL_Delay(100);
    }

    if (key == '#') {
        Debug_Printf("✅ Vote Confirmed!\r\n");
        session.state = STATE_CAST_VOTE;
    } else if (key == '*') {
        Debug_Printf("Vote Cancelled\r\n");
        session.state = STATE_SELECT_CANDIDATE;
    }
}

/**
  * @brief  STATE 11: Cast Vote
  */
void State_CastVote(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 11: CASTING VOTE          \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    Show_Loading("Casting Vote...");

    if (Backend_CastVote()) {
        Debug_Printf("✅ Vote Cast Successfully!\r\n");
        Show_Success("Vote Cast!");
        session.state = STATE_WAIT_RECEIPT;
    } else {
        Show_Error("Vote Failed!");
        HAL_Delay(2000);
        Reset_Session();
    }
}

/**
  * @brief  STATE 12: Wait for Receipt
  */
void State_WaitReceipt(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 12: WAITING RECEIPT       \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    Show_Loading("Processing...");

    uint8_t attempts = 0;
    while (attempts < 10) {
        HAL_Delay(3000);  // Wait 3 seconds between polls

        if (Backend_GetReceipt()) {
            Debug_Printf("✅ Receipt Ready!\r\n");
            Debug_Printf("   TX ID: %s\r\n", session.transaction_id);

            session.state = STATE_SHOW_RECEIPT;
            return;
        }

        attempts++;
        Debug_Printf("Still processing... (%d/10)\r\n", attempts);
    }

    Show_Error("Receipt Timeout");
    session.state = STATE_COMPLETE;
}

/**
  * @brief  STATE 13: Show Receipt
  */
void State_ShowReceipt(void)
{
    Debug_Printf("\r\n══════════════════════════════════════\r\n");
    Debug_Printf("       STEP 13: SHOW RECEIPT          \r\n");
    Debug_Printf("══════════════════════════════════════\r\n");

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Vote Recorded!");
    LCD_SetCursor(1, 0);
    LCD_Print("Sending Email...");

    // Send receipt email
    if (Backend_SendReceiptEmail()) {
        Debug_Printf("✅ Receipt emailed!\r\n");

        LCD_SetCursor(1, 0);
        LCD_Print("Check Your Email");
    } else {
        Debug_Printf("⚠️ Email failed (receipt saved)\r\n");
        LCD_SetCursor(1, 0);
        LCD_Print("Email Failed   ");
    }

    HAL_Delay(3000);

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Thank You!");
    LCD_SetCursor(1, 0);
    LCD_Print("for Voting");

    Debug_Printf("\r\n🎉 VOTING COMPLETE!\r\n\r\n");
    ESP32_LED_Blink(&esp32, 5);
    HAL_Delay(3000);

    session.state = STATE_COMPLETE;
}

/* ========================================================================== */
/* BACKEND API FUNCTIONS                                                       */
/* ========================================================================== */

/**
  * @brief  Fetch active elections from backend
  */
/**
 * @brief Fetch active elections from backend
 */
/**
 * @brief Fetch active elections from backend
 */
bool Backend_GetElections(void)
{
    static HTTP_Response response;
    Debug_Printf("📡 GET %s\r\n", API_ELECTIONS);

    if (!ESP32_HTTP_GET(&esp32, BACKEND_HOST, BACKEND_PORT, API_ELECTIONS, &response)) {
        Debug_Printf("❌ HTTP GET Failed!\r\n");
        return false;
    }

    if (!response.success) {
        Debug_Printf("❌ HTTP %d\r\n", response.status_code);
        return false;
    }

    Debug_Printf("📥 Response: %s\r\n", response.body);

    // ✅ SIMPLE PARSER: Just extract election objects one by one
    // Format: {"success":true,"data":[{...},{...}]}

    session.election_count = 0;
    char *search_pos = response.body;

    // Find each election object in the data array
    while (session.election_count < 10) {
        // Look for "id":" pattern (start of each election)
        char *id_marker = strstr(search_pos, "\"id\":\"");
        if (!id_marker) break; // No more elections

        // Make sure this "id" is inside the data array (not success field)
        char *data_array = strstr(response.body, "\"data\":[");
        if (!data_array || id_marker < data_array) break;

        // Extract ID
        char *id_start = id_marker + 6; // Skip past "id":"
        char *id_end = strchr(id_start, '"');
        if (id_end) {
            size_t id_len = id_end - id_start;
            if (id_len < sizeof(session.elections[0].id)) {
                strncpy(session.elections[session.election_count].id, id_start, id_len);
                session.elections[session.election_count].id[id_len] = '\0';
            }
        }

        // Extract Title (look for "title":" after current position)
        char *title_marker = strstr(id_end, "\"title\":\"");
        if (title_marker && (title_marker - id_end) < 50) { // Make sure it's in same object
            char *title_start = title_marker + 9; // Skip past "title":"
            char *title_end = strchr(title_start, '"');
            if (title_end) {
                size_t title_len = title_end - title_start;
                if (title_len < sizeof(session.elections[0].name)) {
                    strncpy(session.elections[session.election_count].name, title_start, title_len);
                    session.elections[session.election_count].name[title_len] = '\0';
                }
            }

            Debug_Printf("  ✅ Election %d:\r\n", session.election_count + 1);
            Debug_Printf("     ID: %s\r\n", session.elections[session.election_count].id);
            Debug_Printf("     Name: %s\r\n", session.elections[session.election_count].name);

            session.election_count++;

            // Move search position past this election
            search_pos = title_end;
        } else {
            // No title found for this ID, skip it
            search_pos = id_end;
        }
    }

    if (session.election_count == 0) {
        Debug_Printf("❌ No elections parsed\r\n");
        return false;
    }

    Debug_Printf("✅ Total elections parsed: %d\r\n", session.election_count);
    return true;
}

/**
 * @brief Compare two fingerprint templates using Hamming Distance
 * @param template1: First 512-byte template
 * @param template2: Second 512-byte template
 * @retval Similarity score (0-100, higher = better match)
 */
uint8_t Compare_Fingerprint_Templates(uint8_t *template1, uint8_t *template2) {
    uint32_t matching_bytes = 0;
    uint32_t total_significant_bytes = 0;

    // Compare byte-by-byte
    for (int i = 0; i < 512; i++) {
        // Skip "empty" bytes (0x00 or 0xFF padding)
        if ((template1[i] == 0x00 && template2[i] == 0x00) ||
            (template1[i] == 0xFF && template2[i] == 0xFF)) {
            continue; // Ignore padding
        }

        total_significant_bytes++;

        // Calculate bit-level similarity
        uint8_t xor_result = template1[i] ^ template2[i];
        uint8_t bit_differences = 0;

        // Count differing bits
        for (int bit = 0; bit < 8; bit++) {
            if (xor_result & (1 << bit)) {
                bit_differences++;
            }
        }

        // If fewer than 3 bits differ, count as matching
        if (bit_differences <= 2) {
            matching_bytes++;
        }
    }

    // Calculate percentage similarity
    if (total_significant_bytes == 0) {
        return 0; // Invalid templates
    }

    uint8_t similarity = (matching_bytes * 100) / total_significant_bytes;
    return similarity;
}

/**
  * @brief  Send OTP to voter's email
  */
bool Backend_SendOTP(void)
{
    HTTP_Response response;

    snprintf(json_buffer, sizeof(json_buffer),
             "{\"aadhaar\":\"%s\",\"voterId\":\"%s\"}",
             session.aadhaar, session.voter_id);

    Debug_Printf("📡 POST %s\r\n", API_SEND_OTP);

    if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT, API_SEND_OTP, json_buffer, &response)) {
        return false;
    }

    return response.success;
}

/**
  * @brief  Verify OTP and get auth token
  */
bool Backend_VerifyOTP(void)
{
    HTTP_Response response;

    snprintf(json_buffer, sizeof(json_buffer),
             "{\"aadhaar\":\"%s\",\"voterId\":\"%s\",\"otp\":\"%s\"}",
             session.aadhaar, session.voter_id, session.otp);

    Debug_Printf("📡 POST %s\r\n", API_VERIFY_OTP);

    if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT, API_VERIFY_OTP, json_buffer, &response)) {
        return false;
    }

    if (!response.success) {
        return false;
    }

    // Extract auth token and stored template
    if (JSON_GetString(response.body, "authToken", session.auth_token, sizeof(session.auth_token))) {
        return true;
    }
    return false;
}

/**
  * @brief  Cast vote
  */
bool Backend_CastVote(void)
{
    HTTP_Response response;

    char temp_fp_hex[1025];
    for (int i = 0; i < 512; i++) {
        sprintf(&temp_fp_hex[i * 2], "%02X", session.fingerprint_template[i]);
    }
    temp_fp_hex[1024] = '\0';

    // Create fingerprint match hash
    SHA256_Hash_Hex(temp_fp_hex, session.aadhaar_hash);
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"authToken\":\"%s\",\"electionId\":\"%s\",\"candidateId\":\"%s\",\"fingerprintMatchHash\":\"%s\"}",
             session.auth_token,
             session.elections[session.selected_election_idx].id,
             session.candidates[session.selected_candidate_idx].id,
             session.aadhaar_hash);

    Debug_Printf("📡 POST %s\r\n", API_CAST_VOTE);

    if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT, API_CAST_VOTE, json_buffer, &response)) {
        return false;
    }

    return response.success;
}

/**
 * @brief Get receipt (poll endpoint)
 */
bool Backend_GetReceipt(void)
{
    static HTTP_Response response;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s/%s",
             API_GET_RECEIPT,
             session.elections[session.selected_election_idx].id,
             session.aadhaar_hash);

    Debug_Printf("📡 GET %s\r\n", path);

    if (!ESP32_HTTP_GET(&esp32, BACKEND_HOST, BACKEND_PORT, path, &response)) {
        return false;
    }

    if (!response.success) {
        return false;
    }

    Debug_Printf("📥 Receipt response: %s\r\n", response.body);

    // ✅ FIX: Check for "processing":false or "processing":true
    char *processing_marker = strstr(response.body, "\"processing\":");
    if (processing_marker) {
        processing_marker += 13; // Skip past '"processing":'

        // Skip whitespace
        while (*processing_marker == ' ') processing_marker++;

        // Check if it's "false" (receipt ready)
        if (strncmp(processing_marker, "false", 5) == 0) {
            Debug_Printf("✅ Receipt ready!\r\n");

            // Extract transaction ID
            char *txid_marker = strstr(response.body, "\"txId\":\"");
            if (txid_marker) {
                txid_marker += 8; // Skip past '"txId":"'
                char *txid_end = strchr(txid_marker, '\"');

                if (txid_end) {
                    size_t txid_len = txid_end - txid_marker;
                    if (txid_len < sizeof(session.transaction_id)) {
                        strncpy(session.transaction_id, txid_marker, txid_len);
                        session.transaction_id[txid_len] = '\0';

                        Debug_Printf("📜 TX ID: %s\r\n", session.transaction_id);
                        return true;
                    }
                }
            }

            // If we got here, processing is false but no txId (shouldn't happen)
            Debug_Printf("⚠️ Processing complete but no transaction ID found\r\n");
            return false;

        } else if (strncmp(processing_marker, "true", 4) == 0) {
            Debug_Printf("⏳ Still processing on blockchain...\r\n");
            return false;
        }
    }

    Debug_Printf("⚠️ Could not parse processing status\r\n");
    return false;
}


/**
  * @brief  Send receipt via email
  */
bool Backend_SendReceiptEmail(void)
{
    HTTP_Response response;

    snprintf(json_buffer, sizeof(json_buffer),
             "{\"aadhaar\":\"%s\",\"voterId\":\"%s\",\"transactionId\":\"%s\",\"electionId\":\"%s\"}",
             session.aadhaar, session.voter_id, session.transaction_id,
             session.elections[session.selected_election_idx].id);

    Debug_Printf("📡 POST %s\r\n", API_SEND_EMAIL);

    if (!ESP32_HTTP_POST(&esp32, BACKEND_HOST, BACKEND_PORT, API_SEND_EMAIL, json_buffer, &response)) {
        return false;
    }

    return response.success;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();   // ← R307 UART
  MX_USART2_UART_Init();   // ← ESP3
  MX_RNG_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
    HAL_Delay(3000);  // Wait for ESP32 boot
    uint32_t stack_ptr = __get_MSP();

    Debug_Printf("\r\n=== MEMORY DEBUG ===\r\n");
    Debug_Printf("Stack Pointer: 0x%08X\r\n", stack_ptr);
    Debug_Printf("session addr:  0x%08X (size: %d)\r\n", (uint32_t)&session, sizeof(session));
    Debug_Printf("json_buffer:   0x%08X\r\n", (uint32_t)&json_buffer);
    Debug_Printf("Free RAM estimate: %d bytes\r\n\r\n", stack_ptr - (uint32_t)&json_buffer);

    Debug_Printf("\r\n\r\n");
    Debug_Printf("╔══════════════════════════════════════════════════════╗\r\n");
    Debug_Printf("║  BLOCKCHAIN E-VOTING TERMINAL v3.0                  ║\r\n");
    Debug_Printf("║  Production Ready - Backend Integrated               ║\r\n");
    Debug_Printf("╚══════════════════════════════════════════════════════╝\r\n\r\n");

    // Initialize ESP32
    Debug_Printf("📡 Initializing ESP32...\r\n");
    if (!ESP32_Init(&esp32, &huart2)) {
        Debug_Printf("❌ ESP32 init failed!\r\n");
        while(1) HAL_Delay(1000);
    }
    Debug_Printf("✅ ESP32 OK!\r\n\r\n");

    // Initialize LCD
    Debug_Printf("🖥️ Initializing LCD...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)"LCD_INIT\n", 9, 100);
    HAL_Delay(500);
    Debug_Printf("✅ LCD Ready!\r\n\r\n");

    // Initialize Keypad
    Debug_Printf("⌨️ Initializing Keypad...\r\n");
    Keypad_Init();
    Debug_Printf("✅ Keypad Ready!\r\n\r\n");

    // Initialize R307
    Debug_Printf("🔍 Initializing R307 Fingerprint...\r\n");
    if (!R307_Init(&fingerprint, &huart1)) {
        Debug_Printf("⚠️ R307 not detected\r\n\r\n");
    } else {
        Debug_Printf("✅ R307 Ready!\r\n\r\n");
    }

    // Connect WiFi
    Debug_Printf("📶 Connecting to WiFi: %s\r\n", WIFI_SSID);
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print("Connecting WiFi");

    if (!ESP32_ConnectWiFi(&esp32, WIFI_SSID, WIFI_PASSWORD)) {
        Debug_Printf("❌ WiFi connection failed!\r\n");
        Show_Error("WiFi Failed!");
        while(1) HAL_Delay(1000);
    }

    Debug_Printf("✅ WiFi connected!\r\n");
    char ip[16];
    if (ESP32_GetIP(&esp32, ip)) {
        Debug_Printf("🌐 IP Address: %s\r\n\r\n", ip);
    }

    Show_Success("System Ready!");

    // Initialize voting session
    Reset_Session();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
		switch (session.state) {
				case STATE_SELECT_ELECTION:
					State_SelectElection();
					Debug_Printf("Checking Transition...\r\n");
					break;

				case STATE_ENTER_AADHAAR:
					State_EnterAadhaar();  // ← Just this, no debug, no delays!
					break;

				case STATE_ENTER_VOTER_ID:
					State_EnterVoterID();
					break;

				case STATE_SCAN_FINGERPRINT:
					State_ScanFingerprint();
					break;

				case STATE_VERIFY_IDENTITY:
					State_VerifyIdentity();
					break;

				case STATE_SEND_OTP:
					State_SendOTP();
					break;

				case STATE_ENTER_OTP:
					State_EnterOTP();
					break;

				case STATE_VERIFY_OTP:
					State_VerifyOTP();
					break;

				case STATE_SELECT_CANDIDATE:
					State_SelectCandidate();
					break;

				case STATE_CONFIRM_VOTE:
					State_ConfirmVote();
					break;

				case STATE_CAST_VOTE:
					State_CastVote();
					break;

				case STATE_WAIT_RECEIPT:
					State_WaitReceipt();
					break;

				case STATE_SHOW_RECEIPT:
					State_ShowReceipt();
					break;

				case STATE_COMPLETE:
					Reset_Session();
					break;

				default:
					Reset_Session();
					break;
			}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/**
  * @brief RNG Initialization Function
  * @param None
  * @retval None
  */
static void MX_RNG_Init(void)
{

  /* USER CODE BEGIN RNG_Init 0 */
  /* USER CODE END RNG_Init 0 */

  /* USER CODE BEGIN RNG_Init 1 */
  /* USER CODE END RNG_Init 1 */
  hrng.Instance = RNG;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RNG_Init 2 */
  /* USER CODE END RNG_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 57600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */
  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */
  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 PA4 PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB3 PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
