#include <stdio.h>
#include "mmlog.h"

/* The ESP32 shim omits this symbol. Keep diagnostic metadata while excluding
 * packet contents and possible credentials from the serial log. */
void mm_hexdump(char level, const char *function, unsigned line_number,
                const char *title, const uint8_t *buf, size_t len)
{
    (void)buf;
    printf("%c %s:%u %s [%u bytes omitted]\n", level, function, line_number,
           title, (unsigned)len);
}
