#include <cassert>
#include <unistd.h>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fd_stream.hpp"

int main() {
    int fds[2];
    assert(pipe(fds) == 0);

    pypilot_event_loop::EventLoop<> event_loop;
    assert(event_loop.valid());

    pypilot_event_loop::LinuxFdStream reader(fds[0]);
    pypilot_event_loop::LinuxFdStream writer(fds[1]);

    int bytes_read = 0;
    const pypilot_event_loop::EventHandle handle = event_loop.on_bytes_ready(reader, [&]() {
        uint8_t buf[8];
        const int n = reader.read(buf, sizeof(buf));
        if (n > 0) {
            bytes_read += n;
        }
    });
    assert(event_loop.valid(handle));

    assert(event_loop.disable(handle));
    assert(event_loop.enable(handle));

    const uint8_t msg[] = {'o', 'k'};
    assert(writer.write(msg, sizeof(msg)) == 2);

    for (int i = 0; i < 10 && bytes_read == 0; ++i) {
        event_loop.run_once();
    }

    assert(event_loop.remove(handle));
    assert(!event_loop.valid(handle));

    close(fds[0]);
    close(fds[1]);

    assert(bytes_read == 2);
    return 0;
}
