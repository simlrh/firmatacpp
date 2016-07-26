#include "FirmSerial.h"
#include "serial/serial.h"

namespace firmata {

	FirmSerial::FirmSerial(const std::string &port, 
		uint32_t baudrate) : m_serial(port, baudrate, serial::Timeout::simpleTimeout(50)) 
	{
	}

	FirmSerial::~FirmSerial()
	{
		m_serial.close();
	}

	void FirmSerial::open()
	{
		if (!m_serial.isOpen()) m_serial.open();
	}

	bool FirmSerial::isOpen()
	{
		return m_serial.isOpen();
	}

	void FirmSerial::close()
	{
		m_serial.close();
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

	std::vector<PortInfo> FirmSerial::listPorts()
	{
		std::vector<serial::PortInfo> ports = serial::list_ports();
		std::vector<firmata::PortInfo> port_list;
		for (auto port : ports) {
			port_list.push_back({
				port.port,
				port.description,
				port.hardware_id
			});
		}
		return port_list;
	}

}