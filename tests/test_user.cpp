#include "doctest.h"
#include "../user/user.h"
#include <cstdio>

TEST_CASE("UserManager operations") {
    std::remove("users.dat");

    UserManager um;

    SUBCASE("Register new user") {
        bool result = um.registerUser("alex", "password123");
        CHECK(result == true);
        CHECK(um.userExists("alex") == true);
    }

    SUBCASE("Register duplicate user fails") {
        um.registerUser("alex", "pass1");
        bool result = um.registerUser("alex", "pass2");
        CHECK(result == false);
    }

    SUBCASE("Authenticate with correct password") {
        um.registerUser("alex", "mypass");
        CHECK(um.authenticate("alex", "mypass") == true);
    }

    SUBCASE("Authenticate with wrong password") {
        um.registerUser("alex", "mypass");
        CHECK(um.authenticate("alex", "wrongpass") == false);
    }

    SUBCASE("Authenticate non-existent user") {
        CHECK(um.authenticate("nobody", "pass") == false);
    }

    SUBCASE("Get all users") {
        um.registerUser("alex", "pass1");
        um.registerUser("boris", "pass2");
        auto users = um.getAllUsers();
        CHECK(users.size() == 2);
    }

    std::remove("users.dat");
}
