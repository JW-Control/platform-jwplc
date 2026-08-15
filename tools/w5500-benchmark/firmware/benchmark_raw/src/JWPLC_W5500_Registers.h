#ifndef JWPLC_W5500_REGISTERS_H
#define JWPLC_W5500_REGISTERS_H

#include <stdint.h>

// W5500 SPI Phase/Polarity: Mode 0 or Mode 3
// W5500 supports maximum 80 MHz SPI.

// Control Byte Macros
#define W5500_VDM  0x00 // Data phase size: Variable (00)
#define W5500_FDM1 0x01 // Data phase size: 1 byte (01)
#define W5500_FDM2 0x02 // Data phase size: 2 bytes (10)
#define W5500_FDM4 0x03 // Data phase size: 4 bytes (11)

#define W5500_RWB_READ  0x00
#define W5500_RWB_WRITE 0x04

// Block Select (BSB)
#define W5500_COMMON_REG 0x00

// Socket n Register Block
#define W5500_Sn_REG(n)  ((n * 4) + 1)
// Socket n TX Buffer Block
#define W5500_Sn_TX(n)   ((n * 4) + 2)
// Socket n RX Buffer Block
#define W5500_Sn_RX(n)   ((n * 4) + 3)

// Common Registers
#define W5500_MR      0x0000 // Mode Register
#define W5500_GAR     0x0001 // Gateway Address (4 bytes)
#define W5500_SUBR    0x0005 // Subnet Mask (4 bytes)
#define W5500_SHAR    0x0009 // Source Hardware Address (6 bytes)
#define W5500_SIPR    0x000F // Source IP Address (4 bytes)
#define W5500_INTLEVEL 0x0013 // Interrupt Low Level Timer (2 bytes)
#define W5500_IR      0x0015 // Interrupt Register
#define W5500_IMR     0x0016 // Interrupt Mask Register
#define W5500_SIR     0x0017 // Socket Interrupt
#define W5500_SIMR    0x0018 // Socket Interrupt Mask
#define W5500_RTR     0x0019 // Retry Time-value
#define W5500_RCR     0x001B // Retry Count
#define W5500_PTIMER  0x001C // PPP LCP Request Timer
#define W5500_PMAGIC  0x001D // PPP LCP Magic Number
#define W5500_PHAR    0x001E // Destination Hardware Address for PPPoE
#define W5500_PSID    0x0024 // Session ID for PPPoE
#define W5500_PMRU    0x0026 // Maximum Receive Unit for PPPoE
#define W5500_UIPR    0x0028 // Unreachable IP
#define W5500_UPORT   0x002C // Unreachable Port
#define W5500_PHYCFGR 0x002E // PHY Configuration Register
#define W5500_VERSIONR 0x0039 // Chip Version Register (Expected 0x04)

// Socket Registers (n = 0 to 7)
#define W5500_Sn_MR     0x0000 // Socket Mode
#define W5500_Sn_MR_TCP 0x01
#define W5500_Sn_MR_UDP 0x02
#define W5500_Sn_MR_MACRAW 0x04
#define W5500_Sn_CR     0x0001 // Socket Command
#define W5500_Sn_IR     0x0002 // Socket Interrupt
#define W5500_Sn_SR     0x0003 // Socket Status
#define W5500_Sn_PORT   0x0004 // Socket Source Port
#define W5500_Sn_DHAR   0x0006 // Socket Destination Hardware Address
#define W5500_Sn_DIPR   0x000C // Socket Destination IP Address
#define W5500_Sn_DPORT  0x0010 // Socket Destination Port
#define W5500_Sn_MSSR   0x0012 // Socket Maximum Segment Size
#define W5500_Sn_TOS    0x0015 // Socket IP TOS
#define W5500_Sn_TTL    0x0016 // Socket IP TTL
#define W5500_Sn_RXBUF_SIZE 0x001E // Socket RX Buffer Size
#define W5500_Sn_TXBUF_SIZE 0x001F // Socket TX Buffer Size
#define W5500_Sn_TX_FSR 0x0020 // Socket TX Free Size (2 bytes)
#define W5500_Sn_TX_RD  0x0022 // Socket TX Read Pointer (2 bytes)
#define W5500_Sn_TX_WR  0x0024 // Socket TX Write Pointer (2 bytes)
#define W5500_Sn_RX_RSR 0x0026 // Socket RX Received Size (2 bytes)
#define W5500_Sn_RX_RD  0x0028 // Socket RX Read Pointer (2 bytes)
#define W5500_Sn_RX_WR  0x002A // Socket RX Write Pointer (2 bytes)
#define W5500_Sn_IMR    0x002C // Socket Interrupt Mask
#define W5500_Sn_FRAG   0x002D // Socket Fragment Offset in IPv4
#define W5500_Sn_KPALVTR 0x002F // Socket Keep Alive Timer

// Socket Status (Sn_SR) values
#define W5500_SOCK_CLOSED      0x00
#define W5500_SOCK_INIT        0x13
#define W5500_SOCK_LISTEN      0x14
#define W5500_SOCK_ESTABLISHED 0x17
#define W5500_SOCK_CLOSE_WAIT  0x1C
#define W5500_SOCK_UDP         0x22
#define W5500_SOCK_MACRAW      0x42

// Socket Commands (Sn_CR)
#define W5500_CR_OPEN      0x01
#define W5500_CR_LISTEN    0x02
#define W5500_CR_CONNECT   0x04
#define W5500_CR_DISCON    0x08
#define W5500_CR_CLOSE     0x10
#define W5500_CR_SEND      0x20
#define W5500_CR_SEND_MAC  0x21
#define W5500_CR_SEND_KEEP 0x22
#define W5500_CR_RECV      0x40

// MR Register values
#define W5500_MR_RST 0x80

#endif // JWPLC_W5500_REGISTERS_H
