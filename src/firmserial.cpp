#include "firmserial.h"

#include <serial/serial.h>
#include <iostream>

namespace firmata {

	FirmSerial::FirmSerial(const std::string &port, uint32_t baudrate)
	try : m_serial(port, baudrate, serial::Timeout::simpleTimeout(250)) {
#ifndef WIN32
	  serial::Timeout t = m_serial.getTimeout();
	  t.read_timeout_constant = 5000;
	  m_serial.setTimeout(t);
	  m_serial.waitReadable();
	  int count = m_serial.available();
	  t.read_timeout_constant = 250;
	  m_serial.setTimeout(t);
#endif
	}
	catch (serial::IOException e) {
	  throw firmata::IOException();
	}
	catch (serial::PortNotOpenedException e) {
	  throw firmata::NotOpenException();
	}

	FirmSerial::~FirmSerial()
	{
		m_serial.close();
	}

	void FirmSerial::open()
	{
	  try {
		if (!m_serial.isOpen()) m_serial.open();
	  } catch (serial::SerialException e) {
		throw firmata::IOException();
	  } catch (serial::IOException e) {
	        throw firmata::IOException();
	  }
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
		try {
		m_serial.read(bytes, size);
		} catch (serial::PortNotOpenedException e) {
		  throw firmata::NotOpenException();
		} catch (serial::SerialException e) {
		  throw firmata::IOException();
		}
		return bytes;
	}

	size_t FirmSerial::write(std::vector<uint8_t> bytes)
	{
		try {
		  return m_serial.write(bytes);
		} catch (serial::SerialException e) {
		  throw firmata::IOException();
		} catch (serial::IOException e) {
		  throw firmata::IOException();
		} catch (serial::PortNotOpenedException e) {
		  throw firmata::NotOpenException();
		}
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
