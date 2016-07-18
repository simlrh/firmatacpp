#ifndef		__FIRMSERIAL_H_
#define		__FIRMSERIAL_H_

#include "firmio.h"
#include "serial/serial.h"

namespace firmata {

	class FirmSerial : FirmIO {
	public:
		FirmSerial(const std::string &port = "",
			uint32_t baudrate = 9600);

		/*! Destructor */
		virtual ~FirmSerial();

		virtual void open() override;
		virtual bool isOpen() override;
		virtual void close() override;
		virtual bool waitForBytes() override;
		virtual size_t available() override;
		virtual std::vector<uint8_t> read(size_t size = 1) override;
		virtual size_t write(std::vector<uint8_t> bytes) override;

	private:
		serial::Serial m_serial;
	};

}

#endif