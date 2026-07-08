/*
 * NiusMFRC522_Reg.h — MFRC522 register map, command codes and constants
 *
 * All addresses are 6-bit values (bits [6:1] of the SPI address byte).
 * The library shifts them automatically; you do not need to shift here.
 */

#ifndef NIUS_MFRC522_REG_H
#define NIUS_MFRC522_REG_H

/* -----------------------------------------------------------------------
 * Page 0 — Command and status
 * ---------------------------------------------------------------------- */
#define MFRC522_REG_COMMAND        0x01
#define MFRC522_REG_COM_I_EN       0x02
#define MFRC522_REG_DIV_I_EN       0x03
#define MFRC522_REG_COM_IRQ        0x04
#define MFRC522_REG_DIV_IRQ        0x05
#define MFRC522_REG_ERROR          0x06
#define MFRC522_REG_STATUS1        0x07
#define MFRC522_REG_STATUS2        0x08
#define MFRC522_REG_FIFO_DATA      0x09
#define MFRC522_REG_FIFO_LEVEL     0x0A
#define MFRC522_REG_WATER_LEVEL    0x0B
#define MFRC522_REG_CONTROL        0x0C
#define MFRC522_REG_BIT_FRAMING    0x0D
#define MFRC522_REG_COLL           0x0E

/* -----------------------------------------------------------------------
 * Page 1 — Communication settings
 * ---------------------------------------------------------------------- */
#define MFRC522_REG_MODE           0x11
#define MFRC522_REG_TX_MODE        0x12
#define MFRC522_REG_RX_MODE        0x13
#define MFRC522_REG_TX_CONTROL     0x14
#define MFRC522_REG_TX_ASK         0x15
#define MFRC522_REG_TX_SEL         0x16
#define MFRC522_REG_RX_SEL         0x17
#define MFRC522_REG_RX_THRESHOLD   0x18
#define MFRC522_REG_DEMOD          0x19
#define MFRC522_REG_MF_TX          0x1C
#define MFRC522_REG_MF_RX          0x1D
#define MFRC522_REG_SERIAL_SPEED   0x1F

/* -----------------------------------------------------------------------
 * Page 2 — Configuration
 * ---------------------------------------------------------------------- */
#define MFRC522_REG_CRC_RESULT_H   0x21
#define MFRC522_REG_CRC_RESULT_L   0x22
#define MFRC522_REG_MOD_WIDTH      0x24
#define MFRC522_REG_RF_CFG         0x26
#define MFRC522_REG_GS_N           0x27
#define MFRC522_REG_CWG_SP         0x28
#define MFRC522_REG_MOD_GS_P       0x29
#define MFRC522_REG_T_MODE         0x2A
#define MFRC522_REG_T_PRESCALER    0x2B
#define MFRC522_REG_T_RELOAD_H     0x2C
#define MFRC522_REG_T_RELOAD_L     0x2D
#define MFRC522_REG_T_COUNTER_H    0x2E
#define MFRC522_REG_T_COUNTER_L    0x2F

/* -----------------------------------------------------------------------
 * Page 3 — Test
 * ---------------------------------------------------------------------- */
#define MFRC522_REG_TEST_SEL1      0x31
#define MFRC522_REG_TEST_SEL2      0x32
#define MFRC522_REG_TEST_PIN_EN    0x33
#define MFRC522_REG_TEST_PIN_VAL   0x34
#define MFRC522_REG_TEST_BUS       0x35
#define MFRC522_REG_AUTO_TEST      0x36
#define MFRC522_REG_VERSION        0x37
#define MFRC522_REG_ANALOG_TEST    0x38
#define MFRC522_REG_TEST_DAC1      0x39
#define MFRC522_REG_TEST_DAC2      0x3A
#define MFRC522_REG_TEST_ADC       0x3B

/* -----------------------------------------------------------------------
 * MFRC522 internal command codes (write to MFRC522_REG_COMMAND)
 * ---------------------------------------------------------------------- */
#define MFRC522_CMD_IDLE           0x00
#define MFRC522_CMD_MEM            0x01
#define MFRC522_CMD_GEN_RANDOM_ID  0x02
#define MFRC522_CMD_CALC_CRC       0x03
#define MFRC522_CMD_TRANSMIT       0x04
#define MFRC522_CMD_NO_CMD_CHANGE  0x07
#define MFRC522_CMD_RECEIVE        0x08
#define MFRC522_CMD_TRANSCEIVE     0x0C
#define MFRC522_CMD_MF_AUTHENT     0x0E
#define MFRC522_CMD_SOFT_RESET     0x0F

/* -----------------------------------------------------------------------
 * ISO 14443A / MIFARE commands (sent to the card via Transceive)
 * ---------------------------------------------------------------------- */
#define MIFARE_CMD_REQA            0x26  // Request type A (new cards only)
#define MIFARE_CMD_WUPA            0x52  // Wake-up type A (also HALT'd cards)
#define MIFARE_CASCADE_1           0x93  // Anti-collision / select cascade L1
#define MIFARE_CASCADE_2           0x95  // Anti-collision / select cascade L2
#define MIFARE_CASCADE_3           0x97  // Anti-collision / select cascade L3
#define MIFARE_CMD_HALT_MSB        0x50
#define MIFARE_CMD_HALT_LSB        0x00
#define MIFARE_CT                  0x88  // Cascade tag in UID

/* MIFARE Classic commands */
#define MIFARE_AUTH_KEY_A          0x60
#define MIFARE_AUTH_KEY_B          0x61
#define MIFARE_CMD_READ            0x30
#define MIFARE_CMD_WRITE           0xA0
#define MIFARE_CMD_DECREMENT       0xC0
#define MIFARE_CMD_INCREMENT       0xC1
#define MIFARE_CMD_RESTORE         0xC2
#define MIFARE_CMD_TRANSFER        0xB0

/* MIFARE Ultralight / NTAG commands (no auth, page-addressed) */
#define MIFARE_CMD_WRITE_UL        0xA2  // WRITE — 4 bytes to a single page
#define MIFARE_CMD_COMP_WRITE      0xA0  // Compatibility Write (NTAG)
#define MIFARE_CMD_AUTH_UL         0x1A  // PWD_AUTH — NTAG password auth
#define MIFARE_CMD_GET_VERSION     0x60  // GET_VERSION — NTAG / Ultralight EV1 version info

/* MIFARE ACK / NAK */
#define MIFARE_ACK                 0x0A

/* -----------------------------------------------------------------------
 * RFCfgReg gain presets  (upper nibble of the register byte)
 * ---------------------------------------------------------------------- */
#define MFRC522_GAIN_18DB          0x00
#define MFRC522_GAIN_23DB          0x10
#define MFRC522_GAIN_33DB          0x40
#define MFRC522_GAIN_38DB          0x50
#define MFRC522_GAIN_43DB          0x60
#define MFRC522_GAIN_48DB          0x70  // Maximum gain

/* -----------------------------------------------------------------------
 * VersionReg expected values
 * ---------------------------------------------------------------------- */
#define MFRC522_VERSION_1_0        0x91
#define MFRC522_VERSION_2_0        0x92
#define MFRC522_VERSION_FM17522    0x88  // Fudan clone
#define MFRC522_VERSION_FM17522E   0x89  // Fudan clone variant

#endif // NIUS_MFRC522_REG_H
