#include "lib/vadpcm/vadpcm.h"

const char *vadpcm_error_name(vadpcm_error err) {
    switch (err) {
    case kVADPCMErrNone:
        return "no error";
    case kVADPCMErrNoMemory:
        return "out of memory";
    case kVADPCMErrInvalidData:
        return "invalid data";
    case kVADPCMErrLargeOrder:
        return "predictor order too large";
    case kVADPCMErrUnknownVersion:
        return "unknnown VADPCM version";
    }
    return 0;
}
