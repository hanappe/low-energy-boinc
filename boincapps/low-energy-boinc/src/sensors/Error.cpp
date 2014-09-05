#include "Error.hpp"

Error::Error(std::string module, int code, std::string text) {
        m_module = module;
        m_code = code;
        m_text = text;
}

std::ostream& operator<<(std::ostream& os, const Error& e)
{
	os << e.m_module << ","
		<< e.m_code << ","
		<< e.m_text;

	return os;
}

