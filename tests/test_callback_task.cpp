#include <cassert>
#include "pypilot_event_loop/callback_task.hpp"

int main() {
    using namespace pypilot_event_loop;

    int count = 0;
    CallbackTask<32> task;
    assert(!task.active());

    task.set([&count]() {
        count++;
    });

    assert(task.active());
    task.poll(100);
    task.poll(200);
    assert(count == 2);

    task.clear();
    assert(!task.active());
    task.poll(300);
    assert(count == 2);

    return 0;
}
