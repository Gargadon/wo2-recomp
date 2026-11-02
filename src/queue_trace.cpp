#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <rex/cvar.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include <Windows.h>
REXCVAR_DEFINE_BOOL(wo2_queue_trace, false, "WO2", "Capture queue waits and matching GPU writes (restart required)");
namespace {
struct Event { long long us; unsigned tid,kind,object,address,value,target; };
std::atomic<unsigned> watched{0xffffffffu};
std::mutex events_mutex;
std::vector<Event> events;
const auto origin = std::chrono::steady_clock::now();
bool active() { static const bool enabled = REXCVAR_GET(wo2_queue_trace); return enabled; }
void record(unsigned kind,unsigned object,unsigned address,unsigned value,unsigned target) {
 Event e{std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-origin).count(),GetCurrentThreadId(),kind,object,address,value,target};
 std::lock_guard lock(events_mutex);
 if(events.size()<262144) events.push_back(e);
}
void start() {
 static const bool started=[] {
  events.reserve(262144);
  std::thread([] {
   std::ofstream file("queue-trace-"+std::to_string(GetCurrentProcessId())+".csv");
   file << "us,tid,kind,object,physical,value,target\n";
   std::vector<Event> batch; batch.reserve(262144);
   for(;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    { std::lock_guard lock(events_mutex); batch.swap(events); }
    for(auto &e:batch) file<<e.us<<','<<e.tid<<','<<e.kind<<','<<e.object<<','<<e.address<<','<<e.value<<','<<e.target<<'\n';
    file.flush(); batch.clear();
   }
  }).detach(); return true;
 }(); (void)started;
}
thread_local unsigned current_object=0,current_address=0,current_target=0;
}
void wo2_queue_trace_enter(PPCRegister& object,PPCRegister& target) {
 if(!active()) return;
 start();
 auto* memory=REX_KERNEL_STATE()->memory();
 unsigned pointer=__builtin_bswap32(*memory->TranslateVirtual<unsigned*>(object.u32+10896));
 unsigned physical=memory->GetPhysicalAddress(pointer);
 if (physical == 0xffffffffu) return;
 current_object=object.u32; current_address=physical; current_target=target.u32;
 watched.store(physical,std::memory_order_release);
 record(1,object.u32,physical,__builtin_bswap32(*memory->TranslateVirtual<unsigned*>(pointer)),target.u32);
}
void wo2_queue_trace_exit() {
 if(!active() || !current_object) return;
 auto* memory=REX_KERNEL_STATE()->memory();
 record(2,current_object,current_address,__builtin_bswap32(*memory->TranslatePhysical<unsigned*>(current_address)),current_target);
 current_object=0;
}
extern "C" __declspec(dllexport) void wo2_trace_gpu_write(unsigned kind,unsigned physical,unsigned stored) {
 if(physical!=watched.load(std::memory_order_acquire)) return;
 record(kind,0,physical,__builtin_bswap32(stored),0);
}
extern "C" __declspec(dllexport) void wo2_trace_queue_latency(unsigned kind,unsigned detail,unsigned value) {
 if(!active()) return;
 start();
 record(kind,0,detail,value,0);
}

