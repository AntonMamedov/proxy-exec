#include "gtest/gtest.h"

extern "C" {
#include "store.h"
#include "../src/store.c"
}

TEST(PecStoreMap_test, init_test) {
    pec_store_map_t store;
}
