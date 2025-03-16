#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Semaphore {
	public:
		explicit Semaphore();
		virtual ~Semaphore();

	public:
		bool isTaken() const;
		bool take(uint32_t timeout = 0) const;
		void give() const;

	protected:
		SemaphoreHandle_t semaphore;
};

class SemaphoreBinary : public Semaphore {
	public:
		explicit SemaphoreBinary(bool given = true);
};

#endif
