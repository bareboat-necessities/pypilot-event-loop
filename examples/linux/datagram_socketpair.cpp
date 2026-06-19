#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fd_datagram_stream.hpp"

class DatagramReadTask final : public pypilot_event_loop::IRuntimeTask {
public:
    explicit DatagramReadTask(pypilot_event_loop::LinuxFdDatagramStream& stream) : stream_(stream) {}

    void poll(uint64_t) override {
        uint8_t buf[32];
        const int n = stream_.recv(buf, sizeof(buf));
        if (n > 0) {
            std::cout << "datagram " << n << " bytes: ";
            for (int i = 0; i < n; ++i) {
                std::cout << static_cast<char>(buf[i]);
            }
            std::cout << std::endl;
        }
    }

private:
    pypilot_event_loop::LinuxFdDatagramStream& stream_;
};

int main() {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) {
        return 1;
    }

    pypilot_event_loop::EventLoop<> event_loop;
    if (!event_loop.valid()) {
        return 2;
    }

    pypilot_event_loop::LinuxFdDatagramStream receiver(fds[0]);
    pypilot_event_loop::LinuxFdDatagramStream sender(fds[1]);
    DatagramReadTask read_task(receiver);

    if (!event_loop.scheduler().add_readable_fd(fds[0], read_task)) {
        return 3;
    }

    const uint8_t msg[] = {'u', 'd', 'p'};
    sender.send(msg, sizeof(msg));
    event_loop.tick();

    close(fds[0]);
    close(fds[1]);
    return 0;
}
