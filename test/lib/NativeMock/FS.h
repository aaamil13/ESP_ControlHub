#ifndef FS_H
#define FS_H

#include <Arduino.h>

namespace fs {
    class FS {
    public:
        bool begin(bool formatOnFail = false, const char * basePath = "/littlefs", uint8_t maxOpenFiles = 10) { return true; }
    };
}

using namespace fs;

#endif
