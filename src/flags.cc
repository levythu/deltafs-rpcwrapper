#include <gflags/gflags.h>

#include "flags.h"

DEFINE_bool(readMode, false, "Define the access pattern to deltafs");
DEFINE_bool(conversion, false, "Convert from writeMode to readMode");
