#pragma once

namespace pypilot_event_loop {

enum class Status {
    Ok,
    Full,
    Empty,
    InvalidArgument,
    NotSupported,
    Error
};

} // namespace pypilot_event_loop
