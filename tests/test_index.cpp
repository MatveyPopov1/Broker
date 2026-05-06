#include "doctest.h"
#include "../index/index.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

TEST_CASE("Index basic operations") {
    std::remove("index.idx");

    Index idx;

    SUBCASE("New index has no entries") {
        CHECK(idx.exists(1) == false);
    }

    SUBCASE("Add and retrieve entry") {
        idx.add(42, 1024);
        CHECK(idx.exists(42) == true);

        IndexEntry entry = idx.get(42);
        CHECK(entry.id == 42);
        CHECK(entry.offset == 1024);
        CHECK(entry.deleted == 0);
    }

    SUBCASE("Mark deleted") {
        idx.add(42, 1024);
        idx.markDeleted(42);

        IndexEntry entry = idx.get(42);
        CHECK(entry.deleted == 1);
    }

    SUBCASE("markDeleted on non-existent does nothing") {
        idx.markDeleted(999);
        CHECK(idx.exists(999) == false);
    }

    SUBCASE("Multiple entries") {
        idx.add(1, 100);
        idx.add(2, 200);
        idx.add(3, 300);

        CHECK(idx.exists(1) == true);
        CHECK(idx.exists(2) == true);
        CHECK(idx.exists(3) == true);

        CHECK(idx.get(1).offset == 100);
        CHECK(idx.get(2).offset == 200);
        CHECK(idx.get(3).offset == 300);
    }

    SUBCASE("Get all entries") {
        idx.add(1, 100);
        idx.add(2, 200);
        idx.markDeleted(1);

        auto& all = idx.getAll();
        CHECK(all.size() == 2);
        CHECK(all[1].deleted == 1);
        CHECK(all[2].deleted == 0);
    }

    std::remove("index.idx");
}

TEST_CASE("Index persistence") {
    std::remove("index.idx");

    {
        Index idx;
        idx.add(10, 500);
        idx.add(20, 1000);
        idx.markDeleted(10);
    }

    Index idx2;
    idx2.load();

    CHECK(idx2.exists(10) == true);
    CHECK(idx2.exists(20) == true);

    IndexEntry e10 = idx2.get(10);
    CHECK(e10.offset == 500);
    CHECK(e10.deleted == 1);

    IndexEntry e20 = idx2.get(20);
    CHECK(e20.offset == 1000);
    CHECK(e20.deleted == 0);

    std::remove("index.idx");
}
