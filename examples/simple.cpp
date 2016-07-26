#include <iostream>

#include "firmata.h"
#include "firmserial.h"

/*
 * Detect first serial port with a StandardFirmata interface
 * Read analog inputs A0 and A1 and digital pin 2 (eg, a Playstation analog stick + button)
 * as well as I2C address 8 (eg, the slave_sender example that comes with Arduino IDE)
 * and print to stdout
 */

int main(int argc, const char* argv[])
{
	std::vector<firmata::PortInfo> ports = firmata::FirmSerial::listPorts();
	firmata::Firmata<firmata::Base, firmata::I2C>* f;

	for (auto port : ports) {
		std::cout << port.port << std::endl;
		firmata::FirmSerial* serialio = new firmata::FirmSerial(port.port.c_str());
		f = new firmata::Firmata<firmata::Base, firmata::I2C>(serialio);
		if (f->ready()) {
			break;
		}
		f = NULL;
	}

	if (f == NULL) return 1;

	f->setSamplingInterval(50);

	std::cout << f->name << std::endl;
	std::cout << f->major_version << std::endl;
	std::cout << f->minor_version << std::endl;

	f->reportAnalog(0, 1);
	f->reportAnalog(1, 1);
	f->pinMode(2, MODE_INPUT);
	f->reportDigital(0, 1);
	f->configI2C(0);
	f->reportI2C(8, FIRMATA_I2C_REGISTER_NOT_SPECIFIED, 6);
	
	while (true) {
		f->parse();
		int a0 = f->analogRead("A0");
		int a1 = f->analogRead("A1");
		int pin2 = f->digitalRead(2);
		std::vector<uint8_t> i2c = f->readI2C(8);
		std::string s = "";
		for (auto byte = i2c.begin(); byte < i2c.end(); ++byte) {
			s += (char)*byte;
		}
		
		std::cout << a0 << ", " << a1 << ", " << pin2 <<  ", " << s << std::endl;
	};

	delete f;
}