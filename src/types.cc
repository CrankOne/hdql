#include "hdql/types.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

/* TODO: probably, platform-dependant, needs to be conditional? */
#include <cxxabi.h>

extern "C" const char *
hdql_cxx_demangle( const char * mangled
                 , char * buf, size_t buflen
                 ) {
    assert(buf);
    assert(buflen > 4);
    char * buf_ = (char *) malloc(buflen);
    size_t buflen_ = buflen;
    int status = 0;
    char * r = abi::__cxa_demangle(mangled, buf_, &buflen_, &status);
    switch(status) {
        case -1:
            throw std::runtime_error("Memory allocation error during demangling.");
        case -2:
            char errbf[64];
            snprintf( errbf, sizeof(errbf)
                    , "Name \"%s\" is not a valid name under C++ ABI mangling rules."
                    , mangled );
            throw std::runtime_error(errbf);
        case -3:
            throw std::runtime_error("__cxa_demangle() got invalid arguments.");
    };
    assert(r);
    assert(status == 0);
    strncpy(buf, r, buflen);
    if(strlen(r) > buflen) {
        for(int i = 1; i < 4; ++i) {
            buf[buflen - i - 1] = '.';
        }
    }
    free(r);
    buf[buflen-1] = '\0';
    return buf;
}

