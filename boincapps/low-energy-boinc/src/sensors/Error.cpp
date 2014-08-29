#include "Error.hpp"

Error::Error(std::string module, int code, std::string text) {
        m_module = module;
        m_code = code;
        m_text = text;
}

