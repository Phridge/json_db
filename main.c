#include <stdio.h>
#include "jsondb.h"

int main(void) {
    struct jsondb_set search = {0}, reduced = {0};

    jsondb_init();

    jsondb_set_add(&search, "10");
    jsondb_set_add(&search, "103463456");
    jsondb_set_add(&search, "11");
    jsondb_set_add(&search, "14");
    jsondb_set_add(&search, "4");
    jsondb_set_add(&search, "0");
    jsondb_set_add(&search, "9");
    jsondb_set_add(&search, "1");


    jsondb_set_sort(&search);

    jsondb_deinit();
    return 0;
}

