#ifndef Uart_h
#define Uart_h

#include "HardwareSerial.h"
#include "soc/soc_caps.h"
#include "HWCDC.h"
#include "pins_arduino.h"

class Uart: public HardwareSerial
{
public:
    Uart(int uart_nr);

	virtual void begin(unsigned long baud, uint32_t config=SERIAL_8N1) {
		begin(baud, config, -1, -1);
	}
    void begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert=false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
	inline void end() {
		end(true);
	}
    void end(bool turnOffDebug);
    void updateBaudRate(unsigned long baud);
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    inline size_t read(char * buffer, size_t size)
    {
        return read((uint8_t*) buffer, size);
    }
    void flush(void);
    void flush( bool txOnly);
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    inline size_t write(const char * buffer, size_t size)
    {
        return write((uint8_t*) buffer, size);
    }
    inline size_t write(const char * s)
    {
        return write((uint8_t*) s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t) n);
    }
    uint32_t baudRate();

    size_t setRxBufferSize(size_t new_size);
    void setDebugOutput(bool);

    operator bool() const;

    void setRxInvert(bool);
    void setPins(uint8_t rxPin, uint8_t txPin);

protected:
    int _uart_nr;
    uart_t* _uart;
    size_t _rxBufferSize;
};

extern void serialEventRun(void) __attribute__((weak));

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
#include "USB.h"
#include "USBCDC.h"
extern Uart Serial0;
#elif ARDUINO_HW_CDC_ON_BOOT
extern Uart Serial0;
#else
extern Uart Serial;
#endif
/*
#if SOC_UART_NUM > 1
extern Uart Serial1;
#endif
*/
#if SOC_UART_NUM > 2
extern Uart Serial1;
#endif
#endif

// Serial2 for WiFi Module
#if defined(WIFI_MODULE)
extern Uart Serial2;
#endif

#endif
