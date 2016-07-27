#ifndef		__FIRMIO_H_
#define		__FIRMIO_H_

#include "firmata_constants.h"

#include <cstddef>

namespace firmata {

	class FirmIO{
	public:
		virtual void open() = 0;
		virtual bool isOpen() = 0;
		virtual void close() = 0;
		virtual size_t available() = 0;
		virtual std::vector<uint8_t> read(size_t size = 1) = 0;
		virtual size_t write(std::vector<uint8_t> bytes) = 0;
	};

	class IOException : public std::exception {
	public:
	    const char * what () const throw ()
	    {
	      return "Firmata IO Exception";
	    }
	};

	class NotOpenException : public std::exception {
	public:
	    const char * what () const throw ()
	    {
	      return "Firmata Connection Not Open";
	    }
	};
}

#endif
