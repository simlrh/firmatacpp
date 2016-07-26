#ifndef __FIRMEXTENSION_H__
#define __FIRMEXTENSION_H__

class Extension {
protected:
	virtual bool handleSysex(uint8_t command, std::vector<uint8_t> data) = 0;
	virtual bool handleString(std::string) = 0;
};

#endif