#include "HardwareSerialSC.h"

#if defined(HAVE_SERIAL_SC_0)
SC16IS752HardwareSerial SerialSC0(SERIAL_SC_0_CS, 0);
SC16IS752HardwareSerial Serial2(SERIAL_SC_0_CS, 0);
#endif
#if defined(HAVE_SERIAL_SC_1)
SC16IS752HardwareSerial SerialSC1(SERIAL_SC_1_CS, 1);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
SC16IS752HardwareSerial::SC16IS752HardwareSerial(uint32_t cs, uint8_t channel) : spiBus(nullptr), cs(cs), channel(channel), peek_flag(0) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::begin(unsigned long baud, uint32_t config) {
	pinMode(cs, OUTPUT);
	digitalWrite(cs, HIGH);

	if (!spiBus) {
		spiBus = spiStartBus(VSPI, SPI_CLOCK_DIV4, SPI_MODE0, SPI_MSBFIRST);
		if (!spiBus) {
			return;
		}

		spiAttachSCK(spiBus, SCK);
		spiAttachMISO(spiBus, MISO);
		spiAttachMOSI(spiBus, MOSI);
	}

	ResetDevice();
	FIFOEnable(1);
	SetBaudrate(baud);

	switch (config) {
		case SERIAL_5N1:
			SetLine(5, 0, 1);
			break;
		case SERIAL_6N1:
			SetLine(6, 0, 1);
			break;
		case SERIAL_7N1:
			SetLine(7, 0, 1);
			break;
		case SERIAL_5N2:
			SetLine(5, 0, 2);
			break;
		case SERIAL_6N2:
			SetLine(6, 0, 2);
			break;
		case SERIAL_7N2:
			SetLine(7, 0, 2);
			break;
		case SERIAL_8N2:
			SetLine(8, 0, 2);
			break;
		case SERIAL_5E1:
			SetLine(5, 2, 1);
			break;
		case SERIAL_6E1:
			SetLine(6, 2, 1);
			break;
		case SERIAL_7E1:
			SetLine(7, 2, 1);
			break;
		case SERIAL_8E1:
			SetLine(8, 2, 1);
			break;
		case SERIAL_5E2:
			SetLine(5, 2, 2);
			break;
		case SERIAL_6E2:
			SetLine(6, 2, 2);
			break;
		case SERIAL_7E2:
			SetLine(7, 2, 2);
			break;
		case SERIAL_8E2:
			SetLine(8, 2, 2);
			break;
		case SERIAL_5O1:
			SetLine(5, 1, 1);
			break;
		case SERIAL_6O1:
			SetLine(6, 1, 1);
			break;
		case SERIAL_7O1:
			SetLine(7, 1, 1);
			break;
		case SERIAL_8O1:
			SetLine(8, 1, 1);
			break;
		case SERIAL_5O2:
			SetLine(5, 1, 2);
			break;
		case SERIAL_6O2:
			SetLine(6, 1, 2);
			break;
		case SERIAL_7O2:
			SetLine(7, 1, 2);
			break;
		case SERIAL_8O2:
			SetLine(8, 1, 2);
			break;
		case SERIAL_8N1:
		default:
			SetLine(8, 0, 1);
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::end() {

}

////////////////////////////////////////////////////////////////////////////////////////////////////
int SC16IS752HardwareSerial::available() {
	return FIFOAvailableData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int SC16IS752HardwareSerial::availableForWrite() {
	return FIFOAvailableSpace();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int SC16IS752HardwareSerial::read() {
	if (peek_flag == 0) {
		return ReadByte();
	}

	peek_flag = 0;
	return peek_buf;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
size_t SC16IS752HardwareSerial::write(uint8_t val) {
	WriteByte(val);
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t SC16IS752HardwareSerial::ReadRegister(uint8_t reg_addr) {
	if (!spiBus) {
		return 0;
	}

	digitalWrite(cs, LOW);
	delayMicroseconds(10);
	spiTransaction(spiBus, SPI_CLOCK_DIV4, SPI_MODE0, SPI_MSBFIRST);
	spiTransferByteNL(spiBus, 0x80 | ((reg_addr << 3) | (channel << 1)));
	uint8_t result = spiTransferByteNL(spiBus, 0xff);
	spiEndTransaction(spiBus);
	delayMicroseconds(10);
	digitalWrite(cs, HIGH);

	return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::WriteRegister(uint8_t reg_addr, uint8_t val) {
	if (!spiBus) {
		return;
	}

	digitalWrite(cs, LOW);
	delayMicroseconds(10);
	spiTransaction(spiBus, SPI_CLOCK_DIV4, SPI_MODE0, SPI_MSBFIRST);
	spiTransferByteNL(spiBus, (reg_addr << 3) | (channel << 1));
	spiTransferByteNL(spiBus, val);
	spiEndTransaction(spiBus);
	delayMicroseconds(10);
	digitalWrite(cs, HIGH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int16_t SC16IS752HardwareSerial::SetBaudrate(uint32_t baudrate) {
	uint8_t prescaler;
	if ((ReadRegister(SC16IS750_REG_MCR) & 0x80) == 0) {
		prescaler = 1;
	} else {
		prescaler = 4;
	}

	uint16_t divisor = (SC16IS750_CRYSTCAL_FREQ / prescaler) / (baudrate * 16);
	uint8_t temp_lcr = ReadRegister(SC16IS750_REG_LCR);
	temp_lcr |= 0x80;

	WriteRegister(SC16IS750_REG_LCR, temp_lcr);
	WriteRegister(SC16IS750_REG_DLL, (uint8_t)divisor);
	WriteRegister(SC16IS750_REG_DLH, (uint8_t)(divisor >> 8));

	temp_lcr &= 0x7F;

	WriteRegister(SC16IS750_REG_LCR, temp_lcr);

	uint32_t actual_baudrate = (SC16IS750_CRYSTCAL_FREQ / prescaler) / (16 * divisor);
	int16_t error = ((float)actual_baudrate - baudrate) * 1000 / baudrate;

	return error;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::SetLine(uint8_t data_length, uint8_t parity_select, uint8_t stop_length ) {
	uint8_t temp_lcr = ReadRegister(SC16IS750_REG_LCR);

	temp_lcr &= 0xC0;
	switch (data_length) {
		case 5:
			break;
		case 6:
			temp_lcr |= 0x01;
			break;
		case 7:
			temp_lcr |= 0x02;
			break;
		case 8:
			temp_lcr |= 0x03;
			break;
		default:
			temp_lcr |= 0x03;
			break;
	}

	if (stop_length == 2) {
		temp_lcr |= 0x04;
	}

	switch (parity_select) {
		case 0:						//no parity
			break;
		case 1:						//odd parity
			temp_lcr |= 0x08;
			break;
		case 2:						//even parity
			temp_lcr |= 0x18;
			break;
		case 3:						//force '1' parity
			temp_lcr |= 0x03;
			break;
		case 4:						//force '0' parity
			break;
		default:
			break;
	}

	WriteRegister(SC16IS750_REG_LCR, temp_lcr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::ResetDevice() {
	uint8_t origChannel = channel;

	channel = 0;
	uint8_t reg = ReadRegister(SC16IS750_REG_IOCONTROL);
	reg |= 0x08;
	WriteRegister(SC16IS750_REG_IOCONTROL, reg);

	channel = origChannel;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::FIFOEnable(uint8_t fifo_enable) {
	uint8_t temp_fcr = ReadRegister(SC16IS750_REG_FCR);

	if (fifo_enable == 0){
		temp_fcr &= 0xFE;
	} else {
		temp_fcr |= 0x01;
	}

	WriteRegister(SC16IS750_REG_FCR, temp_fcr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::FIFOReset(uint8_t rx_fifo) {
	uint8_t temp_fcr = ReadRegister(SC16IS750_REG_FCR);

	if (rx_fifo == 0){
		temp_fcr |= 0x04;
	} else {
		temp_fcr |= 0x02;
	}

	WriteRegister(SC16IS750_REG_FCR, temp_fcr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::FIFOSetTriggerLevel(uint8_t rx_fifo, uint8_t length) {
	uint8_t temp_reg = ReadRegister(SC16IS750_REG_MCR);
	temp_reg |= 0x04;
	WriteRegister(SC16IS750_REG_MCR, temp_reg); //SET MCR[2] to '1' to use TLR register or trigger level control in FCR register

	temp_reg = ReadRegister(SC16IS750_REG_EFR);
	WriteRegister(SC16IS750_REG_EFR, temp_reg | 0x10); //set ERF[4] to '1' to use the  enhanced features
	if (rx_fifo == 0) {
		WriteRegister(SC16IS750_REG_TLR, length << 4); //Tx FIFO trigger level setting
	} else {
		WriteRegister(SC16IS750_REG_TLR, length);	//Rx FIFO Trigger level setting
	}
	WriteRegister(SC16IS750_REG_EFR, temp_reg); //restore EFR register
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t SC16IS752HardwareSerial::FIFOAvailableData() {
	return ReadRegister(SC16IS750_REG_RXLVL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t SC16IS752HardwareSerial::FIFOAvailableSpace() {
	return ReadRegister(SC16IS750_REG_TXLVL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::WriteByte(uint8_t val) {
	uint8_t tmp_lsr;

	do {
		tmp_lsr = ReadRegister(SC16IS750_REG_LSR);
	} while ((tmp_lsr & 0x20) == 0);

	WriteRegister(SC16IS750_REG_THR, val);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int SC16IS752HardwareSerial::ReadByte() {
	volatile uint8_t val;

	if (FIFOAvailableData() == 0) {
		return -1;
	}

	val = ReadRegister(SC16IS750_REG_RHR);
	return val;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::EnableTransmit(uint8_t tx_enable) {
	uint8_t temp_efcr = ReadRegister(SC16IS750_REG_EFCR);
	if (tx_enable == 0) {
		temp_efcr |= 0x04;
	} else {
		temp_efcr &= 0xFB;
	}
	WriteRegister(SC16IS750_REG_EFCR, temp_efcr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SC16IS752HardwareSerial::flush() {
	uint8_t tmp_lsr;

	do {
		tmp_lsr = ReadRegister(SC16IS750_REG_LSR);
	} while ((tmp_lsr & 0x20) == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
int SC16IS752HardwareSerial::peek() {
	if (peek_flag == 0) {
		peek_buf = ReadByte();
		if (peek_buf >= 0) {
			peek_flag = 1;
		}
	}
	return peek_buf;
}
