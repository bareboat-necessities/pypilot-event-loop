#include <iostream>
#include <unistd.h>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fd_stream.hpp"

class PipeReadTask final : public pypilot_event_loop::IRuntimeTask {
public:
    explicit PipeReadTask(pypilot_event_loop::LinuxFdStream& stream) : stream_(stream) {}

    void poll(uint64_t) override {
        uint8_t buf[32];
        const int n = stream_.read(buf, sizeof(buf));
        if (n > 0) {
            std::cout << "read " << n << " bytes: ";
            for (int i = 0; i < n; ++i) {
                std::cout << static_cast<char>(buf[i]);
            }
            std::cout << std::endl;
        }
    }

private:
    pypilot_event_loop::LinuxFdStream& stream_;
};

int main() {
    int fds[2];
    if (pipe(fds) != 0) {
        return 1;
    }

    pypilot_event_loop::EventLoop<> event_loop;
    if (!event_loop.valid()) {
        return 2;
    }

    pypilot_event_loop::LinuxFdStream reader(fds[0]);
    pypilot_event_loop::LinuxFdStream writer(fds[1]);
    PipeReadTask read_task(reader);

    if (!event_loop.scheduler().add_readable_fd(fds[0], read_task)) {
        return 3;
    }

    const uint8_t msg[] = {'h', 'e', 'l', 'l', 'o'};
    writer.write(msg, sizeof(msg));
    event_loop.tick();

    close(fds[0]);
    close(fds[1]);
    return 0;
}
