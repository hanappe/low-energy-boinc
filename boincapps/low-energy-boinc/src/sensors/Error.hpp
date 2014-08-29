#ifndef __Error_hpp__
#define __Error_hpp__

#include <string>
#include <vector>

struct Error {
        Error(std::string module, int code, std::string text);

        int m_code;
        std::string m_module;
        std::string m_text;
};

typedef std::vector<Error> ErrorV;

#endif
