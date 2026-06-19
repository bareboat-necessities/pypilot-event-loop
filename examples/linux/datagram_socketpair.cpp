#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fd_datagram_stream.hpp"

int main() {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) != 0) {
        return 1;
    }

    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::LinuxFdDatagramStream receiver(fds[0]);
    pypilot_event_loop::LinuxFdDatagramStream sender(fds[1]);

    int packets_read = 0;

    const pypilot_event_loop::EventHandle receiver_event = event_loop.on_bytes_ready(receiver, [&]() {
        uint8_t buf[16];
        const int n = receiver.recv(buf, sizeof(buf));
        if (n > 0) {
            packets_read++;
            std::cout << "datagram data ready read " << n << " bytes" << std::endl;
            event_loop.request_exit();
        }
    });
    if (!event_loop.valid(receiver_event)) {
        return 2;
    }

    event_loop.on_delay(0, [&]() {
        const uint8_t msg[] = {'d', 'a', 't', 'a'};
        sender.send(msg, sizeof(msg));
    });

    event_loop.run_forever();

    close(fds[0]);
    close(fds[1]);
    return packets_read == 1 ? 0 : 1;
}
