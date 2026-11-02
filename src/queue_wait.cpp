#include <rex/cvar.h>
#include <thread>
#include <immintrin.h>

REXCVAR_DEFINE_BOOL(wo2_cooperative_wait, true, "WO2",
                   "Yield periodically while the guest waits for queue progress");

// Called only on the queue's retry path. Do not alter guest state or skip
// its progress checks: the producer must still satisfy the original wait.
void wo2_queue_wait() {
  // Read once per host thread to avoid a registry lookup in the hot loop.
  static thread_local const bool enabled = REXCVAR_GET(wo2_cooperative_wait);
  if (!enabled) return;
  static thread_local unsigned retries = 0;
  _mm_pause();
  if ((++retries & 63u) == 0) std::this_thread::yield();
}

