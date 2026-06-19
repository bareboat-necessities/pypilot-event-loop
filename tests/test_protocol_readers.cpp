#include <cassert>
#include <cstring>

#include "pypilot_event_loop.hpp"
#include "support/memory_stream.hpp"

static size_t one_byte_length_header(const uint8_t* header, size_t header_size) {
    assert(header_size == 1);
    return header[0];
}

int main() {
    using namespace pypilot_event_loop;
    using pypilot_event_loop_test::MemoryByteStream;

    {
        MemoryByteStream<64> stream;
        int lines = 0;
        char captured[2][8]{};
        size_t captured_sizes[2]{};

        LineProtocolReader<16> reader(stream, LineProtocolOptions{}, [&](LineView line) {
            assert(lines < 2);
            captured_sizes[lines] = line.size;
            memcpy(captured[lines], line.data, line.size);
            captured[lines][line.size] = '\0';
            ++lines;
        });

        const uint8_t data[] = {'o','n','e','\r','\n','t','w','o','\n'};
        assert(stream.write(data, sizeof(data)) == static_cast<int>(sizeof(data)));
        reader.poll(0);

        assert(lines == 2);
        assert(captured_sizes[0] == 3);
        assert(strcmp(captured[0], "one") == 0);
        assert(captured_sizes[1] == 3);
        assert(strcmp(captured[1], "two") == 0);
        assert(reader.stats().messages == 2);
    }

    {
        MemoryByteStream<64> stream;
        int frames = 0;
        uint8_t captured[2][3]{};

        FixedFrameProtocolReader<16> reader(stream, FixedFrameProtocolOptions{3}, [&](FrameView frame) {
            assert(frame.size == 3);
            assert(frames < 2);
            memcpy(captured[frames], frame.data, frame.size);
            ++frames;
        });

        const uint8_t data[] = {1,2,3,4,5,6};
        assert(stream.write(data, sizeof(data)) == static_cast<int>(sizeof(data)));
        reader.poll(0);

        assert(frames == 2);
        assert(captured[0][0] == 1 && captured[0][2] == 3);
        assert(captured[1][0] == 4 && captured[1][2] == 6);
        assert(reader.stats().messages == 2);
    }

    {
        MemoryByteStream<64> stream;
        int frames = 0;
        size_t sizes[2]{};

        HeaderPayloadProtocolReader<16> reader(
            stream,
            HeaderPayloadProtocolOptions{1, 8, one_byte_length_header},
            [&](FrameView frame) {
                assert(frames < 2);
                sizes[frames++] = frame.size;
            });

        const uint8_t data[] = {2, 'o', 'k', 3, 'y', 'e', 's'};
        assert(stream.write(data, sizeof(data)) == static_cast<int>(sizeof(data)));
        reader.poll(0);

        assert(frames == 2);
        assert(sizes[0] == 3);
        assert(sizes[1] == 4);
        assert(reader.stats().messages == 2);
    }

    return 0;
}
