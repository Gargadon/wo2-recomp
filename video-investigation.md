# Black WMV investigation

Ghidra/XEXLoaderWV string inventory: out/ghidra/video-map.txt.
Verified generated-code call chain:
82193470 builds a movie path -> 8218CED0 -> 82363AA8 -> 823633D0.
82363AA8 stores filename r4 in a zero-initialized configuration and passes
r5 as the output object slot. 82363AE4 observes the factory return status.
8218CE48 queries metadata through 82362DB0; raw wrapper fields are logged
at 8218CE6C, without assuming their format yet.
8218CD30 calls 82362CC0; the latter tail-calls vtable byte offset 80.
Results at 8218CD70 and 8218CD98 include the retry path (status 0x166600E8).

Run: .\run.ps1 --wo2_video_trace
Let a movie play with black image and audible sound for 20-30 seconds.
Then close normally; [video] lines are in the usual game log.
Tracing does not replace the decoder or change event handling. Updates are
sampled (first eight, then every 300); changed return statuses are logged.

The XEX contains WMV decoder shader paths (residuals, motion compensation,
detiling). This demonstrates a GPU decoding path exists, not that it executes
correctly. Player lifecycle and actual frame delivery still require mapping
from the observed vtable/function addresses. Do not equate update success
with proof that a valid video frame reached the screen.

## First video trace, log warriors_orochi_2_020.log

MEV28, MEV29 and MEV26 opened with result 0. Object 0x42E34C40;
vtable 0x8209f658. Metadata result 0, first raw fields 0x500/0x2d0
(1280/720). Update calls return 0 roughly every 33 ms; later 0x16660026
at the transition to the next movie. Do not interpret status 0 as valid
image pixels; this trace covers the API, not decoded surfaces.

Static vtable extraction: out/ghidra/video-vtable.txt. Slot+80 is
82370BB0, a thunk to slot+76 (82372F10). Therefore 82370BB0 is not the
full renderer. 82372F10 retrieves a sample through internal object+44,
checks it via 82370330, obtains its payload via 82370348, and queries its
vtable+88. It has distinct paths depending on status flags and payload.
The next trace should capture flags at 82373158, sample readiness after
823731B8/823731FC, payload at 8237320C and property at 82373220 to distinguish
missing sample, missing surface and later render/presentation failure.
No event lifetime fault or negative factory/metadata result is visible here.
This does not rule out asynchronous worker or decoding failures.

## Trace phase two

Same flag: --wo2_video_trace. [video2] sites:
0: status flags returned by vtable+260 (82373158).
1: readiness result from 82370330, detail=sample handle from stack+80.
2: payload returned by 82370348 (not assumed to be a surface yet).
3: result of payload vtable+88, detail=payload pointer.
4: branch 82373364 taken for nonzero property; detail=payload pointer.
Each site logs its first eight hits and every 300 thereafter. Missing lines
mean the site was not observed during the captured interval, not necessarily
that a frame never existed. These are raw observations; success codes alone
do not establish valid pixels. No guest values or rendering state are changed.

## Phase two result: warriors_orochi_2_021.log

Ready samples and rotating payloads are observed. vtable+88 consistently
returns 0x151808 = 1382408 bytes, equal to 1280*720*3/2+8. This supports,
but does not prove, an eight-byte header plus YUV420 image. The nonzero
payload-processing branch at 82373364 is reached, so missing samples are
not the leading explanation. Valid image content is not yet established.

Phase three adds bounded raw copies of samples 10 and 30 for each movie,
at 82373380 after payload vtable+80 returns its data pointer. ReadProcessMemory
checks the copy without crashing on inaccessible pages. Files are beside
the executable, named video-sample-PID-MOVIE-SAMPLE.raw. Same video trace flag.
Compare image content outside Xenos before changing the decoder or renderer.

## Raw sample result: warriors_orochi_2_022.log, PID 7796

Six complete 1382408-byte samples were saved. The first eight bytes vary
with sample time; the remaining bytes match a planar 1280x720 YUV420 layout.
Movies 1/2 have nonuniform luma/chroma. Visual inspection of movie 2, sample
30 luma (out/profiles/video-movie2-luma.png) shows recognizable rendered
content, establishing image-bearing decoded output before presentation.
Movie 3 samples 10/30 are uniform Y=16, UV=128 (black); cannot establish
whether expected opening black frames without a reference decode.

Conclusion: missing video image for at least MEV29 is downstream from the
captured sample buffer. Focus on the internal sink at object+44, its video
conversion/texture-upload/presentation chain, not factory/event wakeup as
the leading explanation. Valid luma does not prove every decoded frame or
chroma plane is correct; compare to source if needed. No renderer fix yet.

## Automated downstream capture: logs 023 and 024

Log 023: engine 0x42e34d80, vtable 0x8209f5d8; sink 0x42e359d0,
vtable 0x8209f3a8. Submit method 82363c78, finish 82363ce0,
processing method 82365628. Delivery/finish returns observed as zero;
this is API status, not visual validation.

Log 024 / PID 13892: 82365628 copies three planes into mapped textures.
Y: 1280x720, pitch1280. U/V: 640x360, pitch768. Full copies captured at
82365760 after the row-copy loop. Compared to decoded sample 1/frame10:
- Y: 921600 bytes, zero differences.
- U: 230400 active bytes, zero differences after removing row padding.
- V: 230400 active bytes, zero differences after removing row padding.
Reproduction/check: python tmp/compare-video-textures.py.

Therefore sample delivery and CPU texture-plane copies preserve image data
for this capture. This does not establish that Xenos sees current texture
contents or interprets texture fetch descriptors correctly, nor that its
shader/render state yields visible output. Next boundary: GPU texture
fetch descriptors/physical alias/cache invalidation, shaders, target state
and actual draw output. Automated startup instances were closed normally.

## Xenos draw capture (log 026)

Shader dump identifies YUV converter PS hash 9C016D19C75C8B76; reads
textures tf0/tf2/tf1, constants c254/c255, exports RGB with alpha=1.
Added SDK diagnostic patch patches/rexglue-video-draw-trace.patch.
Log 026 proves pipeline ready, texture usage mask7, color maskF, and
D3DDrawInstanced with six vertices. YUV constants are nonzero/plausible.
Run used --no-async_shader_compilation to eliminate async PSO skipping as
a confounder for command execution (no visual fix claimed).

Descriptor bases are one page above virtual lock pointers masked to 29 bits.
This is expected: Memory::GetPhysicalAddress / PhysicalHeap adds 0x1000 for
E0000000+ aliases; generated REX_PHYS_HOST_OFFSET and TranslateVirtual also
apply the host offset. Do not patch this discrepancy as a bug.

Still unknown: GPU resource pixel values/cache freshness, transformed vertex
positions, final viewport/scissor/depth/blend/output contents. Need a GPU
capture/readback to distinguish bad sampling, clipping or later overwrite.
CPU plane equality plus an emitted draw does not prove visible GPU output.

## RenderDoc capture and working ROV path (2026-10-30)

Captured RTV with portable RenderDoc 1.46:
out/gpu-captures/wo2-video_frame848.rdc. Event 108 samples three valid
R8 textures and writes a recognizable Omega Force movie image to
ResourceId::745 (RT @ 0t, pitch16t, 1xMSAA, RGBA8). Saved image:
out/gpu-captures/event108-output0.png. Post-VS positions cover the active
1280x720 rectangle with the 8192x8192 host viewport and 1280x720 scissor.
Thus decoding, GPU texture upload, YUV conversion and video geometry work
in this captured frame.

The later resolve/load/presentation resources are black. Event 175 dumps
from depth RT ResourceId::732 (base720t, pitch16t, 4xMSAA); event182 resolves
EDRAM to guest address527400960, and event189 loads that address as the
1280x720 swap texture. This narrows the remaining RTV defect to the
EDRAM ownership/resolve/presentation chain. It does not yet prove which
operation is incorrect; do not bypass draws or invent completion values.

Controlled second launch changed render_target_path_d3d12 from rtv to rov,
retaining --no-async_shader_compilation from the RTV test. User confirmed
the video is now visible. Capture: wo2-video-rov_frame824.rdc.
Windows run.ps1 and run.bat now default to rov. No rebuild required for
this launcher change. Battle performance has not been re-measured; this
is a working video configuration, not a proven RTV renderer repair or
performance fix. Vulkan has not been tested by this experiment.

Repro scripts and replay reports are under out/gpu-captures. RenderDoc
temporarily needed Python debugging disabled and analytics total opt-out
to run unattended; the original UI.config was backed up and restored.
