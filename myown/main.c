#include "wordmate.h"
#include <stdio.h>

int main(void)
{
    if (!ensure_data_ready()) {
        fprintf(stderr, "Could not prepare dictionary files.\n");
        return 1;
    }

    run_menu();

    return 0;
}