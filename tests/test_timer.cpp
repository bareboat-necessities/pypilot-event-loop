#include <cassert>
#include "async_event_loop.hpp"

int main() {
    using namespace async_event_loop;

    PeriodicTimer periodic(100);
    assert(periodic.ready(0));
    assert(!periodic.ready(50));
    assert(periodic.ready(100));
    assert(periodic.ready(350));
    assert(!periodic.ready(399));
    assert(periodic.ready(400));

    OneShotTimer one;
    one.arm(1000);
    assert(!one.ready(999));
    assert(one.ready(1000));
    assert(!one.ready(1001));
    assert(one.fired());
    assert(!one.armed());

    return 0;
}
