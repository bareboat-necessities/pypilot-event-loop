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
    pypilot_event_loop::LinuxFdStream reader_fd(fds[0]);
    pypilot_event_loop::LinuxFdStream writer_fd(fds[1]);

    int lines = 0;
    pypilot_event_loop::LineProtocolReader<128> line_reader(
        reader_fd,
        pypilot_event_loop::LineProtocolOptions{},
        [&](pypilot_event_loop::LineView line) {
            std::cout << "line: ";
            std::cout.write(line.data, static_cast<std::streamsize>(line.size));
            std::cout << std::endl;
            if (++lines == 2) {
                event_loop.request_exit();
            }
        });

    event_loop.on_bytes_ready(reader_fd, [&]() {
        line_reader.poll(event_loop.clock().micros());
    });

    event_loop.on_delay(0, [&]() {
        const uint8_t msg[] = {'o','n','e','\n','t','w','o','\r','\n'};
        writer_fd.write(msg, sizeof(msg));
    });

    event_loop.run_forever();

    close(fds[0]);
    close(fds[1]);
    return lines == 2 ? 0 : 1;
}
