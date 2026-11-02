#include <fstream>
#include <Windows.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <string>
REXCVAR_DEFINE_BOOL(wo2_video_trace, false, "WO2", "Trace movie open, metadata and player update results");
namespace {
bool enabled() { static const bool value=REXCVAR_GET(wo2_video_trace); return value; }
unsigned word(unsigned address) { return __builtin_bswap32(*REX_KERNEL_STATE()->memory()->TranslateVirtual<unsigned*>(address)); }
thread_local unsigned output_slot=0, movie_serial=0, sample_count=0;
}
void wo2_video_open(PPCRegister& filename,PPCRegister& output) {
 if(!enabled()) return;
 output_slot=output.u32; ++movie_serial; sample_count=0;
 std::string path;
 auto* bytes=REX_KERNEL_STATE()->memory()->TranslateVirtual<const char*>(filename.u32);
 for(unsigned i=0;i<260 && bytes[i];++i) path+=bytes[i];
 REXLOG_INFO("[video] open file={} output={:08X}",path,output_slot);
}
void wo2_video_open_result(PPCRegister& result) {
 if(!enabled()) return;
 unsigned object=(result.s32>=0 && output_slot)?word(output_slot):0;
 REXLOG_INFO("[video] open_result={:08X} object={:08X}",result.u32,object);
 if(object) { unsigned table=word(object); REXLOG_INFO("[video] vtable={:08X} release={:08X} update={:08X} slot29={:08X}",table,word(table+8),word(table+80),word(table+116)); }
}
void wo2_video_update(PPCRegister& object,PPCRegister& flags) {
 if(!enabled()) return;
 static thread_local unsigned calls=0;
 if(++calls<=8 || calls%300==0) REXLOG_INFO("[video] update object={:08X} flags={:08X} calls={}",object.u32,flags.u32,calls);
}
void wo2_video_update_result(PPCRegister& result) {
 if(!enabled()) return;
 static thread_local unsigned calls=0,last=0xffffffffu;
 ++calls;
 if(calls<=8 || calls%300==0 || last!=result.u32) REXLOG_INFO("[video] update_result={:08X} calls={}",result.u32,calls);
 last=result.u32;
}
void wo2_video_metadata(PPCRegister& result,PPCRegister& wrapper) {
 if(!enabled()) return;
 REXLOG_INFO("[video] metadata_result={:08X} wrapper={:08X} raw={:08X},{:08X},{:08X},{:08X}",result.u32,wrapper.u32,word(wrapper.u32+4),word(wrapper.u32+8),word(wrapper.u32+12),word(wrapper.u32+16));
}

// Phase two: observe sample flow without dereferencing unverified payloads.
namespace {
void sample_event(unsigned site,unsigned object,unsigned value,unsigned detail) {
 if(!enabled()) return;
 static thread_local unsigned counts[5]={};
 unsigned count=++counts[site];
 if(count<=8 || count%300==0)
  REXLOG_INFO("[video2] site={} object={:08X} value={:08X} detail={:08X} calls={}",site,object,value,detail,count);
}
}
void wo2_video_flags(PPCRegister& result,PPCRegister& object) {
 sample_event(0,object.u32,result.u32,0);
}
void wo2_video_ready(PPCRegister& result,PPCRegister& object,PPCRegister& stack) {
 if(!enabled()) return;
 sample_event(1,object.u32,result.u32,word(stack.u32+80));
}
void wo2_video_payload(PPCRegister& result,PPCRegister& object) {
 sample_event(2,object.u32,result.u32,0);
}
void wo2_video_property(PPCRegister& result,PPCRegister& object,PPCRegister& payload) {
 sample_event(3,object.u32,result.u32,payload.u32);
}
void wo2_video_render_path(PPCRegister& object,PPCRegister& payload,PPCRegister& property) {
 sample_event(4,object.u32,property.u32,payload.u32);
}

void wo2_video_dump_sample(PPCRegister& data,PPCRegister& size,PPCRegister& object) {
 if(!enabled()) return;
 ++sample_count;
 if(sample_count!=10 && sample_count!=30) return;
 if(!data.u32 || size.u32<8 || size.u32>16*1024*1024) return;
 std::string buffer(size.u32,'\0');
 SIZE_T copied=0;
 const void* source=REX_KERNEL_STATE()->memory()->TranslateVirtual<const void*>(data.u32);
 if(!ReadProcessMemory(GetCurrentProcess(),source,buffer.data(),buffer.size(),&copied) || copied!=buffer.size()) {
  REXLOG_INFO("[video3] sample_copy_failed copied={} expected={}",copied,buffer.size()); return;
 }
 const std::string path="video-sample-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(movie_serial)+"-"+std::to_string(sample_count)+".raw";
 std::ofstream file(path,std::ios::binary); file.write(buffer.data(),buffer.size());
 REXLOG_INFO("[video3] sample={} ptr={:08X} bytes={} metadata={}x{} saved={}",path,data.u32,size.u32,word(object.u32+232),word(object.u32+236),bool(file));
}

void wo2_video_sink(PPCRegister& result,PPCRegister& object,PPCRegister& stack) {
 if(!enabled()) return;
 static thread_local unsigned count=0;
 if(++count>8 && count%300) return;
 unsigned sink=word(stack.u32+88);
 unsigned engine=word(object.u32+44);
 REXLOG_INFO("[video4] delivery_result={:08X} engine={:08X} sink={:08X} calls={}",result.u32,engine,sink,count);
 if(engine) {unsigned vt=word(engine); REXLOG_INFO("[video4] engine_vt={:08X} get_sink={:08X} clock={:08X}",vt,word(vt+64),word(vt+76));}
 if(sink) {unsigned vt=word(sink); REXLOG_INFO("[video4] sink_vt={:08X} submit={:08X} finish={:08X} process={:08X}",vt,word(vt+56),word(vt+60),word(vt+80));}
}
void wo2_video_sink_result(PPCRegister& result) {
 if(!enabled()) return;
 static thread_local unsigned count=0;
 if(++count<=8 || count%300==0) REXLOG_INFO("[video4] finish_result={:08X} calls={}",result.u32,count);
}

void wo2_video_texture_map(PPCRegister& result,PPCRegister& sink,PPCRegister& plane,PPCRegister& stack) {
 if(!enabled()) return;
 static thread_local unsigned count=0;
 if(++count>12 && count%900) return;
 unsigned p=plane.u32;
 REXLOG_INFO("[video5] map result={:08X} sink={:08X} plane={} pitch={} pixels={:08X} rowbytes={} rows={} source_stride={}",result.u32,sink.u32,p,word(stack.u32+80),word(stack.u32+84),p<3?word(sink.u32+332+p*4):0,p<3?word(sink.u32+356+p*4):0,p<3?word(sink.u32+344+p*4):0);
}
void wo2_video_texture_copy(PPCRegister& sink,PPCRegister& plane,PPCRegister& stack) {
 if(!enabled() || plane.u32>=3) return;
 static thread_local unsigned count=0;
 if(++count!=28 && count!=29 && count!=30) return;
 unsigned pitch=word(stack.u32+80),pixels=word(stack.u32+84),rows=word(sink.u32+356+plane.u32*4);
 if(!pixels || !pitch || !rows || pitch>8192 || rows>2160) return;
 std::string buffer(size_t(pitch)*rows,'\0'); SIZE_T copied=0;
 if(!ReadProcessMemory(GetCurrentProcess(),REX_KERNEL_STATE()->memory()->TranslateVirtual<const void*>(pixels),buffer.data(),buffer.size(),&copied)) return;
 std::string path="video-texture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(plane.u32)+".raw";
 std::ofstream file(path,std::ios::binary);file.write(buffer.data(),buffer.size());
 REXLOG_INFO("[video5] copied_plane={} file={} pitch={} rows={} bytes={}",plane.u32,path,pitch,rows,buffer.size());
}
