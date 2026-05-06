#include "doctest.h"
#include "../offset/offset.h"
#include <cstdio>

TEST_CASE("OffsetManager basic operations") {
    std::remove("offsets.dat");

    OffsetManager om;

    SUBCASE("Get returns 0 for new consumer") {
        uint64_t offset = om.get("unknown_consumer");
        CHECK(offset == 0);
    }

    SUBCASE("Set and get offset") {
        om.set("alex", 42);
        uint64_t offset = om.get("alex");
        CHECK(offset == 42);
    }

    SUBCASE("Multiple consumers have independent offsets") {
        om.set("alex", 10);
        om.set("boris", 20);
        CHECK(om.get("alex") == 10);
        CHECK(om.get("boris") == 20);
    }

    SUBCASE("Update existing offset") {
        om.set("alex", 5);
        om.set("alex", 15);
        CHECK(om.get("alex") == 15);
    }

    SUBCASE("Load from file restores data") {
        om.set("alex", 100);
        om.set("boris", 200);

        OffsetManager om2;
        om2.load();
        CHECK(om2.get("alex") == 100);
        CHECK(om2.get("boris") == 200);
    }

    std::remove("offsets.dat");
}
