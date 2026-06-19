#include <iostream>
#include <unistd.h>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fd_stream.hpp"

int main() {
    int fds[2];
    if (pipe(fds) != 0) {
        return 1;
    }

    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::LinuxFdStream reader(fds[0]);
    pypilot_event_loop::LinuxFdStream writer(fds[1]);

    int bytes_read = 0;

    const pypilot_event_loop::EventHandle reader_event = event_loop.on_bytes_ready(reader, [&]() {
        uint8_t buf[16];
        const int n = reader.read(buf, sizeof(buf));
        if (n > 0) {
            bytes_read += n;
            std::cout << "byte stream data ready read " << n << " bytes" << std::endl;
            event_loop.request_exit();
        }
    });
    if (!event_loop.valid(reader_event)) {
        return 2;
    }

    event_loop.on_delay(0, [&]() {
        const uint8_t msg[] = {'h', 'e', 'l', 'l', 'o'};
        writer.write(msg, sizeof(msg));
    });

    event_loop.run_forever();

    close(fds[0]);
    close(fds[1]);
    return bytes_read == 5 ? 0 : 1;
}
