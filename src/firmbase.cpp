#include "firmbase.h"

#include <chrono>
#include <string>
#include <iostream>

namespace firmata {

	Base::Base(FirmIO *firmIO)
		: m_firmIO(firmIO), name(""), major_version(0), minor_version(0), is_ready(false)
	{
		m_firmIO->open();
		standardCommand({ FIRMATA_REPORT_VERSION });
		is_ready = awaitResponse(FIRMATA_REPORT_VERSION);
		if (is_ready) {
			init();
		}
	}

	Base::~Base()
	{
		m_firmIO->close();
		delete m_firmIO;
	}

	bool Base::ready()
	{
		return is_ready;
	}

	void Base::init()
	{
		reportFirmware();
		initPins();
		capabilityQuery();
		analogMappingQuery();
		pinStateQuery();
	}

	void Base::pinMode(uint8_t pin, uint8_t mode)
	{
		pins[pin].mode = mode;
		m_firmIO->write({ FIRMATA_SET_PIN_MODE, pin, mode });
	}

	void Base::digitalWrite(uint8_t pin, uint8_t value = HIGH)
	{
		pins[pin].value = value;

		standardCommand({ FIRMATA_SET_DIGITAL_PIN, pin, value });
	}

	void Base::analogWrite(uint8_t pin, uint32_t value)
	{
		if (pin > 15 || value > FIRMATA_MAX) {
			return analogWriteExtended(pin, value);
		}

		pins[pin].value = value;

		uint8_t analog_write_pin = FIRMATA_ANALOG_MESSAGE | pin;
		uint8_t lsb = FIRMATA_LSB(value);
		uint8_t msb = FIRMATA_MSB(value);
		standardCommand({ analog_write_pin, lsb, msb });
	}

	void Base::analogWriteExtended(uint8_t pin, uint32_t value)
	{
		pins[pin].value = value;

		std::vector<uint8_t> bytes({ FIRMATA_EXTENDED_ANALOG, pin });
		bytes.push_back(FIRMATA_LSB(value));
		bytes.push_back(FIRMATA_MSB(value));

		// Keep sending more significant bytes until value is complete
		for (value >>= 14; value > 0; value >>= 7) {
			bytes.push_back(FIRMATA_LSB(value));
		}
		sysexCommand(bytes);
	}

	void Base::analogWrite(const std::string& channel, uint32_t value)
	{
		if (channel[0] != 'A') return;
		for (uint8_t pin = 0; pin < 127; pin++) {
			if (pins[pin].analog_channel + '0' == channel[1]) {
				pins[pin].value = value;
				analogWrite(pin, value);
				return;
			}
		}
	}

	uint8_t Base::digitalRead(uint8_t pin)
	{
		return pins[pin].value;
	}

	uint32_t Base::analogRead(uint8_t pin)
	{
		return pins[pin].value;
	}

	uint32_t Base::analogRead(const std::string& channel)
	{
		if (channel[0] != 'A') return 0;
		for (uint8_t pin = 0; pin < 127; pin++) {
			if (pins[pin].analog_channel + '0' == channel[1]) {
				return pins[pin].value;
			}
		}
		return 0;
	}

	void Base::reportAnalog(uint8_t channel, uint8_t enable)
	{
		uint8_t report_channel = FIRMATA_REPORT_ANALOG | channel;
		standardCommand({ report_channel, enable });
	}

	void Base::reportDigital(uint8_t port, uint8_t enable)
	{
		uint8_t report_port = FIRMATA_REPORT_DIGITAL | port;
		standardCommand({ report_port, enable });
	}

	void Base::setSamplingInterval(uint32_t intervalms)
	{

		uint8_t lsb = intervalms & 0x7F;
		uint8_t msb = (intervalms >> 7) & 0x7F;

		sysexCommand({ FIRMATA_SAMPLING_INTERVAL, lsb, msb });
	}

	void Base::standardCommand(std::vector<uint8_t> standard_command)
	{
		m_firmIO->write(standard_command);
	}

	void Base::sysexCommand(uint8_t sysex_command)
	{
		m_firmIO->write({ FIRMATA_START_SYSEX, sysex_command, FIRMATA_END_SYSEX });
	}

	void Base::sysexCommand(std::vector<uint8_t> sysex_command)
	{
		sysex_command.insert(sysex_command.begin(), FIRMATA_START_SYSEX);
		sysex_command.insert(sysex_command.end(), FIRMATA_END_SYSEX);

		m_firmIO->write(sysex_command);
	}

	uint16_t Base::parse(uint32_t num_commands)
	{
		std::vector<uint8_t> new_data = m_firmIO->read(FIRMATA_MSG_LEN);
		std::vector<uint8_t> parse_buffer(saved_buffer);
		parse_buffer.insert(parse_buffer.end(), new_data.begin(), new_data.end());
		if (parse_buffer.size() == 0) return 0;

		bool interrupted_command = false;
		uint32_t completed_commands = 0;

		for (int i = 0; i < parse_buffer.size(); i++) {
			uint8_t whole_command, command_index, first_nibble;
			uint16_t last_completed = 0;

			command_index = i;

			whole_command = parse_buffer[i];
			first_nibble = FIRMATA_FIRST_NIBBLE(whole_command);

			uint32_t value, channel, port;

			switch (first_nibble) {
			case(FIRMATA_ANALOG_MESSAGE) :
				if (parse_buffer.size() < i + 3) {
					interrupted_command = true;
				}
				else {
					channel = FIRMATA_LAST_NIBBLE(whole_command);
					uint8_t lsb = parse_buffer[i + 1];
					uint8_t msb = parse_buffer[i + 2];

					if (lsb > 0x7F) continue; // TODO: Why do we sometimes get 2 analog message bytes instead of one. Off by one in savePartialBuffer?
					if (msb > 0x7F) continue; // Why do we sometimes get only 1 data byte?

					value = FIRMATA_COMBINE_LSB_MSB(lsb, msb);
					for (int pin = 0; pin < 128; pin++) {
						if (pins[pin].analog_channel == channel) {
							pins[pin].value = value;
							break;
						}
					}
					i += 2;
					completed_commands++;
					last_completed = whole_command;
				}
				break;
			case(FIRMATA_DIGITAL_MESSAGE) :
				if (parse_buffer.size() < i + 3) {
					interrupted_command = true;
				}
				else {
					port = FIRMATA_LAST_NIBBLE(whole_command);
					uint8_t lsb = parse_buffer[i + 1];
					uint8_t msb = parse_buffer[i + 2];

					if (lsb > 0x7F) continue; // TODO: Why do we sometimes get 2 analog message bytes instead of one. Off by one in savePartialBuffer?
					if (msb > 0x7F) continue; // Why do we sometimes get only 1 data byte?

					value = FIRMATA_COMBINE_LSB_MSB(lsb, msb);
					for (int pin = 0; pin < 8; pin++) {
						if (pins[port * 8 + pin].mode == MODE_INPUT) {
							pins[port * 8 + pin].value = FIRMATA_NTH_BIT(value, pin);
						}
					}
					i += 2;
					completed_commands++;
					last_completed = whole_command;
				}
				break;
			}

			switch (whole_command) {
			case(FIRMATA_REPORT_VERSION) :
				if (parse_buffer.size() < i + 3) {
					interrupted_command = true;
				}
				else {
					major_version = parse_buffer[i + 1];
					minor_version = parse_buffer[i + 2];
					i += 2;
					completed_commands++;
					last_completed = whole_command;
				}
				break;
			case(FIRMATA_START_SYSEX) :
				if (parse_buffer.size() < i + 2) {
					interrupted_command = true;
				}
				else {
					uint8_t subcommand = parse_buffer[i + 1];

					std::vector<uint8_t> sysex_buffer;
					for (i = i + 2; i < parse_buffer.size() && parse_buffer[i] != FIRMATA_END_SYSEX; i++) { // Copy sysex and skip to next command
						sysex_buffer.push_back(parse_buffer[i]);
					}
					if (i == parse_buffer.size()) {
						interrupted_command = true;
					}
					else {
						handleSysex(subcommand, sysex_buffer);
						completed_commands++;
						last_completed = (whole_command << 8) | subcommand;
					}
				}
				break;
			}

			if (interrupted_command) {
				savePartialBuffer(parse_buffer.begin() + command_index, parse_buffer.end());
				return last_completed;
			}
			else if (num_commands && num_commands == completed_commands) {
				savePartialBuffer(parse_buffer.begin() + i + 1, parse_buffer.end());
				return last_completed;
			}
		}
	}

	bool Base::handleSysex(uint8_t subcommand, std::vector<uint8_t> data)
	{
		bool is_mode_byte;
		uint8_t pin;

		switch (subcommand) {
		case(FIRMATA_REPORT_FIRMWARE) :
			major_version = data[0];
			minor_version = data[1];

			name = stringFromBytes(data.begin() + 2, data.end());


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

			return true;

		case(FIRMATA_PIN_STATE_RESPONSE) :
			pin = data[0];
			pins[pin].mode = data[1];
			pins[pin].value = data[2];
			if (data.size() > 3) pins[pin].value |= (data[3] << 7);
			if (data.size() > 4) pins[pin].value |= (data[4] << 14);
			return true;

		case(FIRMATA_ANALOG_MAPPING_RESPONSE) :
			for (pin = 0; pin < data.size(); pin++) {
				pins[pin].analog_channel = data[pin];
			}
			return true;

		case(FIRMATA_STRING) :
			handleString(stringFromBytes(data.begin(), data.end()));
			return true;

		}

		return false;
	}

	bool Base::handleString(std::string data)
	{
		std::cout << data << std::endl;
		return false;
	}

	void Base::savePartialBuffer(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end) {
		saved_buffer = {};
		for (auto byte = begin; byte != end; ++byte) {
			saved_buffer.push_back(*byte);
		}
	}

	std::string Base::stringFromBytes(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end)
	{
		std::string s;
		for (auto byte = begin; byte < end - 1; ++byte) {
			s += (*byte) | (*(++byte) << 7);
		}
		return s;
	}

	bool Base::awaitResponse(uint8_t command, uint32_t timeout)
	{
		bool succeeded = true;
		std::chrono::time_point<std::chrono::system_clock> start, current;
		start = std::chrono::system_clock::now();
		std::chrono::duration<std::chrono::system_clock::rep,
			std::chrono::system_clock::period> timeoutDuration(timeout * 10000), elapsed;

		uint8_t first_nibble = FIRMATA_FIRST_NIBBLE(command);
		uint16_t result;
		do {
			current = std::chrono::system_clock::now();
			elapsed = current - start;
			if (elapsed > timeoutDuration) {
				succeeded = false;
				break;
			}

			result = parse(1);
		} while (FIRMATA_FIRST_NIBBLE(result) != first_nibble);

		return succeeded;
	}

	bool Base::awaitSysexResponse(uint8_t sysexCommand, uint32_t timeout)
	{
		bool succeeded = true;
		std::chrono::time_point<std::chrono::system_clock> start, current;
		start = std::chrono::system_clock::now();
		std::chrono::duration<std::chrono::system_clock::rep, 
			std::chrono::system_clock::period> timeoutDuration(timeout * 10000), elapsed;

		uint16_t result, result_sysex, result_command;
		do {
			current = std::chrono::system_clock::now();
			elapsed = current - start;
			if (elapsed > timeoutDuration) {
				succeeded = false;
				break;
			}

			result = parse(1);
			result_sysex = result >> 8;
			result_command = result & 0x00FF;
		} while (!(result_sysex == FIRMATA_START_SYSEX && result_command == sysexCommand));

		return succeeded;
	}

	void Base::initPins()
	{
		for (int i = 0; i < 128; i++) {
			pins[i].mode = 255;
			pins[i].analog_channel = 127;
			pins[i].supported_modes = {};
			pins[i].resolutions = {};
			pins[i].value = 0;
		}
	}

	void Base::reportFirmware() {
		sysexCommand(FIRMATA_REPORT_FIRMWARE);
		is_ready = awaitSysexResponse(FIRMATA_REPORT_FIRMWARE);
	}

	void Base::capabilityQuery() {
		sysexCommand(FIRMATA_CAPABILITY_QUERY);
		awaitSysexResponse(FIRMATA_CAPABILITY_RESPONSE);
	}

	void Base::analogMappingQuery() {
		sysexCommand(FIRMATA_ANALOG_MAPPING_QUERY);
		awaitSysexResponse(FIRMATA_ANALOG_MAPPING_RESPONSE);
		for (uint8_t pin = 0; pin < 128; pin++) {
			if (pins[pin].analog_channel < 127) {
				pins[pin].mode = MODE_ANALOG;
				standardCommand({ FIRMATA_SET_PIN_MODE, pin, MODE_ANALOG });
			}
		}
	}

	void Base::pinStateQuery() {
		// send a state query for for every pin with any modes                                
		for (uint8_t pin = 0; pin < 128; pin++) {
			if (pins[pin].supported_modes.size()) {
				sysexCommand({ FIRMATA_PIN_STATE_QUERY, pin });
//				awaitSysexResponse(FIRMATA_PIN_STATE_RESPONSE, 100);
			}
		}
	}
}
