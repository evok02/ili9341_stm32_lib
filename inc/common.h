#ifndef INC_COMMON_H
#define INC_COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define BPOINT() { \
    __asm__("BKPT #0"); }

#endif
