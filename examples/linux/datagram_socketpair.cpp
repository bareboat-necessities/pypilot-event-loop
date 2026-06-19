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

    event_loop.on_readable(receiver, [&]() {
        uint8_t buf[16];
        const int n = receiver.recv(buf, sizeof(buf));
        if (n > 0) {
            packets_read++;
            std::cout << "datagram readiness read " << n << " bytes" << std::endl;
            event_loop.request_exit();
        }
    });

    event_loop.on_delay(0, [&]() {
        const uint8_t msg[] = {'d', 'a', 't', 'a'};
        sender.send(msg, sizeof(msg));
    });

    event_loop.run_forever();

    close(fds[0]);
    close(fds[1]);
    return packets_read == 1 ? 0 : 1;
}
