#ifndef		__FIRMSERIAL_H_
#define		__FIRMSERIAL_H_

#include "firmio.h"
#include "serial/serial.h"

namespace firmata {

	typedef struct PortInfo {
		std::string port;
		std::string description;
		std::string hardware_id;
	} PortInfo;

	class FirmSerial : public FirmIO {
	public:
		FirmSerial(const std::string &port = "",
			uint32_t baudrate = 57600);
		~FirmSerial();

		virtual void open() override;
		virtual bool isOpen() override;
		virtual void close() override;
		virtual size_t available() override;
		virtual std::vector<uint8_t> read(size_t size = 1) override;
		virtual size_t write(std::vector<uint8_t> bytes) override;

		static std::vector<PortInfo> listPorts();

	private:
		serial::Serial m_serial;
	};

}

#endif