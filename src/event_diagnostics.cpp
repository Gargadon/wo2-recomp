#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xevent.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <stdexcept>
REXCVAR_DEFINE_BOOL(wo2_event_self_test, false, "WO2", "Check native event and inline reset synchronization at startup");
void wo2_test_events(rex::system::KernelState* kernel) {
  using rex::X_STATUS;
  if (!REXCVAR_GET(wo2_event_self_test)) return;
  auto* memory = kernel->memory();
  for (bool manual : {true, false}) {
    const auto address = memory->SystemHeapAlloc(sizeof(rex::system::X_KEVENT));
    if (!address) throw std::runtime_error("event self-test allocation failed");
    auto* guest = memory->TranslateVirtual<rex::system::X_KEVENT*>(address);
    guest->header.type = manual ? 0 : 1;
    guest->header.signal_state = 0;
    guest->header.wait_list_flink = address + 8;
    guest->header.wait_list_blink = address + 8;
    auto event = rex::system::XObject::GetNativeObject<rex::system::XEvent>(kernel, guest);
    if (!event || event->guest_object() != address) throw std::runtime_error("native event address was not retained");
    event->Set(0, false);
    if (guest->header.signal_state != 1) throw std::runtime_error("event Set did not update guest state");
    guest->header.signal_state = 0; // The title's inline KeClearEvent.
    event = rex::system::XObject::GetNativeObject<rex::system::XEvent>(kernel, guest);
    uint64_t timeout = 0;
    if (event->Wait(0, 0, 0, &timeout) != X_STATUS_TIMEOUT) throw std::runtime_error("inline clear left host event signalled");
    guest->header.signal_state = 1;
    event = rex::system::XObject::GetNativeObject<rex::system::XEvent>(kernel, guest);
    if (event->Wait(0, 0, 0, &timeout) != X_STATUS_SUCCESS) throw std::runtime_error("inline set did not wake host event");
    if (uint32_t(guest->header.signal_state) != unsigned(manual)) throw std::runtime_error("wait used incorrect reset semantics");
    event->Reset();
  }
  REXSYS_INFO("[wo2-event-test] PASS: native address, Set, inline clear/set, manual/auto-reset waits");
}
void wo2_clear_host_event(PPCRegister& address) {
  auto* kernel = REX_KERNEL_STATE();
  auto* guest = kernel->memory()->TranslateVirtual<rex::system::X_KEVENT*>(address.u32);
  auto event = rex::system::XObject::GetNativeObject<rex::system::XEvent>(kernel, guest);
  if (event) event->Clear();
}
