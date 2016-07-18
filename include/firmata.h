#ifndef		__FIRMATA_H_
#define		__FIRMATA_H_

#include "firmatacpp_export.h"
#include "firmio.h"

#define MODE_INPUT	0x00
#define MODE_OUTPUT	0x01
#define MODE_ANALOG	0x02
#define MODE_PWM	0x03
#define MODE_SERVO	0x04
#define MODE_SHIFT	0x05
#define MODE_I2C	0x06

#define LOW			0
#define HIGH		1

#define FIRMATA_START_SYSEX				0xF0 // start a MIDI Sysex message                                                   
#define FIRMATA_END_SYSEX				0xF7 // end a MIDI Sysex message                                                     
#define FIRMATA_PIN_MODE_QUERY			0x72 // ask for current and supported pin modes                                      
#define FIRMATA_PIN_MODE_RESPONSE		0x73 // reply with current and supported pin modes                                   
#define FIRMATA_PIN_STATE_QUERY			0x6D
#define FIRMATA_PIN_STATE_RESPONSE		0x6E
#define FIRMATA_CAPABILITY_QUERY		0x6B
#define FIRMATA_CAPABILITY_RESPONSE		0x6C
#define FIRMATA_ANALOG_MAPPING_QUERY	0x69
#define FIRMATA_ANALOG_MAPPING_RESPONSE	0x6A
#define FIRMATA_EXTENDED_ANALOG			0x6F

#define FIRMATA_FIRST_NIBBLE(x)			((x) & 0xF0)
#define FIRMATA_LAST_NIBBLE(x)			((x) & 0x0F)
#define FIRMATA_LSB(x)					((x) & 0x7F)
#define FIRMATA_MSB(x)					(((x) >> 7) & 0x7F)
#define FIRMATA_COMBINE_LSB_MSB(x,y)	((x) | ((y) << 7))
#define FIRMATA_NTH_BIT(x,y)			((x) & (1 << (y)))

#define FIRMATA_DIGITAL_MESSAGE         0x90 // send data for a digital pin port
#define FIRMATA_SET_DIGITAL_PIN			0xF4 // send data form single digital pin
#define FIRMATA_ANALOG_MESSAGE          0xE0 // send data for an analog pin (or PWM)
#define FIRMATA_ANALOG_MESSAGE          0xE0 // send data for an analog pin (or PWM)
#define FIRMATA_REPORT_ANALOG           0xC0 // enable analog input by pin #
#define FIRMATA_REPORT_DIGITAL          0xD0 // enable digital input by port pair
#define FIRMATA_SET_PIN_MODE            0xF4 // set a pin to INPUT/OUTPUT/PWM/etc

#define FIRMATA_REPORT_VERSION          0xF9 // report protocol version
#define FIRMATA_SYSTEM_RESET            0xFF // reset from MIDI

#define FIRMATA_START_SYSEX             0xF0 // start a MIDI Sysex message
#define FIRMATA_END_SYSEX               0xF7 // end a MIDI Sysex message
 
// extended command set using sysex (0-127/0x00-0x7F)
/* 0x00-0x0F reserved for custom commands */
#define FIRMATA_SERVO_CONFIG            0x70 // set max angle, minPulse, maxPulse, freq
#define FIRMATA_STRING                  0x71 // a string message with 14-bits per char
#define FIRMATA_REPORT_FIRMWARE         0x79 // report name and version of the firmware
#define FIRMATA_SAMPLING_INTERVAL		0x7A // report name and version of the firmware
#define FIRMATA_SYSEX_NON_REALTIME      0x7E // MIDI Reserved for non-realtime messages
#define FIRMATA_SYSEX_REALTIME          0x7F // MIDI Reserved for realtime messages

#define FIRMATA_MAX						0x3FFF
#define FIRMATA_MSG_LEN					1024

typedef struct		s_pin
{
  uint8_t				mode;
  uint8_t				analog_channel;
  std::vector<uint8_t>	supported_modes;
  std::vector<uint8_t>	resolutions;
  uint32_t				value;
} t_pin;

namespace FIRMATACPP_EXPORT firmata {

	class FIRMATACPP_EXPORT Firmata {
		Firmata(FirmIO &firmIO);
		virtual ~Firmata();

	public:
		std::string name;
		int major_version;
		int minor_version;

		bool isReady();
		void parse();

		void pinMode(uint8_t pin, uint8_t mode);
		void digitalWrite(uint8_t pin, uint8_t value);
		void analogWrite(uint8_t pin, int value);

		void standardCommand(std::vector<uint8_t> standard_command);
		void sysexCommand(uint8_t sysex_command);
		void sysexCommand(std::vector<uint8_t> sysex_command);

		void reportAnalog(uint8_t channel, uint8_t enable = 1);
		void reportDigital(uint8_t port, uint8_t enable = 1);
		void setSamplingInterval(int intervalms);

	protected:
		virtual bool handleSysex(uint8_t command, std::vector<uint8_t> data);
		virtual bool handleString(std::string);

	private:
		void initPins();
		std::string stringFromBytes(std::vector<uint8_t>::iterator begin, std::vector<uint8_t>::iterator end);

		void analogWriteExtended(uint8_t pin, int value);

		FirmIO* m_firmIO;
		t_pin pins[128];
		bool ready;
	};
}

#endif