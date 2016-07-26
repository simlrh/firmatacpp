#ifndef		__FIRMATA_H_
#define		__FIRMATA_H_

#include "firmatacpp_export.h"
#include "firmio.h"
#include "firmbase.h"
#include "firmi2c.h"

namespace FIRMATACPP_EXPORT firmata {

	template< class ... Extensions >
	class Firmata : virtual public Extensions...
	{
	public:
		Firmata(FirmIO* firmIO) : Extensions(firmIO)... {};
		virtual ~Firmata() {};

	protected:
		virtual bool handleSysex(uint8_t command, std::vector<uint8_t> data) override
		{
			bool handled[] = { (Extensions::handleSysex(command, data))... };

			for (bool h : handled) {
				if (h) return true;
			}

			return false;
		}
		virtual bool handleString(std::string data) override
		{
			bool handled[] = { (Extensions::handleString(data))... };

			for (bool h : handled) {
				if (h) return true;
			}

			return false;
		}
	};
}

#endif