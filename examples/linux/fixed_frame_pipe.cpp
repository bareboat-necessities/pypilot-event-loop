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

    int frames = 0;
    pypilot_event_loop::FixedFrameProtocolReader<16> frame_reader(
        reader_fd,
        pypilot_event_loop::FixedFrameProtocolOptions{3},
        [&](pypilot_event_loop::FrameView frame) {
            std::cout << "frame size " << frame.size << std::endl;
            if (++frames == 2) {
                event_loop.request_exit();
            }
        });

    event_loop.on_bytes_ready(reader_fd, [&]() {
        frame_reader.poll(event_loop.clock().micros());
    });

    event_loop.on_delay(0, [&]() {
        const uint8_t msg[] = {1, 2, 3, 4, 5, 6};
        writer_fd.write(msg, sizeof(msg));
    });

    event_loop.run_forever();

    close(fds[0]);
    close(fds[1]);
    return frames == 2 ? 0 : 1;
}
