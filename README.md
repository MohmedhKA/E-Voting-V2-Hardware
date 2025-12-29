# Blockchain-Based Voting System

# 🗳️ Blockchain E-Voting Terminal - Firmware

> **Hardware voting terminal firmware for secure blockchain-based electronic voting system**  
> Built with STM32L432KC microcontroller and ESP32 WiFi bridge over 11 months of solo development.

![Terminal](https://img.shields.io/badge/Platform-STM32L-blue) ![WiFi](https://img.shields.io/badge/WiFi-ESP32-green) ![Biometric](https://img.shields.io/badge/Biometric-R307-orange)

***

## Project Overview
This project implements a blockchain-based voting system designed to ensure secure, transparent, and tamper-proof elections. The system leverages the STM32 microcontroller and ESP32 for hardware integration, along with custom-built libraries to manage the voting process and blockchain operations.


## 🏗️ Hardware Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    STM32L432KC (Main MCU)                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Keypad     │  │   LCD I2C    │  │   R307 FP    │      │
│  │   Input      │  │   Display    │  │   Sensor     │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│                            │ UART (115200 baud)              │
│                            ↓                                 │
└────────────────────────────┼─────────────────────────────────┘
                             │
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                ESP32-WROOM-32 (WiFi Bridge)                 │
│                                                             │
│  ┌────────────────┐         ┌────────────────┐             │
│  │  HTTP Client   │ ←────→  │   TLS/SSL      │             │
│  │  JSON Parser   │         │   Encryption   │             │
│  └────────────────┘         └────────────────┘             │
│                            │                                 │
│                            │ WiFi/HTTPS                      │
│                            ↓                                 │
└────────────────────────────┼─────────────────────────────────┘
                             │
                             ↓
                    ┌────────────────┐
                    │  Backend API   │
                    │  (Node.js)     │
                    └────────┬───────┘
                             │
                             ↓
                    ┌────────────────┐
                    │  Hyperledger   │
                    │  Fabric        │
                    └────────────────┘
```

***

## ✨ Features

### Biometric Authentication
- **R307 Fingerprint Sensor** integration via UART
- Template enrollment with 512-byte capacity
- Real-time matching with adjustable confidence scores
- Support for 1:1 verification mode

### Secure Communication
- **ESP32 WiFi Bridge** handles all network operations
- HTTPS support with TLS 1.2+
- Certificate validation (configurable)
- Command-response protocol via UART
- Automatic retry logic with exponential backoff

### User Interface
- **16x2 LCD Display** with I2C interface (PCF8574)
- **4x4 Matrix Keypad** for Aadhaar/VoterID entry
- Scrolling candidate list navigation
- Real-time status updates and error messages
- Loading animations during network operations

### Blockchain Integration
- Direct API calls to Hyperledger Fabric backend
- Vote submission with cryptographic proofs
- Receipt polling with transaction IDs
- Automatic session management
- Email receipt delivery

### Security Features
- **SHA256 hashing** for voter identity
- Fingerprint-based authentication proof
- OTP verification via email
- Auth token-based session management
- No local vote storage (direct blockchain submission)

***

## 🛠️ Hardware Requirements

| Component | Model/Spec | Quantity | Purpose |
|-----------|------------|----------|---------|
| **Microcontroller** | STM32L432KC (NUCLEO-32) | 1 | Main voting logic controller |
| **WiFi Module** | ESP32-WROOM-32 DevKit | 1 | Network communication bridge |
| **Fingerprint Sensor** | R307 Optical Sensor | 1 | Biometric authentication |
| **LCD Display** | 16x2 with I2C (PCF8574) | 1 | User interface display |
| **Keypad** | 4x4 Matrix Membrane | 1 | Data entry (Aadhaar/VoterID) |
| **Power Supply** | 5V 2A adapter | 1 | System power |
| **Jumper Wires** | Male-to-Female | 20+ | Connections |

### Optional Components
- Breadboard or custom PCB
- Enclosure for terminal housing
- Buzzer for audio feedback
- Status LEDs

***

## 🔌 Pin Connections

### STM32L432KC Pinout

#### UART1 - R307 Fingerprint Sensor
```
PA9  (TX) → R307 RX (Yellow)
PA10 (RX) → R307 TX (White)
         → R307 VCC (Red) → 5V
         → R307 GND (Black) → GND
```

#### UART2 - ESP32 Communication Bridge
```
PA2 (TX) → ESP32 RX (GPIO16)
PA3 (RX) → ESP32 TX (GPIO17)
```

#### I2C1 - LCD Display (PCF8574)
```
PB6 (SCL) → LCD SCL
PB7 (SDA) → LCD SDA
          → LCD VCC → 5V
          → LCD GND → GND
```

#### GPIO - 4x4 Keypad Matrix
```
Rows:
PB0 → Row 1
PB1 → Row 2
PA8 → Row 3
PA11 → Row 4

Columns:
PA0 → Col 1
PA1 → Col 2
PA4 → Col 3
PA5 → Col 4
```

### ESP32-WROOM-32 Pinout

```
GPIO16 (RX2) → STM32 UART2 TX (PA2)
GPIO17 (TX2) → STM32 UART2 RX (PA3)
GND          → STM32 GND (common ground)
```

**⚠️ Note:** ESP32 and STM32 must share common ground. ESP32 runs on 3.3V logic, STM32 UART pins are 5V tolerant.

***

## Folder Structure
```
Hardware/
├── BlockchainVotingMX_STM32/
│   ├── Core/
│   │   ├── Inc/                # Header files for STM32
│   │   ├── Src/                # Source files for STM32
│   │   └── Startup/            # Startup assembly files
│   ├── Drivers/                # STM32 HAL drivers
│   ├── Debug/                  # Debugging files
│   └── BlockchainVotingMX.ioc  # STM32CubeMX configuration file
├── Evoting_ESP32/
│   └── Evoting.ino             # Arduino sketch for ESP32
├── LICENSE                     # License file
└── README.md                   # Project documentation
```

***

## 📱 Usage

### First-Time Setup

1. **Power on the terminal**
2. **Enroll fingerprints** (admin mode - if implemented)
3. **Connect to WiFi** - ESP32 auto-connects on boot
4. **Verify backend connection** - Check LCD for status messages

### Voting Flow

```
┌─────────────────────┐
│  1. SELECT ELECTION │ ← Navigate with keypad (↑/↓/Enter)
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  2. ENTER AADHAAR   │ ← 12-digit Aadhaar number
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  3. ENTER VOTER ID  │ ← Alphanumeric Voter ID
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  4. SCAN FINGERPRINT│ ← Place finger on R307
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  5. VERIFY IDENTITY │ ← Backend validates with stored template
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  6. SEND OTP        │ ← OTP sent to registered email
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  7. ENTER OTP       │ ← 6-digit OTP code
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  8. VERIFY OTP      │ ← Backend validates OTP
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  9. SELECT CANDIDATE│ ← Scroll through candidate list
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  10. CONFIRM VOTE   │ ← Press # to confirm, * to cancel
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  11. CAST VOTE      │ ← Vote submitted to blockchain
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  12. WAIT RECEIPT   │ ← Polling blockchain for confirmation
└──────────┬──────────┘
           ↓
┌─────────────────────┐
│  13. SHOW RECEIPT   │ ← Display TX ID and success message
└─────────────────────┘
```

### Keypad Controls

```
┌───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ A │  A = Up/Previous
├───┼───┼───┼───┤
│ 4 │ 5 │ 6 │ B │  B = Down/Next
├───┼───┼───┼───┤
│ 7 │ 8 │ 9 │ C │  C = Clear/Cancel
├───┼───┼───┼───┤
│ * │ 0 │ # │ D │  # = Enter/Confirm
└───┴───┴───┴───┘  * = Back/Cancel
                    D = Menu (reserved)
```

***

## Software Components
- **Custom Libraries**:
  - `esp32_bridge`: Manages communication between STM32 and ESP32.
  - `keypad`: Handles keypad input.
  - `sha256`: Provides cryptographic hashing.
  - `r307`: Interfaces with the biometric sensor.
- **Blockchain Implementation**: Ensures secure and immutable vote storage.

## Setup Instructions
1. **STM32 Setup**:
   - Open the `BlockchainVotingMX.ioc` file in STM32CubeMX.
   - Generate the code and compile it using STM32CubeIDE.
   - Flash the firmware onto the STM32 microcontroller.

2. **ESP32 Setup**:
   - Open the `Evoting.ino` file in the Arduino IDE.
   - Install the required libraries for ESP32.
   - Upload the sketch to the ESP32 board.

3. **Hardware Connections**:
   - Connect the STM32 and ESP32 using UART.
   - Attach the keypad and biometric sensor to the STM32.

4. **Run the System**:
   - Power on the hardware.
   - Follow the on-screen instructions to cast votes.

***

## ⭐ Star History

If this project helped you or inspired your work, please give it a ⭐!

***

## 📊 Project Stats

- **Lines of Code:** ~3000+ (STM32) + ~800+ (ESP32)
- **Development Time:** 11 months
- **Languages:** C, C++
- **Hardware Platforms:** STM32L4, ESP32
- **Communication Protocols:** UART, I2C, HTTP/HTTPS
- **Security:** SHA256, TLS, Fingerprint biometrics

***

**Built with ❤️ and countless cups of coffee ☕**

*"From concept to completion, every line of code tells a story of persistence."*

## Contributing
Contributions are welcome! Please fork the repository and submit a pull request with your changes.

## License
This project is licensed under the terms of the [LICENSE](LICENSE) file.

## Acknowledgments
- Special thanks to the open-source community for providing tools and libraries that made this project possible.

---