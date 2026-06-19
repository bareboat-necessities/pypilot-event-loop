cmake_minimum_required(VERSION 3.16)
project(pypilot_event_loop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(LIBGPIOD REQUIRED libgpiod)

add_library(pypilot_event_loop INTERFACE)
target_include_directories(pypilot_event_loop INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBEVENT_INCLUDE_DIRS}
    ${LIBGPIOD_INCLUDE_DIRS}
)
target_link_directories(pypilot_event_loop INTERFACE ${LIBEVENT_LIBRARY_DIRS} ${LIBGPIOD_LIBRARY_DIRS})
target_link_libraries(pypilot_event_loop INTERFACE ${LIBEVENT_LIBRARIES} ${LIBGPIOD_LIBRARIES})
target_compile_options(pypilot_event_loop INTERFACE ${LIBEVENT_CFLAGS_OTHER} ${LIBGPIOD_CFLAGS_OTHER})

add_executable(test_event_queue tests/test_event_queue.cpp)
target_link_libraries(test_event_queue PRIVATE pypilot_event_loop)

add_executable(test_spsc_event_queue tests/test_spsc_event_queue.cpp)
target_link_libraries(test_spsc_event_queue PRIVATE pypilot_event_loop)

add_executable(test_timer tests/test_timer.cpp)
target_link_libraries(test_timer PRIVATE pypilot_event_loop)

add_executable(test_test_scheduler tests/test_test_scheduler.cpp)
target_link_libraries(test_test_scheduler PRIVATE pypilot_event_loop)

add_executable(test_memory_stream tests/test_memory_stream.cpp)
target_link_libraries(test_memory_stream PRIVATE pypilot_event_loop)

add_executable(test_callback_task tests/test_callback_task.cpp)
target_link_libraries(test_callback_task PRIVATE pypilot_event_loop)

add_executable(test_pin_event_task tests/test_pin_event_task.cpp)
target_link_libraries(test_pin_event_task PRIVATE pypilot_event_loop)

add_executable(test_readable_callback tests/test_readable_callback.cpp)
target_link_libraries(test_readable_callback PRIVATE pypilot_event_loop)

add_executable(test_protocol_readers tests/test_protocol_readers.cpp)
target_link_libraries(test_protocol_readers PRIVATE pypilot_event_loop)

add_executable(test_linux_fifo_stream tests/test_linux_fifo_stream.cpp)
target_link_libraries(test_linux_fifo_stream PRIVATE pypilot_event_loop)

add_executable(test_linux_libevent_loop tests/test_linux_libevent_loop.cpp)
target_link_libraries(test_linux_libevent_loop PRIVATE pypilot_event_loop)

add_executable(libevent_loop_smoke examples/linux/libevent_loop_smoke.cpp)
target_link_libraries(libevent_loop_smoke PRIVATE pypilot_event_loop)

add_executable(byte_stream_pipe examples/linux/byte_stream_pipe.cpp)
target_link_libraries(byte_stream_pipe PRIVATE pypilot_event_loop)

add_executable(datagram_socketpair examples/linux/datagram_socketpair.cpp)
target_link_libraries(datagram_socketpair PRIVATE pypilot_event_loop)

add_executable(line_protocol_pipe examples/linux/line_protocol_pipe.cpp)
target_link_libraries(line_protocol_pipe PRIVATE pypilot_event_loop)

add_executable(fixed_frame_pipe examples/linux/fixed_frame_pipe.cpp)
target_link_libraries(fixed_frame_pipe PRIVATE pypilot_event_loop)

add_executable(fifo_line_reader examples/linux/fifo_line_reader.cpp)
target_link_libraries(fifo_line_reader PRIVATE pypilot_event_loop)

add_executable(gpiod_pin_event examples/linux/gpiod_pin_event.cpp)
target_link_libraries(gpiod_pin_event PRIVATE pypilot_event_loop)

enable_testing()
add_test(NAME test_event_queue COMMAND test_event_queue)
add_test(NAME test_spsc_event_queue COMMAND test_spsc_event_queue)
add_test(NAME test_timer COMMAND test_timer)
add_test(NAME test_test_scheduler COMMAND test_test_scheduler)
add_test(NAME test_memory_stream COMMAND test_memory_stream)
add_test(NAME test_callback_task COMMAND test_callback_task)
add_test(NAME test_pin_event_task COMMAND test_pin_event_task)
add_test(NAME test_readable_callback COMMAND test_readable_callback)
add_test(NAME test_protocol_readers COMMAND test_protocol_readers)
add_test(NAME test_linux_fifo_stream COMMAND test_linux_fifo_stream)
add_test(NAME test_linux_libevent_loop COMMAND test_linux_libevent_loop)
