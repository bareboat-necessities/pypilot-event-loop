#pragma once

namespace async_event_loop {

enum class Status {
    Ok,
    Full,
    Empty,
    InvalidArgument,
    NotSupported,
    Error
};

} // namespace async_event_loop
