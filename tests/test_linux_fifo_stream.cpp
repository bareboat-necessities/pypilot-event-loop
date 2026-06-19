#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fifo_byte_stream.hpp"

int main() {
    char path[160];
    std::snprintf(path, sizeof(path), "/tmp/pypilot-event-loop-fifo-%ld", static_cast<long>(getpid()));
    unlink(path);

    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::LinuxFifoByteStream fifo(path);
    assert(fifo.valid());
    assert(fifo.native_fd() >= 0);

    int lines = 0;
    pypilot_event_loop::LineProtocolReader<64> reader(
        fifo,
        pypilot_event_loop::LineProtocolOptions{},
        [&](pypilot_event_loop::LineView line) {
            (void)line;
            ++lines;
        });

    const pypilot_event_loop::EventHandle handle = event_loop.on_bytes_ready(fifo, [&]() {
        reader.poll(event_loop.clock().micros());
    });
    assert(event_loop.valid(handle));

    int writer = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    assert(writer >= 0);
    assert(write(writer, "one\n", 4) == 4);

    for (int i = 0; i < 20 && lines < 1; ++i) {
        event_loop.run_once();
    }
    assert(lines == 1);
    close(writer);

    writer = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    assert(writer >= 0);
    assert(write(writer, "two\n", 4) == 4);

    for (int i = 0; i < 20 && lines < 2; ++i) {
        event_loop.run_once();
    }
    assert(lines == 2);
    close(writer);

    assert(event_loop.remove(handle));
    unlink(path);
    return 0;
}
