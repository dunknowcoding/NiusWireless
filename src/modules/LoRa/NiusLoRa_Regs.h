/*
 * NiusLoRa_Regs.h — Register maps and constants for SX127x and SX126x LoRa chips
 *
 * SX127x family:  SX1276, SX1277, SX1278  (RFM95W/96W/98W, RA-01, RA-02)
 * SX126x family:  SX1261, SX1262, SX1268
 */

#ifndef NIUS_LORA_REGS_H
#define NIUS_LORA_REGS_H

/* =======================================================================
 * SX127x Register Map  (SX1276 / SX1277 / SX1278)
 * SPI read:  send 0x00|addr, read response
 * SPI write: send 0x80|addr, send value
 * ====================================================================== */

/* --- Page 0 (LoRa mode) ----------------------------------------------- */
#define SX127X_REG_FIFO               0x00
#define SX127X_REG_OP_MODE            0x01
#define SX127X_REG_FR_MSB             0x06
#define SX127X_REG_FR_MID             0x07
#define SX127X_REG_FR_LSB             0x08
#define SX127X_REG_PA_CONFIG          0x09
#define SX127X_REG_PA_RAMP            0x0A
#define SX127X_REG_OCP                0x0B
#define SX127X_REG_LNA                0x0C
#define SX127X_REG_FIFO_ADDR_PTR      0x0D
#define SX127X_REG_FIFO_TX_BASE       0x0E
#define SX127X_REG_FIFO_RX_BASE       0x0F
#define SX127X_REG_FIFO_RX_CURRENT    0x10
#define SX127X_REG_IRQ_FLAGS_MASK     0x11
#define SX127X_REG_IRQ_FLAGS          0x12
#define SX127X_REG_RX_NB_BYTES        0x13
#define SX127X_REG_PKT_SNR_VALUE      0x19
#define SX127X_REG_PKT_RSSI_VALUE     0x1A
#define SX127X_REG_RSSI_VALUE         0x1B
#define SX127X_REG_HOP_CHANNEL        0x1C
#define SX127X_REG_MODEM_CONFIG1      0x1D
#define SX127X_REG_MODEM_CONFIG2      0x1E
#define SX127X_REG_SYMB_TIMEOUT_LSB   0x1F
#define SX127X_REG_PREAMBLE_MSB       0x20
#define SX127X_REG_PREAMBLE_LSB       0x21
#define SX127X_REG_PAYLOAD_LENGTH     0x22
#define SX127X_REG_MAX_PAYLOAD        0x23
#define SX127X_REG_HOP_PERIOD         0x24
#define SX127X_REG_FIFO_RX_BYTE_ADDR 0x25
#define SX127X_REG_MODEM_CONFIG3      0x26
#define SX127X_REG_SYNC_WORD          0x39
#define SX127X_REG_DIO_MAPPING1       0x40
#define SX127X_REG_DIO_MAPPING2       0x41
#define SX127X_REG_VERSION            0x42
#define SX127X_REG_PA_DAC             0x4D

/* --- SX127x OpMode values --------------------------------------------- */
#define SX127X_OPMODE_LORA            0x80  // bit 7: LoRa mode
#define SX127X_OPMODE_SLEEP           0x00
#define SX127X_OPMODE_STDBY           0x01
#define SX127X_OPMODE_FSTX            0x02
#define SX127X_OPMODE_TX              0x03
#define SX127X_OPMODE_FSRX            0x04
#define SX127X_OPMODE_RX_CONT         0x05
#define SX127X_OPMODE_RX_SINGLE       0x06
#define SX127X_OPMODE_CAD             0x07

/* Low-frequency mode (SX1278, used for bands < 600 MHz) */
#define SX127X_OPMODE_LF              0x08

/* --- SX127x IRQ flags (RegIrqFlags 0x12) ------------------------------ */
#define SX127X_IRQ_CAD_DETECTED       0x01
#define SX127X_IRQ_FHSS_CHANGE_CH    0x02
#define SX127X_IRQ_CAD_DONE           0x04
#define SX127X_IRQ_TX_DONE            0x08
#define SX127X_IRQ_VALID_HEADER       0x10
#define SX127X_IRQ_PAYLOAD_CRC_ERR   0x20
#define SX127X_IRQ_RX_DONE            0x40
#define SX127X_IRQ_RX_TIMEOUT         0x80

/* --- SX127x PA config ------------------------------------------------- */
#define SX127X_PA_SELECT_BOOST        0x80  // PA_BOOST pin (20 dBm max)
#define SX127X_PA_SELECT_RFO          0x00  // RFO pin (14 dBm max)
#define SX127X_PA_DAC_20DBM           0x87  // Enable +20 dBm mode
#define SX127X_PA_DAC_DEFAULT         0x84

/* --- SX127x known version values -------------------------------------- */
#define SX1276_VERSION                0x12
#define SX1278_VERSION                0x12   // Same value, different chip

/* --- SX127x bandwidth codes for ModemConfig1 ------------------------- */
// BW[7:4] in RegModemConfig1
#define SX127X_BW_7K8                 0x00
#define SX127X_BW_10K4                0x10
#define SX127X_BW_15K6                0x20
#define SX127X_BW_20K8                0x30
#define SX127X_BW_31K25               0x40
#define SX127X_BW_41K7                0x50
#define SX127X_BW_62K5                0x60
#define SX127X_BW_125K                0x70
#define SX127X_BW_250K                0x80
#define SX127X_BW_500K                0x90  // Only SX1276 (HF bands)

/* --- SX127x sync word ------------------------------------------------- */
#define SX127X_SYNC_PRIVATE           0x12  // Private / custom networks
#define SX127X_SYNC_LORAWAN           0x34  // LoRaWAN public network

/* =======================================================================
 * SX126x Command / Register Map  (SX1261 / SX1262 / SX1268)
 *
 * Unlike SX127x, the SX126x uses a command-based SPI interface:
 *   CS low → send command byte → send/receive payload bytes → CS high
 * The BUSY pin must be LOW before any command is sent.
 * ====================================================================== */

/* --- SX126x commands -------------------------------------------------- */
#define SX126X_CMD_SET_SLEEP              0x84
#define SX126X_CMD_SET_STANDBY            0x80
#define SX126X_CMD_SET_FS                 0xC1
#define SX126X_CMD_SET_TX                 0x83
#define SX126X_CMD_SET_RX                 0x82
#define SX126X_CMD_STOP_TIMER_ON_PREAMBLE 0x9F
#define SX126X_CMD_SET_RX_DUTY_CYCLE      0x94
#define SX126X_CMD_SET_CAD                0xC5
#define SX126X_CMD_SET_TX_CONTINUOUS_WAVE 0xD1
#define SX126X_CMD_SET_TX_INF_PREAMBLE    0xD2
#define SX126X_CMD_SET_REGULATOR_MODE     0x96
#define SX126X_CMD_CALIBRATE              0x89
#define SX126X_CMD_CALIBRATE_IMAGE        0x98
#define SX126X_CMD_SET_PA_CONFIG          0x95
#define SX126X_CMD_SET_RX_TX_FALLBACK     0x93
#define SX126X_CMD_WRITE_REGISTER         0x0D
#define SX126X_CMD_READ_REGISTER          0x1D
#define SX126X_CMD_WRITE_BUFFER           0x0E
#define SX126X_CMD_READ_BUFFER            0x1E
#define SX126X_CMD_SET_DIO_IRQ_PARAMS     0x08
#define SX126X_CMD_GET_IRQ_STATUS         0x12
#define SX126X_CMD_CLEAR_IRQ_STATUS       0x02
#define SX126X_CMD_SET_DIO2_AS_RF_SWITCH  0x9D
#define SX126X_CMD_SET_DIO3_AS_TCXO_CTRL 0x97
#define SX126X_CMD_SET_RF_FREQUENCY       0x86
#define SX126X_CMD_SET_PACKET_TYPE        0x8A
#define SX126X_CMD_GET_PACKET_TYPE        0x11
#define SX126X_CMD_SET_TX_PARAMS          0x8E
#define SX126X_CMD_SET_MODULATION_PARAMS  0x8B
#define SX126X_CMD_SET_PACKET_PARAMS      0x8C
#define SX126X_CMD_SET_CAD_PARAMS         0x88
#define SX126X_CMD_SET_BUFFER_BASE_ADDR   0x8F
#define SX126X_CMD_SET_LORA_SYMB_TIMEOUT  0xA0
#define SX126X_CMD_GET_STATUS             0xC0
#define SX126X_CMD_GET_RX_BUFFER_STATUS   0x13
#define SX126X_CMD_GET_PACKET_STATUS      0x14
#define SX126X_CMD_GET_RSSI_INST          0x15
#define SX126X_CMD_GET_STATS              0x10
#define SX126X_CMD_RESET_STATS            0x00
#define SX126X_CMD_GET_DEVICE_ERRORS      0x17
#define SX126X_CMD_CLEAR_DEVICE_ERRORS    0x07

/* --- SX126x register addresses ---------------------------------------- */
#define SX126X_REG_WHITENING_MSB          0x06B8
#define SX126X_REG_WHITENING_LSB          0x06B9
#define SX126X_REG_CRC_MSB                0x06BC
#define SX126X_REG_CRC_LSB                0x06BD
#define SX126X_REG_SYNC_WORD0             0x06C0
#define SX126X_REG_SYNC_WORD1             0x06C1
#define SX126X_REG_SYNC_WORD2             0x06C2
#define SX126X_REG_SYNC_WORD3             0x06C3
#define SX126X_REG_SYNC_WORD4             0x06C4
#define SX126X_REG_SYNC_WORD5             0x06C5
#define SX126X_REG_SYNC_WORD6             0x06C6
#define SX126X_REG_SYNC_WORD7             0x06C7
#define SX126X_REG_NODE_ADDR              0x06CD
#define SX126X_REG_BROADCAST_ADDR         0x06CE
#define SX126X_REG_IQ_POLARITY            0x0736
#define SX126X_REG_LORA_SYNC_WORD_MSB     0x0740  // Two bytes for LoRa sync
#define SX126X_REG_LORA_SYNC_WORD_LSB     0x0741
#define SX126X_REG_TX_MODULATION          0x0889
#define SX126X_REG_RX_GAIN                0x08AC
#define SX126X_REG_TX_CLAMP               0x08D8
#define SX126X_REG_OCP                    0x08E7
#define SX126X_REG_RTC_CTRL               0x0902
#define SX126X_REG_XTA_TRIM               0x0911
#define SX126X_REG_XTB_TRIM               0x0912
#define SX126X_REG_DIO3_OUT_VOLTAGE       0x0920
#define SX126X_REG_EVENT_MASK             0x0944

/* --- SX126x packet types ---------------------------------------------- */
#define SX126X_PACKET_TYPE_GFSK           0x00
#define SX126X_PACKET_TYPE_LORA           0x01

/* --- SX126x LoRa sync word -------------------------------------------- */
#define SX126X_SYNC_PRIVATE               0x1424  // Private network
#define SX126X_SYNC_LORAWAN               0x3444  // LoRaWAN public network

/* --- SX126x IRQ flags ------------------------------------------------- */
#define SX126X_IRQ_TX_DONE                0x0001
#define SX126X_IRQ_RX_DONE                0x0002
#define SX126X_IRQ_PREAMBLE_DETECTED      0x0004
#define SX126X_IRQ_SYNC_WORD_VALID        0x0008
#define SX126X_IRQ_HEADER_VALID           0x0010
#define SX126X_IRQ_HEADER_ERROR           0x0020
#define SX126X_IRQ_CRC_ERROR              0x0040
#define SX126X_IRQ_CAD_DONE               0x0080
#define SX126X_IRQ_CAD_DETECTED           0x0100
#define SX126X_IRQ_TIMEOUT                0x0200
#define SX126X_IRQ_ALL                    0x03FF

/* --- SX126x standby modes --------------------------------------------- */
#define SX126X_STANDBY_RC                 0x00  // 13 MHz RC oscillator
#define SX126X_STANDBY_XOSC              0x01  // Crystal oscillator

/* --- SX126x regulator modes ------------------------------------------- */
#define SX126X_REGULATOR_LDO              0x00
#define SX126X_REGULATOR_DCDC             0x01

/* --- SX126x PA config for SX1262 -------------------------------------- */
// SetPaConfig(paDutyCycle, hpMax, deviceSel, paLut)
#define SX1262_PA_DUTY_CYCLE_14DBM       0x02
#define SX1262_PA_DUTY_CYCLE_17DBM       0x02
#define SX1262_PA_DUTY_CYCLE_20DBM       0x04
#define SX1262_PA_DUTY_CYCLE_22DBM       0x04
#define SX1262_PA_HP_MAX_14DBM           0x02
#define SX1262_PA_HP_MAX_17DBM           0x05
#define SX1262_PA_HP_MAX_20DBM           0x03
#define SX1262_PA_HP_MAX_22DBM           0x07
#define SX1262_DEVICE_SEL                0x00  // 0=SX1262, 1=SX1261
#define SX126X_PA_LUT_DEFAULT            0x01

#endif // NIUS_LORA_REGS_H
