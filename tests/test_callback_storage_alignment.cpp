#include "pypilot_event_loop.hpp"

#include <cassert>

struct AlignedCallable {
    long long padding;
    int* value;
    void operator()() const { ++(*value); }
};

int main() {
    int calls = 0;
    pypilot_event_loop::CallbackTask<64> task;
    assert(task.set(AlignedCallable{0, &calls}));
    task.poll(0);
    assert(calls == 1);
    return 0;
}
