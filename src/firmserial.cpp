#include "FirmSerial.h"
#include "serial/serial.h"

namespace firmata {

	FirmSerial::FirmSerial(const std::string &port, 
		uint32_t baudrate) : m_serial(port, baudrate, serial::Timeout::simpleTimeout(50)) {}


	void FirmSerial::open()
	{
		m_serial.open();
	}

	bool FirmSerial::isOpen()
	{
		return m_serial.isOpen();
	}

	void FirmSerial::close()
	{
		m_serial.close();
	}

	bool FirmSerial::waitForBytes()
	{
		return m_serial.waitReadable() && m_serial.available();
	}

	size_t FirmSerial::available()
	{
		return m_serial.available();
	}

	std::vector<uint8_t> FirmSerial::read(size_t size)
	{
		std::vector<uint8_t> bytes;
		m_serial.read(bytes, size);
		return bytes;
	}

	size_t FirmSerial::write(std::vector<uint8_t> bytes)
	{
		return m_serial.write(bytes);
	}

}