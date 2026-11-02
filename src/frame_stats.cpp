#include "frame_stats.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <rex/cvar.h>
REXCVAR_DEFINE_UINT32(wo2_fps_limit, 60, "WO2", "Maximum guest FPS; 0 disables pacing (restart required)");
extern "C" void wo2_trace_queue_latency(unsigned kind,unsigned detail,unsigned value);
static std::atomic<uint64_t> frames{0};
void wo2_count_frame() {
  using Clock = std::chrono::steady_clock;
  static const unsigned limit = REXCVAR_GET(wo2_fps_limit);
  if (limit) {
    const auto interval = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / limit));
    static thread_local auto next = Clock::now();
    auto now = Clock::now();
    if (now < next) {
      std::this_thread::sleep_until(next);
      now = Clock::now();
    }
    // Slow stages and pauses must not cause a burst of catch-up frames.
    next = now + interval;
  }
  const auto count = frames.fetch_add(1, std::memory_order_relaxed) + 1;
  wo2_trace_queue_latency(25, unsigned(count), 0);
}
rex::ui::FrameStats wo2_frame_stats() {
  using Clock = std::chrono::steady_clock;
  static auto last_time = Clock::now();
  static uint64_t last_count = 0;
  static rex::ui::FrameStats stats;
  const auto now = Clock::now();
  const auto count = frames.load(std::memory_order_relaxed);
  const double seconds = std::chrono::duration<double>(now - last_time).count();
  if (seconds >= 0.5) {
    stats.fps = double(count - last_count) / seconds;
    stats.frame_time_ms = stats.fps > 0 ? 1000.0 / stats.fps : 0;
    last_count = count;
    last_time = now;
  }
  stats.frame_count = count;
  return stats;
}
