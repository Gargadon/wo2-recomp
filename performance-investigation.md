# Stage performance investigation

The 2026-10-30 capture is out/profiles/stage-20261030-023741.etl.

Observed: 19-20 guest swaps/second in battle; 480p improves to about 21.
Disabling occlusion queries did not materially improve the result.
Menus are fast. WO2 battle scenes have more visible detail than SW2XL.

A 10-second thread CPU sample measured main guest thread 6332 at 97.7%
of one core, thread 16064 at 90.5%, and GPU thread 19248 at 11.3%.
These numbers do not measure GPU utilization or identify wait/spin functions.

ETW sampled CPU by module (percent of six-core machine capacity):
- warriors_orochi_2.exe: 17.09%
- ntoskrnl.exe: 6.27%
- ntdll.dll: 4.27%
- rexruntime.dll: 3.63%
- rexgpu-xenos.dll: 1.40%
- amdxc64.dll: 1.20%

The old executable had no matching PDB. Public-symbol resolution did not
complete and was stopped. Do not attribute ntdll samples to a specific lock
without resolved stacks. Do not infer GPU utilization from CPU samples.

Release now emits CodeView line information, a PDB and a linker MAP while
retaining O3 and ThinLTO. Capture again with this exact binary; preserve the
EXE, PDB and DLL versions together with the ETL. The old ETL must not be
symbolized using a newly linked binary's PDB.

Any required SDK changes must be stored as patches within wo2-recomp.
No SDK source patch has been made for this investigation.

## Symbolized capture: 2026-10-30 02:51

ETL: out/profiles/stage-20261030-025116.etl
Matching binaries/PDB/MAP: same path with .binaries suffix.
Function samples: out/profiles/stage-symbol-functions.txt.

Top game-process sampled CPU (percent of total machine capacity):
- sub_8231A690: 5.71%
- sub_8231C9A0: 3.14%
- NtDelayExecution: 3.12%
- RtlQueryPerformanceCounter: 2.36%
- rexruntime.dll unresolved: 3.76%

sub_8231C9A0 loops over a memory-backed progress counter loaded through
object+10896 and the producer state at object+10908, calling sub_8231A690
while the unsigned-distance condition remains unsatisfied. sub_8231A690
contains guest db16cyc delays emitted as comments and progress/time checks.
This establishes repeated polling, not the identity of the producer or the
reason it is late. It is not sufficient evidence to skip the wait or alter
counter values. Need runtime object/counter observations and caller stacks
to distinguish command-buffer progress from other queues.

No SDK fix is justified yet; any SDK changes must be supplied as a patch
under this project. Optimized symbol build and capture preservation are
project-only changes.

## Cooperative retry experiment

src/queue_wait.cpp is inserted at 0x8231CA30, the queue retry path.
It issues a host PAUSE and yields once per 64 retries. Original guest
instructions, progress reads, comparisons and return values remain intact.
The startup option --no-wo2_cooperative_wait disables it for comparison.
The option is cached per thread and changes require restarting the game.

This may reduce host CPU contention; it does not fix a late producer or
establish that the game's FPS will increase. Validate against the same
stage/camera and check visual correctness and stability. WMV rendering
remains a separate unresolved issue. No SDK source change is involved.

## Reanalysis with cooperative wait (02:58 capture)

Capture: out/profiles/stage-20261030-025822.etl; exact binaries preserved.
Resolved report: out/profiles/stage-cooperative-functions.txt.
Machine CPU sample percentages, baseline -> cooperative capture:
- sub_8231A690: 5.71 -> 5.41
- sub_8231C9A0: 3.14 -> 3.08
- rexruntime.dll unresolved: 3.76 -> 3.90
- rexgpu-xenos.dll unresolved: 1.32 -> 1.67
- NtDelayExecution: 3.12 -> 1.85
- RtlQueryPerformanceCounter: 2.36 -> 1.36

Ten-second thread sample: guest main thread 7684 92.8% of one core,
next busiest 11592 23.5%; GPU worker 17388 18.1%. Scenes and timings were
not identical, so these are not controlled speedup measurements.

Result: cooperative retry has not removed the dominant guest polling paths.
Do not claim an FPS improvement from this capture; no new FPS measurement
was recorded. Next diagnostic must identify the owner of the progress word
and observe producer/consumer values and caller chains. More arbitrary
sleeping or skipping the guest wait is not supported by the evidence.

## Direct Ghidra/XEXLoaderWV analysis

Imported default.xex directly with Ghidra 12.1.4 and installed XEXLoaderWV;
no REA binary analysis was used. Project: out/ghidra/project/wo2-queue.gpr.
Output: out/ghidra/queue-decompile-core.txt.
Ghidra truncated functions after save-register helpers. For the two hot
routines, core bodies were separately defined after the first two instructions;
this is analysis metadata only and is explicitly noted in the output.

The recovered core verifies the same unsigned-distance polling condition
seen in generated C++. object+0x2a90 points to the progress word;
object+0x2a9c holds producer state. Initialization sub_8231D3E8 allocates
96-byte GPU-visible storage for +0x2a90 and configures the primary ring,
read-pointer writeback at storage+60, and system GPU identifier at storage+8.
Thus the progress word at storage+0 is distinct from the ring read pointer
at +60 and the system GPU identifier at +8. Do not treat the unimplemented
VdSetSystemCommandBufferGpuIdentifierAddress as proven cause of this wait.
Need locate packet writes to storage+0 and measure when they execute.

## Queue trace from playthrough (PID 19112)

Input: out/build/win-amd64-release/queue-trace-19112.csv
Analyzer: tmp/analyze-queue-trace.py
Summary: out/profiles/queue-trace-summary.json

104491 events spanning 435.987 seconds; 35370 paired waits, no overwritten
entries or orphan exits, one final unmatched entry. All tracked writes were
EVENT_WRITE_SHD (kind 11), targeting physical 0x1fc9c000 (33750 writes).
9961 completed waits exceeded 10 ms. 9958 have an exact target-value write
between entry and exit. For those matched long waits:
- Entry to matching write: median 29.382 ms, p90 35.127 ms, p99 45.407 ms.
- Matching write to exit: median 0.003 ms, p90 0.006 ms, p99 0.039 ms.
Total paired wait wall time: 278.626 seconds (not CPU execution time).

Conclusion: EVENT_WRITE_SHD is the observed producer for this progress word.
The guest observes its updates promptly. Slow visibility/endian handling of
this particular word is not supported as the dominant cause by this trace.
Latency precedes execution of the matching packet. The trace does NOT record
its enqueue timestamp or per-packet durations, so it cannot distinguish late
submission from earlier command execution cost/stalls. Hardware GPU completion
is not measured. Next instrumentation should timestamp command submission and
primary/indirect packet execution, including waits and D3D12 queue fences.
Do not bypass the guest wait or synthesize completion values.

## DW5E reference comparison

Reference root: C:/Users/Gargadon/Source2/dw5-empires-recomp (read only).
notes/PHASE2.md documents playable Linux/Vulkan movies and battles.
patches/rexglue/0001-dw5e-runtime-fixes.patch contains:
- General guest/host KEVENT reconciliation (SyncFromGuest called on native
  object lookup, initial guest header state, set/reset and auto-reset updates).
  Our SDK lacks SyncFromGuest; WO2 only hooks one inline Clear site.
  Relevant to XMEDIA lifecycle/synchronization; not proven to fix black video.
- XMA partial-frame drain fix; relevant to audio cleanup/synchronization,
  not a demonstrated cause of missing video pixels or the GPU fence delay.
- Better invalid-call diagnostics (LR/register/object details).

DW5E's EVENT_WRITE_SHD implementation is equivalent to our uninstrumented
handler. No dedicated packet/fence-performance fix was found in its patch.
Its app has no native WMV replacement or special video renderer. Existing
player lifecycle trace hooks are a useful template; addresses are game-specific
and cannot be copied to WO2. Need map WO2's XMEDIA constructor/callback/worker
in Ghidra before adapting them.

A controlled Vulkan-versus-D3D12 test could isolate a backend-dependent video
issue, but the documented success is Linux/Vulkan, not established Windows
parity. Do not replace all SDK files or claim a fix solely from these notes.

## ROV follow-up

User reports stage performance up to 30 FPS with ROV, versus approximately
20 FPS with RTV; stages still feel slow. This is an observed improvement,
not a controlled benchmark. No active game instance remained when the
follow-up started, so a current stage profile could not be captured.

Added opt-in SDK packet/submission wall-time instrumentation, preserved in
patches/rexglue-queue-latency.patch, and host CSV callback in queue_trace.cpp.
Use trace-stage.ps1, then trace-stage.ps1 -NoVSync in the same scene to test
whether presentation synchronization contributes to the observed rate.
Do not assume 30 FPS is a hard cap or that higher FPS restores game speed.
Need traces from the stage to distinguish current packet costs from waits.

## ROV stage traces: 1424 / 10900

User measured menus60/stages25-30 with VSync and menus200+/stages30-36
without. Files span49.51s and47.41s respectively; neither stores launch
arguments, so their VSync labels are inferred from menu timing, not proven.
Summary: out/profiles/rov-latency-summary.json; analyzer:
tmp/analyze-rov-latency.py. Third trace1844 is only14.4s.

In seconds20-40, PID1424 XE_SWAP uses8.745/8.761 seconds per10-second
window, median29.897/31.399ms. PID10900 uses8.665/8.643 seconds,
median28.039/28.300ms. Primary batches are32-36ms median. Slow draw
packets and WAIT_REG_MEM do not account for the dominant measured time
in these stable windows. Sub-ms draw costs are not captured individually.
This localizes the CPU wall delay inside swap handling, not yet to a
specific host operation or hardware-GPU bottleneck.

Added finer D3D12 swap/EndSubmission phase instrumentation in separate
patch rexglue-submission-latency.patch. Initial SDK-only rebuild left the
executable-directory GPU DLL stale; caught by comparing hashes. Added
wo2_sync_gpu build target so future SDK-only builds copy the updated DLL.
The failed phase smoke capture16104 therefore has no kind24 events and
must not be used to infer timings. Earlier kind21 traces remain usable.

With VSync disabled the SDK emits virtual vblanks at1000Hz, rather than
the configured60Hz (graphics_system.cpp). No independent guest-frame
limiter existed. Added project-level pacing at the existing guest swap
hook, default60 FPS via wo2_fps_limit (0 disables for diagnostic baselines).
It sleeps to a steady-clock deadline and resets late deadlines without
catch-up bursts. Stage frames already over16.67ms receive no added sleep.
This caps menus; it does not make slow stage frames meet60FPS.

Validation: Windows Release build succeeded. Final ROV/no-VSync startup
smoke PID16424 exited0, recorded309 guest frames and469 phase events.
Minimum observed frame interval16.972ms (below60 FPS maximum); median
32.467ms during intro playback. This validates pacing and trace delivery
at startup, not sustained menu60FPS or stage performance. Earlier smoke
21112 exposed catch-up intervals; the limiter was corrected to schedule
the next deadline from actual wake time, then rebuilt and retested.

## Full ROV trace5520: runtime cinematics and stage

User observed a runtime cinematic mostly20-30FPS, a final section reaching
60, followed by stage18-27FPS. Trace spans approximately173s. Scene
boundaries are not marked, so do not assign exact timestamps to specific
cinematic shots. Seconds40-50 and70-80 average55.89/55.97 guestFPS;
seconds130-170 average23-26FPS. See queue-trace-5520-submission.json.

In seconds130-170, phase14 (command_list Close + ExecuteCommandLists)
has medians32.204,34.795,36.824,35.840ms, totaling8.47-8.66s per10s
window. Phase13 deferred replay medians0.75-0.78ms. The combined driver
boundary is the dominant measured wall delay; pipeline creation, page
validity updates and deferred replay do not explain it. This does not
yet establish CPU driver cost versus driver blocking/backpressure.
Added rexglue-driver-latency.patch to split Close (phase16) from
ExecuteCommandLists (phase17), preserving guest waits and rendering.

## Close/Execute split with the TU1 event-sync build (2026-10-31)

First measurement with phases 16 and 17 separated, on the build containing
rexglue-null-object-guard.patch + rexglue-event-guest-sync.patch. Live menu and
battle captures (the game now reaches a battle directly from its own continue
path), same machine, ROV render target path, VSync on, 720p unless noted.

- Menu: 46.0 guest FPS (21.75 ms).
- Battle 720p: 22.5-25.5 FPS (39.2-44.4 ms).
- Battle 480p: 23.8 FPS (42.0 ms). Lowering resolution does not help, so the
  limit is not pixel/fill-rate.
- Phase split from `queue-trace-3284.csv` (analyzer: tmp/phase-summary.py):

  | window | frames | fps  | close_only (16) | execute_only (17) |
  |--------|--------|------|-----------------|-------------------|
  | 0      | 332    | 34.6 | -               | 13.1 ms           |
  | 1      | 415    | 41.5 | -               | 10.7 ms           |
  | 2      | 247    | 24.7 | 0.41 ms         | 32.8 ms           |
  | 3      | 259    | 25.9 | -               | 30.1 ms           |
  | 4      | 248    | 24.8 | 0.14 ms         | 29.4 ms           |
  | 5      | 14     | 24.7 | -               | 30.7 ms           |

- `command_list->Close()` is negligible (0.1-0.4 ms). Effectively all of the
  driver boundary is `ExecuteCommandLists`: 29-33 ms per frame at ~25 FPS, i.e.
  about 75% of the frame.
- Draws per frame measured with a temporary counter in IssueDraw (reverted):
  about 51, peak 58. A ~30 ms driver call for ~51 draws is far too large to be
  per-command CPU cost, so the time is consistent with the queue blocking while
  the GPU drains the frame (or a fixed per-frame GPU-side cost), not with
  command recording, command count or pixel count.
- Process CPU is about 1.6 cores during the battle, so the machine is not
  saturated; the submission thread is serialized on that wait.

Conclusions that the data supports:

1. The stage limit is not the guest event/queue defect fixed earlier. That fix
   removed the busy polling and cut startup/menu CPU, but the battle frame rate
   and the ExecuteCommandLists time are unchanged.
2. The stage limit is not Close, not deferred replay, not pipeline creation and
   not fill rate.
3. What remains is GPU-side drain of the submitted frame or a blocking driver
   call. The next measurement must separate the two: D3D12 timestamp queries or
   GPUView/PIX around the frame's ExecuteCommandLists, plus a per-command-type
   breakdown (draws vs resolves/copies/barriers) to find a fixed-cost pass.

Do not re-add arbitrary sleeps, skip the guest wait, or change the render
target path based on this data alone; none of it explains a 30 ms Execute.

## Command breakdown: the stage limit is command submission CPU cost

Follow-up experiments on the same build, all in a battle, ROV, 720p, with the
temporary histograms in `DeferredCommandList::Execute` (since reverted):

- No VSync: 22.6-24.0 FPS, ExecuteCommandLists 36-39 ms. Same as with VSync, so
  presentation/vblank is not the limit.
- `--native_2x_msaa` (extra GPU pixel work): 24.7-26.6 FPS, Execute 32-35 ms.
  Same. So the GPU is not the limit either.
- `--resolution=480p`: 23.8 FPS. Same. Not fill rate.

The replay histogram, averaged per frame over 60 replays:

| command | per frame |
|---------|-----------|
| kD3DDrawInstanced (9) | ~612 |
| kD3DDrawIndexedInstanced (8) | ~423 |
| kD3DSetGraphicsRootConstantBufferView (25) | ~2100 |
| kD3DIASetIndexBuffer (13) | ~420 |
| kD3DResourceBarrier (19) | ~310 |
| kD3DSetGraphicsRootDescriptorTable (26) | ~120 |
| kD3DCopyBufferRegion (3) | ~113 |
| kD3DSetPipelineState + handle (35/36) | ~200 |
| kD3DDispatch (7) | ~59 |
| everything else (clear/copy/dispatch/root sig/scissor/viewport) | ~300 |

Total about 4000 D3D12 commands per frame. At ~30 ms per ExecuteCommandLists
that is roughly 7 us per command, which is ordinary AMD D3D12 driver cost; the
frame is simply command-bound on the CPU, on the submission thread.

Redundant consecutive `SetGraphicsRootConstantBufferView` calls: **0**. Every one
of the ~2100 is a different buffer location, so deduplicating them is not an
option; the guest changes constants per draw.

Draw amplification: **none**. Measured in a single run, with the guest-side
`IssueDraw` counter and the replay counters active at the same time:

```
guest_draws=1094 draws_idx=450 draws_plain=615 cmds=4900 replay_ms=0.69
guest_draws=1042 draws_idx=411 draws_plain=602 cmds=4692 replay_ms=0.70
guest_draws= 968 draws_idx=376 draws_plain=563 cmds=4436 replay_ms=0.70
```

So the game genuinely issues ~1000 draws per frame and the backend turns them
into ~1060 D3D12 draws (1:1). The earlier "~51 per frame" reading was a bad
measurement (wrong frame boundary) and is withdrawn.

Recording the deferred command list costs **0.70 ms per frame** (0.15 us per
command), so the backend's own recording is not the problem. The ~30 ms is
inside `ExecuteCommandLists`, i.e. in the AMD driver: about **6.5 us per
command** on this machine (Ryzen 5 4500U, Zen 2, 6C/6T).

Conclusion: the battle frame is limited by the number of D3D12 commands issued
per frame (~4600, of which ~1060 draws and ~2100 root constant buffer views),
not by the GPU, not by vsync, not by fill rate, not by Close, and not by the
guest event/queue defect fixed earlier.

Directions that this evidence supports, in rough order of expected payoff:

1. Reduce constant-buffer commands per draw: move the per-draw constants to a
   root/push-constant upload or a shared constant buffer updated by copies,
   instead of one `SetGraphicsRootConstantBufferView` per fetch slot per draw.
2. Reduce draws: batch draws that share state.
3. Batch/reduce barriers (about 310 per frame) and index-buffer binds (~420).
4. A Vulkan backend (the DW5E reference runs it) removes this specific D3D12
   per-command cost, but that is a large change.

Any of these needs the same measurement discipline: same stage, same camera,
counts and FPS recorded together.

## GPU timestamps settle it: the battle is GPU-bound (2026-10-31)

The command-count analysis above measured the CPU side. To measure the GPU,
temporary `D3D12_QUERY_TYPE_TIMESTAMP` queries were placed around each frame's
command list and resolved into a readback buffer (diagnostic since reverted).
`gpu_submit_ms` is the GPU duration of the frame's commands; `gpu_frame_ms` is
the interval between consecutive frames' start timestamps, on the GPU clock.

Same build, ROV, 720p, no VSync:

| phase | gpu_submit_ms | gpu_frame_ms | guest FPS |
|-------|---------------|--------------|-----------|
| boot / menu | 3-5 | 16-28 | 60 (capped) |
| battle | 34-41 | 32-42 | 22-25 |
| battle, RTV path | 49-51 | 47-49 | ~20 |
| battle, camera raised | 26-33 | 26-40 | 23-29 |

So the GPU is busy 34-41 ms per battle frame and that is exactly the frame
time. The ~30 ms spent inside `ExecuteCommandLists` on the CPU is therefore
**backpressure**, not CPU work: the CPU is blocked waiting for the GPU queue.
This supersedes the earlier "not the GPU" statement in this file; that statement
was based on two invalid tests:

- `--resolution=480p` changes the guest video mode, but the title keeps its own
  1280x720 render targets, so pixel count did not change.
- `--native_2x_msaa` only changes behaviour when the title uses MSAA, and this
  title does not.

RTV is both slower (49-51 ms) and does not display WMV video, so ROV stays.

Why a 360 title costs this much on this machine: the Xbox 360 has 10 MB of
embedded DRAM for render targets, which makes blending and resolves nearly free.
The emulator has no EDRAM, so blending goes through rasterizer-ordered UAVs in
system memory on an integrated GPU of roughly 1 TFLOP. That is several times
more expensive per pixel than on the console. The menu (few draws, little
overdraw) costs 3-5 ms; the battle (about 1000 draws and heavy alpha overdraw)
costs 34-41 ms.

CPU headroom, for reference: the process uses about 1.6 of 6 cores, recording
the whole frame costs 0.70 ms, and the guest frame rate follows the GPU time
exactly. A faster CPU would not raise the battle frame rate on this machine.

The only lever that changes this materially is rendering fewer pixels. The
existing scaling path clamps `resolution_scale` / `draw_resolution_scale_x/y` to
**at least 1** (`std::max(1, ...)` in `TextureCache::GetConfigDrawResolutionScale`),
so rendering below the guest resolution is not currently possible; supporting it
would need fractional scales through the render target sizes, viewport transform
and shader coordinate mapping. Until then, the honest options are a stronger
GPU or accepting ~25 FPS in battles on this hardware.

## It is not the pixels either: the cost is per draw (2026-10-31)

Two experiments with the GPU timestamps, in battle, ROV, 720p, no VSync:

1. Half-size viewport **and** half-size scissor, so the draws shade one quarter
   of the pixels: GPU 30.2 ms -> 27.4 ms. A 4x reduction of shaded pixels bought
   about 10%. Pixel work is not the cost.
2. Only every second draw issued (~500 instead of ~1000, same pixels):
   GPU 30.2 ms -> 18-19 ms. Halving the draws removed 40% of the GPU time.

Backend option matrix (same scenario, `gpu_submit_ms`, last window):

| option | gpu_submit_ms |
|--------|---------------|
| baseline | 30.2 |
| `--anisotropic_override=0` | 28.4 |
| `--d3d12_bindless=false` | **39.1 (worse, keep bindless on)** |
| `--direct_host_resolve=false` | 31.3 |
| `--pre_mask_resolve_l2_block=false` | 27.4 |

Everything except bindless is within noise.

Conclusion: on this machine each D3D12 draw costs roughly **24-30 us of GPU
time**. That is the signature of the ROV (pixel shader interlock) path
serializing draws - interlocked rasterizer-ordered writes cannot overlap, so the
GPU drains between draws instead of pipelining them. With about 1060 draws per
frame, that alone is the frame. Consequences:

- Downscaling the internal resolution would **not** help. Measured before
  investing in it, so it was not implemented.
- The RTV path avoids ROV but pays for resolves instead: 49-51 ms, and it also
  breaks WMV video.
- The remaining lever is the number of draws per frame, or a backend whose
  synchronization lets draws overlap. Draw merging is limited here because each
  draw carries its own constants (~2 root constant buffer views per draw, all
  distinct), which is why each is a separate draw in the first place.

Do not start with resolution scaling or texture filtering based on this data;
neither moves the number that matters.

## Vulkan backend attempt (2026-10-31)

Vulkan is compiled out of this build by default on Windows
(`REXGLUE_USE_VULKAN=OFF`). To test it, the SDK was reconfigured with
`-DREXGLUE_USE_VULKAN=ON` and a temporary `gpu_backend` cvar was added so the
plugin could be asked for the Vulkan system (`rex_gpu_create` accepts
`any`/`d3d12`/`vulkan`). Both changes were reverted afterwards.

Result: Vulkan initializes and renders the battle correctly, but it is
significantly slower and does not show WMV video at all.

| backend | battle FPS | frame | video |
|---------|-----------|-------|-------|
| D3D12 + ROV | 22-25 | 39-44 ms | works |
| D3D12 + RTV | ~20 | 47-51 ms | broken |
| Vulkan | **14.9** | 67 ms | broken |

The Vulkan device in use is `amdvlk64.dll` (AMDVLK, the open-source AMD Vulkan
implementation) on `AMD Radeon(TM) Graphics`, and the log also shows
"Skipping Vulkan frame presentation due to async placeholder draw usage in this
frame", so presentation is being dropped as well.

Conclusion: Vulkan is not a win on this machine, and it is not the lever for the
per-draw cost described above. D3D12 + ROV stays as the path to use.

## Per-draw GPU timings: the cost is in the EDRAM transfers (2026-10-31)

Instrumented every draw with a timestamp pair (`patches/rexglue-wo2-draw-gpu-timings.patch`,
diagnostic, applied temporarily and then reverted). Battle, ROV, 720p, no VSync,
60-frame window:

```
draws=68190 avg_us=32.0 max_us=1825.7
buckets(<5,<15,<30,<60,<120,>120)=29463,16341,8557,5755,3250,4824
```

- About 1136 draws per frame, 32 us average, so about 36 ms per frame of draw
  time, which matches the per-frame GPU measurement.
- 43 % of the draws cost less than 5 us, so the game's geometry draws are cheap.
- **7 % of the draws (about 80 per frame) cost more than 120 us, and they account
  for the majority of the frame.** The maximum single draw was 1.8 ms.

Sampling the draws at or above 500 us shows every one of them is a
`DrawInstanced` with **3 or 6 vertices**:

```
[wo2-heavy] kind=1 size=3 us=953.9
[wo2-heavy] kind=1 size=6 us=1531.6
[wo2-heavy] kind=1 size=3 us=504.5
```

A 3-vertex or 6-vertex draw cannot be expensive because of geometry, so these are
full-screen passes. In the backend the only full-screen draws are the
emulator's own EDRAM work, in `src/graphics/d3d12/render_target_cache.cpp`:

- line 4549, `D3DDrawInstanced(transfer_vertex_count, 1, 0, 0)` - the render
  target **resolve / transfer** passes, drawn once per transfer rectangle, and
  up to 8 times when a stencil bit is involved (`for j <= is_stencil_bit * 7`).
- line 4678, `D3DDrawInstanced(3, 1, 0, 0)` - render target **clears done by
  drawing** a full-screen triangle, used when the clear value cannot be
  represented exactly as a float (k_32_FLOAT / k_32_32_FLOAT paths).

So the battle frame is dominated by the EDRAM emulation passes (resolves,
transfers and shader clears), not by the game's geometry and not by the ROV
blending of the scene itself. That also explains why changing the viewport size
barely helped: those passes cover the whole render target regardless of the
scene viewport.

Directions this supports, in order of expected payoff:

1. Skip resolves that are not needed - resolve a render target only when it is
   actually read as a texture, and avoid repeated resolves of unchanged content.
2. Audit the stencil transfer loop: 8 full-screen passes per transfer when
   stencil is enabled is the most suspicious multiplier in the code.
3. Prefer hardware `ClearRenderTargetView` over the drawing path where the
   bit-exact clear value does not matter for the title.
4. Only then consider lowering the resolution of the EDRAM copies.

The patch with the instrumentation is kept so this can be repeated:
`patches/rexglue-wo2-draw-gpu-timings.patch` (adds the `wo2_draw_gpu_timings`
cvar, default on while applied; logs `[wo2-draw]` and `[wo2-heavy]` lines).
It was reverted from the working tree after measuring.

## Points 1 and 2 measured: both are dead ends in the ROV path (2026-10-31)

Before changing anything, the two hypotheses were instrumented (temporary
counters in `render_target_cache.cpp`, reverted afterwards):

1. **Redundant resolves**: a resolve key was built from the colour/depth EDRAM
   bases and the destination range, and a draw epoch was bumped on every
   `Update()` with rasterization. A resolve repeating the same key with the same
   epoch would be skippable. Over about 50 seconds of battle:

   ```
   [wo2-resolve] total=24600 redundant=0
   ```

   **There are no redundant resolves.** The title only resolves when contents
   changed or the destination differs, so skipping them saves nothing.

2. **Stencil transfer loop** (`for j <= is_stencil_bit * 7`, up to 8 full-screen
   passes): the counters placed in that loop never fired, because
   `D3D12RenderTargetCache::Update` only calls `PerformTransfersAndResolveClears`
   for `Path::kHostRenderTargets`. With the **ROV (pixel shader interlock) path
   that code is not used at all**, so the 8-pass stencil loop never runs here.

This corrects the previous section: the heavy 3-vertex and 6-vertex draws seen in
the per-draw timings are therefore **guest draws** - the title's own full-screen
effect / post-processing passes - not emulator transfers or shader clears. Each
of those passes costs roughly 0.5-1.8 ms on this GPU, and about 60-80 of them per
frame are the battle frame.

So the remaining cost is the title's own full-screen passes as emulated by the
ROV path, not the resolve/transfer machinery. Further work would have to target
those passes specifically (identify their pixel shaders and check whether the ROV
path makes each of them serialise), rather than the resolve code.

## Which shaders those heavy passes use (2026-10-31)

The per-draw timer was extended to record the active pixel shader ucode hash for
every guest draw (the command processor stores it per draw in the frame's stream
and the timer reports it for the heavy ones). Battle sample:

```
[wo2-heavy] kind=1 size=6 us=745.2 ps=1EB86A9AE369C277
[wo2-heavy] kind=1 size=6 us=710.2 ps=1EB86A9AE369C277
[wo2-heavy] kind=1 size=3 us=783.4 ps=A4A965C189287B99
[wo2-heavy] kind=1 size=6 us=586.3 ps=1EB86A9AE369C277
[wo2-heavy] kind=1 size=6 us=745.2 ps=1EB86A9AE369C277
[wo2-heavy] kind=1 size=3 us=598.9 ps=A4A965C189287B99
[wo2-heavy] kind=1 size=6 us=653.6 ps=1EB86A9AE369C277
[wo2-heavy] kind=1 size=6 us=698.3 ps=1EB86A9AE369C277
```

Two pixel shaders account for essentially every expensive draw:

- `ps=1EB86A9AE369C277`, always a 6-vertex quad, about 0.6-0.75 ms per draw.
- `ps=A4A965C189287B99`, always a 3-vertex triangle, about 0.6-0.8 ms per draw.

Both are full-screen passes of the title itself (a quad and a full-screen
triangle), drawn roughly 60-80 times per frame between them, which is the battle
frame. At 1280x720 those times are roughly 1-1.3 ns per pixel, which is what a
multi-tap full-screen effect costs on this integrated GPU.

Conclusion: the remaining cost is the title's own full-screen effect passes, not
an inefficiency that the emulator adds in the resolve path. The levers left are
rendering those passes at a lower internal resolution (not supported, the scaling
path clamps to at least 1) or a stronger GPU; the emulator cannot merge or skip
them without changing the image.

## One of the two heavy passes is a clear in disguise (2026-10-31)

The timer also records the pixel shader's used texture mask, and the shaders were
dumped with `--dump_shaders` plus `--d3d12_dxbc_disasm`:

```
[wo2-heavy] kind=1 size=6 us=617.8 ps=1EB86A9AE369C277 textures=00000001
[wo2-heavy] kind=1 size=3 us=934.5 ps=A4A965C189287B99 textures=00000000
```

- `1EB86A9AE369C277` samples one texture: a real full-screen effect.
- `A4A965C189287B99` samples **no textures at all**. Its guest ucode is three
  instructions:

  ```
  alloc colors
  exece
  max oC0, r0, r0
  ```

  It writes a constant colour and does nothing else.

The guest state at those draws:

```
[wo2-const] blend=00000000 blend1=00000000 mask=00000000 mode=00000004 surface=00000000
```

`RB_COLOR_MASK` is zero (no colour channels written) and both blend controls are
zero, so this full-screen triangle is not producing an image - it is a masked
fill, i.e. a clear-type pass implemented by the title as a full-screen triangle.
It costs 0.7-0.9 ms per draw and appears roughly 10-20 times per frame, which is
7-14 ms of a ~40 ms frame.

That is the one place where "do the EDRAM work in software" could pay off:
detect a draw whose pixel shader is a constant writer, whose colour mask is zero
(and/or whose destination is a depth/stencil clear) and whose geometry covers the
target, and replace it with a hardware clear of the corresponding EDRAM region
(`ClearUnorderedAccessViewUint` on the ROV buffer, or the matching depth/stencil
clear) instead of a full-screen triangle draw. A clear costs on the order of
0.05 ms, against 0.7-0.9 ms for the triangle, so the expected saving is most of
those 7-14 ms per frame.

Not implemented: it needs the clear value and the region extracted from the guest
state, plus care with the EDRAM tiling, and it only applies while the colour mask
is zero and nothing else in the pass contributes. It is the one remaining change
with a large expected payoff.

### Implemented and measured: neutral in battle

The skip was implemented in `IssueDraw` (patch
`rexglue-wo2-draw-gpu-timings.patch`, cvar `wo2_skip_empty_fullscreen_draws`,
default off): after `RenderTargetCache::Update` has done all the state work, a
draw is dropped when the normalised colour mask is zero, depth test, depth write
and stencil are all disabled, the pixel shader does not memexport and no
occlusion query is active - i.e. a draw that provably cannot write anything.

Measured in battle, skip on versus off, two runs each, same start-up path:

| run | guest FPS | frame |
|-----|-----------|-------|
| skip on | 31.5 | 31.78 ms |
| skip off | 31.8 | 31.44 ms |

The counter confirms it fires a lot (`empty full-screen draws skipped total=84000`
in about 50 seconds, roughly 50 draws per frame), but the frame time does not
change. Those draws are cheap: the expensive masked-clear instances we saw in the
heavy-draw samples happen only once or twice per frame, so they are worth about
1-2 ms at most, which is inside run-to-run noise. The many cheap no-ops it also
removes were already nearly free.

Conclusion: the optimisation is correct but does not pay off in battle, so the
cvar is left off by default. The frame is dominated by the `1EB86A9AE369C277`
full-screen quad effect, which samples a texture and is real work.








