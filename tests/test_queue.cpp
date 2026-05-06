#include "doctest.h"
#include "../queue/queue.h"

TEST_CASE("MessageQueue priority ordering") {
    MessageQueue mq;

    Message m1 = {0, "test", "low", 1};
    Message m2 = {0, "test", "medium", 5};
    Message m3 = {0, "test", "high", 10};

    mq.push(m1);
    mq.push(m3);
    mq.push(m2);

    Message first = mq.pop();
    CHECK(first.priority == 10);

    Message second = mq.pop();
    CHECK(second.priority == 5);

    Message third = mq.pop();
    CHECK(third.priority == 1);
}
