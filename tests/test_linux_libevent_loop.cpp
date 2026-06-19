#include <cassert>
#include <unistd.h>

#include "pypilot_event_loop_linux/libevent_loop.hpp"
#include "pypilot_event_loop_linux/linux_fd_stream.hpp"
#include "pypilot_event_loop_linux/monotonic_clock.hpp"

class PipeReadTask final : public pypilot_event_loop::IRuntimeTask {
public:
    explicit PipeReadTask(pypilot_event_loop::LinuxFdStream& stream) : stream_(stream) {}

    void poll(uint64_t now_us) override {
        uint8_t buf[8];
        int n = stream_.read(buf, sizeof(buf));
        if (n > 0) {
            bytes += n;
            last_us = now_us;
        }
    }

    pypilot_event_loop::LinuxFdStream& stream_;
    int bytes = 0;
    uint64_t last_us = 0;
};

class CountTask final : public pypilot_event_loop::IRuntimeTask {
public:
    void poll(uint64_t now_us) override {
        count++;
        last_us = now_us;
    }
    int count = 0;
    uint64_t last_us = 0;
};

int main() {
    using namespace pypilot_event_loop;

    LinuxMonotonicClock clock;
    LinuxLibeventLoop loop(clock);
    assert(loop.valid());

    int fds[2];
    assert(pipe(fds) == 0);

    LinuxFdStream read_stream(fds[0]);
    PipeReadTask read_task(read_stream);
    assert(loop.add_readable_fd(fds[0], read_task));

    const uint8_t msg[3] = {1, 2, 3};
    assert(::write(fds[1], msg, sizeof(msg)) == 3);
    loop.run_once();
    assert(read_task.bytes == 3);

    CountTask one;
    assert(loop.add_one_shot(one, clock.micros()));
    loop.run_once();
    assert(one.count == 1);
    loop.run_once();
    assert(one.count == 1);

    close(fds[0]);
    close(fds[1]);
    return 0;
}
