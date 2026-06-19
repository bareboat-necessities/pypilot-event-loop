#if !defined(__linux__)
#error "LinuxFifoIpcExample is Linux-only"
#endif

#include <cstdlib>
#include <iostream>
#include <string.h>

#include <pypilot_event_loop.hpp>
#include <pypilot_event_loop_linux/linux_fifo_byte_stream.hpp>

using namespace pypilot_event_loop;

EventLoop<> event_loop;
uint32_t tx_count = 0;

static void fifo_write_text(IByteStream& fifo, const char* text) {
    fifo.write(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

static void fifo_write_uint32(IByteStream& fifo, uint32_t value) {
    char digits[10];
    size_t n = 0;
    do {
        digits[n++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value > 0 && n < sizeof(digits));
    while (n > 0) {
        const char c = digits[--n];
        fifo.write(reinterpret_cast<const uint8_t*>(&c), 1);
    }
}

int main(int argc, char** argv) {
    const char* rx_path = argc > 1 ? argv[1] : "/tmp/pypilot-event-loop-rx.fifo";
    const char* tx_path = argc > 2 ? argv[2] : "/tmp/pypilot-event-loop-tx.fifo";

    LinuxFifoByteStream rx_fifo(rx_path);
    LinuxFifoByteStream tx_fifo(tx_path);

    if (!rx_fifo.valid()) {
        std::cerr << "failed to open rx fifo: " << rx_path << std::endl;
        return 1;
    }
    if (!tx_fifo.valid()) {
        std::cerr << "failed to open tx fifo: " << tx_path << std::endl;
        return 1;
    }

    LineProtocolReader<160> lines(rx_fifo, LineProtocolOptions{}, [&](LineView line) {
        fifo_write_text(tx_fifo, "rx: ");
        if (line.data && line.size > 0) {
            tx_fifo.write(reinterpret_cast<const uint8_t*>(line.data), line.size);
        }
        fifo_write_text(tx_fifo, "\n");
    });

    event_loop.on_bytes_ready(rx_fifo, [&]() {
        lines.poll(event_loop.clock().micros());
    });

    event_loop.on_repeat(1000, [&]() {
        fifo_write_text(tx_fifo, "tx: ");
        fifo_write_uint32(tx_fifo, tx_count++);
        fifo_write_text(tx_fifo, "\n");
    });

    std::cout << "linux fifo ipc example" << std::endl;
    std::cout << "rx fifo: " << rx_fifo.path() << std::endl;
    std::cout << "tx fifo: " << tx_fifo.path() << std::endl;
    std::cout << "write lines into rx fifo and read replies from tx fifo" << std::endl;

    event_loop.run_forever();
    return 0;
}
