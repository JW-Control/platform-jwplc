#ifndef _HardwareSerialSC_H_
#define _HardwareSerialSC_H_

#include <stdint.h>
#include "HardwareSerial.h"
#include "Arduino.h"

//General Registers
#define SC16IS750_REG_RHR		(0x00)
#define SC16IS750_REG_THR		(0X00)
#define SC16IS750_REG_IER		(0X01)
#define SC16IS750_REG_FCR		(0X02)
#define SC16IS750_REG_IIR		(0X02)
#define SC16IS750_REG_LCR		(0X03)
#define SC16IS750_REG_MCR		(0X04)
#define SC16IS750_REG_LSR		(0X05)
#define SC16IS750_REG_MSR		(0X06)
#define SC16IS750_REG_SPR		(0X07)
#define SC16IS750_REG_TCR		(0X06)
#define SC16IS750_REG_TLR		(0X07)
#define SC16IS750_REG_TXLVL		(0X08)
#define SC16IS750_REG_RXLVL		(0X09)
#define SC16IS750_REG_IODIR		(0X0A)
#define SC16IS750_REG_IOSTATE	(0X0B)
#define SC16IS750_REG_IOINTENA	(0X0C)
#define SC16IS750_REG_IOCONTROL	(0X0E)
#define SC16IS750_REG_EFCR		(0X0F)

//Special Registers
#define SC16IS750_REG_DLL		(0x00)
#define SC16IS750_REG_DLH		(0X01)

//Enhanced Registers
#define SC16IS750_REG_EFR		(0X02)
#define SC16IS750_REG_XON1		(0X04)
#define SC16IS750_REG_XON2		(0X05)
#define SC16IS750_REG_XOFF1		(0X06)
#define SC16IS750_REG_XOFF2		(0X07)

//
#define SC16IS750_INT_CTS		(0X80)
#define SC16IS750_INT_RTS		(0X40)
#define SC16IS750_INT_XOFF		(0X20)
#define SC16IS750_INT_SLEEP		(0X10)
#define SC16IS750_INT_MODEM		(0X08)
#define SC16IS750_INT_LINE		(0X04)
#define SC16IS750_INT_THR		(0X02)
#define SC16IS750_INT_RHR		(0X01)

#define SC16IS750_CRYSTCAL_FREQ	(14745600UL)

class SC16IS752HardwareSerial : public HardwareSerial {
	public:
		SC16IS752HardwareSerial(uint32_t cs, uint8_t channel);

	public:
		virtual void begin(unsigned long baud, uint32_t config = SERIAL_8N1);
		virtual void end();

		using HardwareSerial::write;
		virtual size_t write(uint8_t value);
		virtual int available();
		virtual int availableForWrite();
		virtual int read();
		virtual int peek();
		virtual void flush();

	private:
		int16_t SetBaudrate(uint32_t baudrate);
		uint8_t ReadRegister(uint8_t reg_addr);
		void WriteRegister(uint8_t reg_addr, uint8_t val);
		void SetLine(uint8_t data_length, uint8_t parity_select, uint8_t stop_length);
		void ResetDevice();

		void FIFOEnable(uint8_t fifo_enable);
		void FIFOReset(uint8_t rx_fifo);
		void FIFOSetTriggerLevel(uint8_t rx_fifo, uint8_t length);
		uint8_t FIFOAvailableData();
		uint8_t FIFOAvailableSpace();
		void WriteByte(uint8_t val);
		int ReadByte();
		void EnableTransmit(uint8_t tx_enable);

	private:
		spi_t* spiBus;
		uint32_t cs;
		uint8_t channel;
		int peek_buf;
		uint8_t peek_flag;
};

#if defined(HAVE_SERIAL_SC_0)
extern SC16IS752HardwareSerial SerialSC0;
extern SC16IS752HardwareSerial Serial2;
#endif
#if defined(HAVE_SERIAL_SC_1)
extern SC16IS752HardwareSerial SerialSC1;
#endif

#endif
