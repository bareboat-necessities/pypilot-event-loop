#include <cassert>
#include <cstring>

#include "async_event_loop.hpp"

static size_t one_byte_length_header(const uint8_t* header, size_t header_size) {
    assert(header_size == 1);
    return header[0];
}

int main() {
    using namespace async_event_loop;

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
        int lines = 0;
        char captured[2][16]{};

        LineProtocolReader<16> reader(stream, LineProtocolOptions{}, [&](LineView line) {
            assert(lines < 2);
            memcpy(captured[lines], line.data, line.size);
            captured[lines][line.size] = '\0';
            ++lines;
        });

        const uint8_t part1[] = {'p','a','r','t'};
        const uint8_t part2[] = {'i','a','l','\n','n','e','x','t','\n'};
        assert(stream.write(part1, sizeof(part1)) == static_cast<int>(sizeof(part1)));
        reader.poll(0);
        assert(lines == 0);
        assert(reader.stats().messages == 0);

        assert(stream.write(part2, sizeof(part2)) == static_cast<int>(sizeof(part2)));
        reader.poll(0);
        assert(lines == 2);
        assert(strcmp(captured[0], "partial") == 0);
        assert(strcmp(captured[1], "next") == 0);
    }

    {
        MemoryByteStream<64> stream;
        int lines = 0;
        size_t sizes[2]{};

        LineProtocolReader<4> reader(stream, LineProtocolOptions{}, [&](LineView line) {
            assert(lines < 2);
            sizes[lines++] = line.size;
        });

        const uint8_t data[] = {'a','b','c','d','e','\n','o','k','\n'};
        assert(stream.write(data, sizeof(data)) == static_cast<int>(sizeof(data)));
        reader.poll(0);

        assert(reader.stats().overflows == 1);
        assert(lines == 2);
        assert(sizes[0] == 0);
        assert(sizes[1] == 2);
    }

    {
        MemoryByteStream<64> stream;
        int lines = 0;
        char captured[2][8]{};

        LineProtocolReader<4> reader(stream, LineProtocolOptions{'\n', true, false}, [&](LineView line) {
            assert(lines < 2);
            memcpy(captured[lines], line.data, line.size);
            captured[lines][line.size] = '\0';
            ++lines;
        });

        const uint8_t data[] = {'a','b','c','d','e','f','\n','o','k','\n'};
        assert(stream.write(data, sizeof(data)) == static_cast<int>(sizeof(data)));
        reader.poll(0);

        assert(reader.stats().overflows == 2);
        assert(lines == 2);
        assert(strcmp(captured[0], "abcd") == 0);
        assert(strcmp(captured[1], "ok") == 0);
    }

    {
        MemoryByteStream<128> stream;
        int messages = 0;
        char captured[3][48]{};

        JsonProtocolReader<64> reader(stream, JsonProtocolOptions{}, [&](JsonView json) {
            assert(messages < 3);
            memcpy(captured[messages], json.data, json.size);
            captured[messages][json.size] = '\0';
            ++messages;
        });

        const char data[] = "  {\"a\":1} [true,false,{\"s\":\"brace } in string\"}]\n\"ok\"";
        assert(stream.write(reinterpret_cast<const uint8_t*>(data), sizeof(data) - 1) == static_cast<int>(sizeof(data) - 1));
        reader.poll(0);

        assert(messages == 3);
        assert(strcmp(captured[0], "{\"a\":1}") == 0);
        assert(strcmp(captured[1], "[true,false,{\"s\":\"brace } in string\"}]") == 0);
        assert(strcmp(captured[2], "\"ok\"") == 0);
        assert(reader.stats().messages == 3);
    }

    {
        MemoryByteStream<128> stream;
        int messages = 0;
        char captured[2][32]{};

        JsonProtocolReader<64> reader(stream, JsonProtocolOptions{}, [&](JsonView json) {
            assert(messages < 2);
            memcpy(captured[messages], json.data, json.size);
            captured[messages][json.size] = '\0';
            ++messages;
        });

        const char part1[] = "{\"split\":";
        const char part2[] = "[1,2,3]} null";
        assert(stream.write(reinterpret_cast<const uint8_t*>(part1), sizeof(part1) - 1) == static_cast<int>(sizeof(part1) - 1));
        reader.poll(0);
        assert(messages == 0);

        assert(stream.write(reinterpret_cast<const uint8_t*>(part2), sizeof(part2) - 1) == static_cast<int>(sizeof(part2) - 1));
        reader.poll(0);
        assert(messages == 2);
        assert(strcmp(captured[0], "{\"split\":[1,2,3]}") == 0);
        assert(strcmp(captured[1], "null") == 0);
    }

    {
        MemoryByteStream<128> stream;
        int messages = 0;
        char captured[2][16]{};

        JsonProtocolReader<64> reader(stream, JsonProtocolOptions{}, [&](JsonView json) {
            assert(messages < 2);
            memcpy(captured[messages], json.data, json.size);
            captured[messages][json.size] = '\0';
            ++messages;
        });

        const char data[] = "123 false";
        assert(stream.write(reinterpret_cast<const uint8_t*>(data), sizeof(data) - 1) == static_cast<int>(sizeof(data) - 1));
        reader.poll(0);
        assert(messages == 2);
        assert(strcmp(captured[0], "123") == 0);
        assert(strcmp(captured[1], "false") == 0);
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
