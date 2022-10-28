#pragma once

#include "domain.h"

class Serialization {
    static const std::string& GetFilePath();

public:
    static bool Read(domain::BUSES & busses, domain::STOPS & stops);
    static void Write(const domain::BUSES & busses, const domain::STOPS & stops);
};
