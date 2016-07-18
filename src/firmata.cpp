#include "firmata.h"
#include "firmio.h" 

namespace firmata {
	Firmata::Firmata(FirmIO &firmIO)
		: m_firmIO(&firmIO), name(""), major_version(0), minor_version(0), ready(false)
	{
		m_firmIO->open();

		initPins();
		sysexCommand(FIRMATA_REPORT_FIRMWARE);
	}

	Firmata::~Firmata()
	{
		m_firmIO->close();
	}

	bool Firmata::isReady()
	{
		if (!ready) parse();
		return ready;
	}

	void Firmata::pinMode(uint8_t pin, uint8_t mode)
	{
		pins[pin].mode = mode;
		m_firmIO->write({ FIRMATA_SET_PIN_MODE, pin, mode });
	}

	void Firmata::analogWrite(uint8_t pin, int value)
	{
		if (pin > 15 || value > FIRMATA_MAX) {
			return analogWriteExtended(pin, value);
		}

		uint8_t analog_write_pin = FIRMATA_ANALOG_MESSAGE | pin;
		uint8_t lsb = FIRMATA_LSB(value);
		uint8_t msb = FIRMATA_MSB(value);
		standardCommand({ analog_write_pin, lsb, msb });
	}

	void Firmata::analogWriteExtended(uint8_t pin, int value)
	{
		std::vector<uint8_t> bytes({ FIRMATA_EXTENDED_ANALOG, pin });
		bytes.push_back(FIRMATA_LSB(value));
		bytes.push_back(FIRMATA_MSB(value));

		//
		for (value >>= 14; value > 0; value >>= 7) {
			bytes.push_back(FIRMATA_LSB(value));
		}
		sysexCommand(bytes);
	}


	void Firmata::digitalWrite(uint8_t pin, uint8_t value = HIGH)
	{
		standardCommand({ FIRMATA_SET_DIGITAL_PIN, pin, value });
	}


	void Firmata::reportAnalog(uint8_t channel, uint8_t enable)
	{
		uint8_t report_channel = FIRMATA_REPORT_ANALOG | channel;
		standardCommand({ report_channel, enable });
	}

	void Firmata::reportDigital(uint8_t port, uint8_t enable)
	{
		uint8_t report_port = FIRMATA_REPORT_DIGITAL | port;
		standardCommand({ report_port, enable });
	}

	void Firmata::setSamplingInterval(int intervalms)
	{
		uint8_t lsb = FIRMATA_LSB(intervalms);
		uint8_t msb = FIRMATA_MSB(intervalms);

		sysexCommand({ FIRMATA_SAMPLING_INTERVAL, lsb, msb });
	}

	void Firmata::standardCommand(std::vector<uint8_t> standard_command)
	{
		m_firmIO->write(standard_command);
	}

	void Firmata::sysexCommand(uint8_t sysex_command)
	{
		m_firmIO->write({ FIRMATA_START_SYSEX, sysex_command, FIRMATA_END_SYSEX });
	}

	void Firmata::sysexCommand(std::vector<uint8_t> sysex_command)
	{
		sysex_command.insert(sysex_command.begin(), FIRMATA_START_SYSEX);
		sysex_command.insert(sysex_command.end(), FIRMATA_END_SYSEX);

		m_firmIO->write(sysex_command);
	}

	void Firmata::parse()
	{
		size_t input = m_firmIO->waitForBytes();
		if (!input) return;

		std::vector<uint8_t> parse_buffer = m_firmIO->read(FIRMATA_MSG_LEN);
		if (parse_buffer.size() == 0) return;

		for (int i = 0; i < parse_buffer.size(); i++) {
			uint8_t whole_command = parse_buffer[i];
			uint8_t first_nibble = FIRMATA_FIRST_NIBBLE(whole_command);

			int value, channel, port;

			switch (first_nibble) {
			case(FIRMATA_REPORT_VERSION) :
				major_version = parse_buffer[++i];
				minor_version = parse_buffer[++i];
				break;
			case(FIRMATA_ANALOG_MESSAGE) :
				channel = FIRMATA_LAST_NIBBLE(whole_command);
				value = FIRMATA_COMBINE_LSB_MSB(parse_buffer[++i], parse_buffer[++i]);
				for (int pin = 0; pin < 128; pin++) {
					if (pins[pin].analog_channel == channel) {
						pins[pin].value = value;
					}
				}
				break;
			case(FIRMATA_DIGITAL_MESSAGE) :
				port = FIRMATA_LAST_NIBBLE(whole_command);
				value = FIRMATA_COMBINE_LSB_MSB(parse_buffer[++i], parse_buffer[++i]);
				for (int pin = 0; pin < 8; pin++) {
					if (pins[port * 8 + pin].mode == MODE_INPUT) {
						pins[port * 8 + pin].value = FIRMATA_NTH_BIT(value, pin);
					}
				}
				break;
			}

			switch (whole_command) {
			case(FIRMATA_START_SYSEX) :
				uint8_t subcommand = parse_buffer[++i];

				std::vector<uint8_t> sysex_buffer;
				for (++i; parse_buffer[i] != FIRMATA_END_SYSEX; i++) { // Copy sysex and skip to next command
					sysex_buffer.push_back(parse_buffer[i]);
				}

				handleSysex(subcommand, sysex_buffer);
				break;
			}
		}
	}

	bool Firmata::handleSysex(uint8_t subcommand, std::vector<uint8_t> data)
	{
		bool is_mode_byte;
		uint8_t pin;

		switch (subcommand) {
		case(FIRMATA_REPORT_FIRMWARE) :
			major_version = data[0];
			minor_version = data[1];

			name = stringFromBytes(data.begin()+2, data.end());

			if (!ready) {
				ready = true;
				sysexCommand(FIRMATA_ANALOG_MAPPING_QUERY);
				sysexCommand(FIRMATA_CAPABILITY_QUERY);
				for (int j = 0; j < 16; j++) {
					reportAnalog(j);
					reportDigital(j);
				}
			}

			return true;
		case(FIRMATA_CAPABILITY_RESPONSE) :
			pin = 0;
			is_mode_byte = true;

			for (pin = 0; pin < 128; pin++) {
				pins[pin].supported_modes = {};
				pins[pin].resolutions = {};
			}

			pin = 0;
			for (uint8_t byte : data) {
				if (byte == 127) {
					pin++;
					is_mode_byte = true;
				}
				else if (is_mode_byte) {
					pins[pin].supported_modes.push_back(byte);
					is_mode_byte = false;
				}
				else {
					pins[pin].resolutions.push_back(byte);
					is_mode_byte = true;
				}
			}

			// send a state query for for every pin with any modes                                
			for (pin = 0; pin < 128; pin++) {
				if (pins[pin].supported_modes.size()) {
					sysexCommand({ FIRMATA_PIN_STATE_QUERY, pin });
				}
			}
			return true;

		case(FIRMATA_PIN_STATE_RESPONSE) :
			pin = data[0];
			pins[pin].mode = data[1];
			pins[pin].value = data[2];
			if (data.size() > 3) pins[pin].value |= (data[3] << 7);
			if (data.size() > 4) pins[pin].value |= (data[4] << 14);
			return true;

		case(FIRMATA_ANALOG_MAPPING_RESPONSE) :
			for (pin = 0; pin < data.size(); pin++) 
			{
				pins[pin].analog_channel = data[pin];
			}
			return true;

		case(FIRMATA_STRING) :
			handleString(stringFromBytes(data.begin(), data.end()));
			return true;
		}
	}

	bool Firmata::handleString(std::string)
	{
		return false;
	}

	std::string Firmata::stringFromBytes(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end)
	{
		std::string s;
		for (auto byte = begin; byte != end; ++byte) {
			s += (*byte) | (*(++byte) << 7);
		}
		return s;
	}

	void Firmata::initPins()
	{
		for (int i = 0; i < 128; i++) {
			pins[i].mode = 255;
			pins[i].analog_channel = 127;
			pins[i].supported_modes = {};
			pins[i].resolutions = {};
			pins[i].value = 0;
		}
	}
}