#include "firmi2c.h"

namespace firmata {
	I2C::I2C(FirmIO* firmIO) : Base(firmIO) {};
	I2C::~I2C() {};

	void I2C::configI2C(uint32_t delay)
	{
		m_delay = delay;

		uint8_t lsb = FIRMATA_LSB(delay);
		uint8_t msb = FIRMATA_MSB(delay);

		sysexCommand({ FIRMATA_I2C_CONFIG, lsb, msb });
	}

	void I2C::reportI2C(uint16_t address, uint16_t reg, uint32_t bytes)
	{
		uint8_t address_lsb = FIRMATA_LSB(address);
		uint8_t address_msb = FIRMATA_MSB(address);

		if (address_msb) {
			address_msb |= FIRMATA_I2C_10_BIT;
		}

		if (bytes == 0) {
			address_msb |= FIRMATA_I2C_STOP_READING;
			reporting[address][reg] = false;
		}
		else {
			address_msb |= FIRMATA_I2C_READ_CONTINUOUS;		
			reporting[address][reg] = true;
		}

		uint8_t bytes_lsb = FIRMATA_LSB(bytes);
		uint8_t bytes_msb = FIRMATA_MSB(bytes);

		if (reg == FIRMATA_I2C_REGISTER_NOT_SPECIFIED) {
			sysexCommand({ FIRMATA_I2C_REQUEST, address_lsb, address_msb, bytes_lsb, bytes_msb });
		}
		else {
			uint8_t register_lsb = FIRMATA_LSB(reg);
			uint8_t register_msb = FIRMATA_MSB(reg);

			sysexCommand({ FIRMATA_I2C_REQUEST, address_lsb, address_msb, register_lsb, register_msb, bytes_lsb, bytes_msb });
		}
	}

	std::vector<uint8_t> I2C::readI2COnce(uint16_t address, uint16_t reg, uint32_t bytes)
	{
		uint8_t address_lsb = FIRMATA_LSB(address);
		uint8_t address_msb = FIRMATA_MSB(address);

		if (address_msb) {
			address_msb |= FIRMATA_I2C_10_BIT;
		}
		address_msb |= FIRMATA_I2C_READ_ONCE;

		uint8_t bytes_lsb = FIRMATA_LSB(bytes);
		uint8_t bytes_msb = FIRMATA_MSB(bytes);

		if (reg == FIRMATA_I2C_REGISTER_NOT_SPECIFIED) {
			sysexCommand({ FIRMATA_I2C_REQUEST, address_lsb, address_msb, bytes_lsb, bytes_msb });
		}
		else {
			uint8_t register_lsb = FIRMATA_LSB(reg);
			uint8_t register_msb = FIRMATA_MSB(reg);

			sysexCommand({ FIRMATA_I2C_REQUEST, address_lsb, address_msb, register_lsb, register_msb, bytes_lsb, bytes_msb });
		}

		awaitSysexResponse(FIRMATA_I2C_REPLY); // TODO: Wait for specific reply, not just any reply

		return values[address][reg];
	}

	std::vector<uint8_t> I2C::readI2C(uint16_t address, uint16_t reg)
	{
		if (reporting[address][reg]) return values[address][reg];
		return {};
	}

	void I2C::writeI2C(uint16_t address, std::vector<uint8_t> data)
	{
		uint8_t address_lsb = FIRMATA_LSB(address);
		uint8_t address_msb = FIRMATA_MSB(address);

		if (address_msb) {
			address_msb |= FIRMATA_I2C_10_BIT;
		}
		address_msb |= FIRMATA_I2C_WRITE;

		std::vector<uint8_t> sysex_buffer = { FIRMATA_I2C_REQUEST, address_lsb, address_msb };

		uint8_t lsb;
		uint8_t msb;
		for (uint8_t byte : data) {
			lsb = FIRMATA_LSB(byte);
			msb = FIRMATA_MSB(byte);
			sysex_buffer.push_back(lsb);
			sysex_buffer.push_back(msb);
		}

		sysexCommand(sysex_buffer);
	}

	bool I2C::handleSysex(uint8_t command, std::vector<uint8_t> data)
	{
		if (command == FIRMATA_I2C_REPLY) {
			std::vector<uint8_t> reply_buffer = {};

			uint16_t address = FIRMATA_COMBINE_LSB_MSB(data[0], data[1]);
			uint16_t reg = FIRMATA_COMBINE_LSB_MSB(data[2], data[3]);

			for (int i = 4; i + 1 < data.size(); i = i + 2) {
				uint8_t reply_byte = FIRMATA_COMBINE_LSB_MSB(data[i], data[i + 1]);
				reply_buffer.push_back(reply_byte);
			}

			values[address][reg] = reply_buffer;

			return true;
		}
		return false;
	}

	bool I2C::handleString(std::string data)
	{
		return false;
	}
}
