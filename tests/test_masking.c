#include <stdlib.h>

#include "signal_masking.h"

int main(void)
{
    int result = demo_signal_masking();

    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

