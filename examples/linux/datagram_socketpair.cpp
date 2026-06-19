#include <iostream>

#include "pypilot_event_loop.hpp"

int main() {
    pypilot_event_loop::EventLoop<> event_loop;
    pypilot_event_loop::StaticDatagramStream<4, 32> stream;

    int packets_read = 0;

    event_loop.on_delay(0, [&stream]() {
        const uint8_t msg[] = {'d', 'a', 't', 'a'};
        stream.send(msg, sizeof(msg));
    });

    event_loop.on_repeat(1, [&event_loop, &stream, &packets_read]() {
        uint8_t buf[16];
        const int n = stream.recv(buf, sizeof(buf));
        if (n > 0) {
            packets_read++;
            std::cout << "datagram stream read " << n << " bytes" << std::endl;
            event_loop.request_exit();
        }
    });

    event_loop.run_forever();
    return packets_read == 1 ? 0 : 1;
}
