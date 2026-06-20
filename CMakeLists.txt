cmake_minimum_required(VERSION 3.16)
project(pypilot_event_loop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(LIBGPIOD2 libgpiod>=2.0)

add_library(pypilot_event_loop INTERFACE)
target_include_directories(pypilot_event_loop INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBEVENT_INCLUDE_DIRS}
)
target_link_directories(pypilot_event_loop INTERFACE ${LIBEVENT_LIBRARY_DIRS})
target_link_libraries(pypilot_event_loop INTERFACE ${LIBEVENT_LIBRARIES})
target_compile_options(pypilot_event_loop INTERFACE ${LIBEVENT_CFLAGS_OTHER})

if(LIBGPIOD2_FOUND)
    target_include_directories(pypilot_event_loop INTERFACE ${LIBGPIOD2_INCLUDE_DIRS})
    target_link_directories(pypilot_event_loop INTERFACE ${LIBGPIOD2_LIBRARY_DIRS})
    target_link_libraries(pypilot_event_loop INTERFACE ${LIBGPIOD2_LIBRARIES})
    target_compile_options(pypilot_event_loop INTERFACE ${LIBGPIOD2_CFLAGS_OTHER})
endif()

enable_testing()

function(add_linux_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE pypilot_event_loop)
endfunction()

function(add_unit_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE pypilot_event_loop)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_unit_test(test_timer tests/test_timer.cpp)
add_unit_test(test_test_scheduler tests/test_test_scheduler.cpp)
add_unit_test(test_memory_stream tests/test_memory_stream.cpp)
add_unit_test(test_callback_task tests/test_callback_task.cpp)
add_unit_test(test_bytes_ready_callback tests/test_bytes_ready_callback.cpp)
add_unit_test(test_event_loop_remove_reuses_slot tests/test_event_loop_remove_reuses_slot.cpp)
add_unit_test(test_event_loop_remove_inside_callback tests/test_event_loop_remove_inside_callback.cpp)
add_unit_test(test_protocol_readers tests/test_protocol_readers.cpp)
add_unit_test(test_linux_fifo_stream tests/test_linux_fifo_stream.cpp)
add_unit_test(test_linux_libevent_loop tests/test_linux_libevent_loop.cpp)
add_unit_test(test_tcp_line_server tests/test_tcp_line_server.cpp)
add_unit_test(test_tcp_fixed_header_server tests/test_tcp_fixed_header_server.cpp)
add_unit_test(test_tcp_server_shutdown tests/test_tcp_server_shutdown.cpp)
add_unit_test(test_tcp_disconnect_detection tests/test_tcp_disconnect_detection.cpp)
add_unit_test(test_tcp_client_disconnect tests/test_tcp_client_disconnect.cpp)
add_unit_test(test_tcp_listener_lifecycle tests/test_tcp_listener_lifecycle.cpp)
add_unit_test(test_tcp_close_closes_clients tests/test_tcp_close_closes_clients.cpp)

if(LIBGPIOD2_FOUND)
    add_unit_test(test_linux_gpiod_pin_io_compile tests/test_linux_gpiod_pin_io_compile.cpp)
endif()

add_linux_example(event_loop_timer_example examples/LinuxPortable/EventLoopTimerExample.cpp)
add_linux_example(line_protocol_example examples/LinuxPortable/LineProtocolExample.cpp)
add_linux_example(fixed_header_protocol_example examples/LinuxPortable/FixedHeaderProtocolExample.cpp)
add_linux_example(serial_line_protocol_example examples/LinuxPortable/SerialLineProtocolExample.cpp)
add_linux_example(tcp_line_client_example examples/LinuxPortable/TcpLineClientExample.cpp)
add_linux_example(udp_broadcast_sender_example examples/LinuxPortable/UdpBroadcastSenderExample.cpp)
add_linux_example(udp_receiver_example examples/LinuxPortable/UdpReceiverExample.cpp)
add_linux_example(pin_io_example examples/LinuxPortable/PinIoExample.cpp)
add_linux_example(integrated_event_loop_example examples/LinuxPortable/IntegratedEventLoopExample.cpp)
add_linux_example(pin_event_example examples/LinuxPortable/PinEventExample.cpp)

add_executable(tcp_line_server_example examples/TcpLineServerExample/TcpLineServerExample.cpp)
target_link_libraries(tcp_line_server_example PRIVATE pypilot_event_loop)

add_executable(tcp_fixed_header_server_example examples/TcpFixedHeaderServerExample/TcpFixedHeaderServerExample.cpp)
target_link_libraries(tcp_fixed_header_server_example PRIVATE pypilot_event_loop)

add_executable(tcp_disconnect_server_example examples/TcpDisconnectServerExample/TcpDisconnectServerExample.cpp)
target_link_libraries(tcp_disconnect_server_example PRIVATE pypilot_event_loop)

add_executable(linux_fifo_ipc_example examples/LinuxFifoIpcExample/LinuxFifoIpcExample.cpp)
target_link_libraries(linux_fifo_ipc_example PRIVATE pypilot_event_loop)
