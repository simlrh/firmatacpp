#ifndef __FIRMI2C_H__
#define __FIRMI2C_H__

#include <firmatacpp_export.h>
#include "firmata_constants.h"
#include "firmbase.h"
#include "firmio.h"

#define FIRMATA_I2C_REQUEST	0x76
#define FIRMATA_I2C_REPLY	0x77
#define FIRMATA_I2C_CONFIG	0x78

#define FIRMATA_I2C_AUTO_RESTART	0x40
#define FIRMATA_I2C_10_BIT		0x20
#define FIRMATA_I2C_WRITE		0x00
#define FIRMATA_I2C_READ_ONCE		0x08
#define FIRMATA_I2C_READ_CONTINUOUS	0x10
#define FIRMATA_I2C_STOP_READING	0x18

#define FIRMATA_I2C_REGISTER_NOT_SPECIFIED 0x00

namespace firmata {

	class FIRMATACPP_EXPORT I2C : virtual Base {
	public:
		I2C(FirmIO *firmIO);
		virtual ~I2C();

		void configI2C(uint32_t delay);
		void reportI2C(uint16_t address, uint16_t reg, uint32_t bytes);
		std::vector<uint8_t> readI2C(uint16_t address, uint16_t reg = 0);
		std::vector<uint8_t> readI2COnce(uint16_t address, uint16_t reg, uint32_t bytes);
		void writeI2C(uint16_t address, std::vector<uint8_t> data);

	protected:
		virtual bool handleSysex(uint8_t command, std::vector<uint8_t> data);
		virtual bool handleString(std::string data);

	private:
		uint32_t m_delay;
		bool reporting[1024][1024];
		std::vector<uint8_t> values[1024][1024];
	};

}

#endif // !__FIRMI2C_H__
