#ifndef HardwareSerial_h
#define HardwareSerial_h

#include <inttypes.h>
#include "Stream.h"
#include "esp32-hal.h"

class HardwareSerial: public Stream {
	public:
		virtual void begin(unsigned long baud, uint32_t config = SERIAL_8N1) = 0;
		virtual void end() = 0;

		virtual int availableForWrite() = 0;
};

#endif
