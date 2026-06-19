#include <iostream>

#include "pypilot_event_loop.hpp"

int main() {
    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::StaticByteStream<64> stream;

    int bytes_read = 0;

    event_loop.on_delay(0, [&stream]() {
        const uint8_t msg[] = {'h', 'e', 'l', 'l', 'o'};
        stream.write(msg, sizeof(msg));
    });

    event_loop.on_repeat(1, [&event_loop, &stream, &bytes_read]() {
        uint8_t buf[16];
        const int n = stream.read(buf, sizeof(buf));
        if (n > 0) {
            bytes_read += n;
            std::cout << "byte stream read " << n << " bytes" << std::endl;
            event_loop.request_exit();
        }
    });

    event_loop.run_forever();
    return bytes_read == 5 ? 0 : 1;
}
