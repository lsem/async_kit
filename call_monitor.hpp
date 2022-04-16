#pragma once

#include <chrono>
#include <functional>
#include <string>

namespace call_monitor {

void start(std::function<void(std::string)> log_fn);
void stop();

void sync_call(std::function<void()> callable, std::string call_id, std::chrono::steady_clock::duration t);

}  // namespace call_monitor
