#include <stdio.h>
#include "jsondb.h"

int main(void) {
    struct jsondb_set selection;

    jsondb_init();

    jsondb_insert("{\"Hello\": [1,2,-3.45e-8,true,false,null, {}, [], \"\\n\"]}", NULL);

    selection = jsondb_select("/Hello/1");

    jsondb_deinit();
    return 0;
}
