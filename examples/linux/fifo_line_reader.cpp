#include <iostream>

#include "pypilot_event_loop.hpp"
#include "pypilot_event_loop_linux/linux_fifo_byte_stream.hpp"

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/tmp/pypilot-event-loop.fifo";

    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::LinuxFifoByteStream fifo(path);
    if (!event_loop.valid() || !fifo.valid()) {
        return 1;
    }

    pypilot_event_loop::LineProtocolReader<256> lines(
        fifo,
        pypilot_event_loop::LineProtocolOptions{},
        [](pypilot_event_loop::LineView line) {
            std::cout << "fifo line: ";
            std::cout.write(line.data, static_cast<std::streamsize>(line.size));
            std::cout << std::endl;
        });

    event_loop.on_bytes_ready(fifo, [&]() {
        lines.poll(event_loop.clock().micros());
    });

    std::cout << "reading FIFO " << path << std::endl;
    event_loop.run_forever();
    return 0;
}
