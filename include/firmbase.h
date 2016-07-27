#ifndef __FIRMBASE_H__
#define __FIRMBASE_H__

#include <firmatacpp_export.h>
#include "firmata_constants.h"
#include "firmio.h"

#include <string>

namespace firmata {

	class FIRMATACPP_EXPORT Base {
	public:
		Base(FirmIO *firmIO);
		virtual ~Base();

		void init();

		bool is_ready;
		std::string name;
		int major_version;
		int minor_version;

		bool ready();

		uint16_t parse(uint32_t num_commands = 0);

		void pinMode(uint8_t pin, uint8_t mode);
		void digitalWrite(uint8_t pin, uint8_t value);
		void analogWrite(uint8_t pin, uint32_t value);
		void analogWrite(const std::string& channel, uint32_t value);

		uint8_t digitalRead(uint8_t pin);
		uint32_t analogRead(uint8_t pin);
		uint32_t analogRead(const std::string& channel);

		void standardCommand(std::vector<uint8_t> standard_command);
		void sysexCommand(uint8_t sysex_command);
		void sysexCommand(std::vector<uint8_t> sysex_command);

		void reportAnalog(uint8_t channel, uint8_t enable = 1);
		void reportDigital(uint8_t port, uint8_t enable = 1);
		void setSamplingInterval(uint32_t intervalms);

	protected:
		virtual bool handleSysex(uint8_t command, std::vector<uint8_t> data);
		virtual bool handleString(std::string data);

		bool awaitResponse(uint8_t command, uint32_t timeout = 1000);
		bool awaitSysexResponse(uint8_t sysexCommand, uint32_t timeout = 1000);

	private:
		void initPins();
		void reportFirmware();
		void capabilityQuery();
		void analogMappingQuery();
		void pinStateQuery();

		std::string stringFromBytes(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end);

		void analogWriteExtended(uint8_t pin, uint32_t value);
		void savePartialBuffer(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end);
		std::vector<uint8_t> saved_buffer;


		FirmIO* m_firmIO;
		t_pin pins[128];
	};

}

#endif // !__FIRMBASE_H__
