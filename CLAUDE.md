# CLAUDE.md

This file guides Claude Code when it works in this repository.

## What this is

A Windows screen saver (`.scr`) built with Direct3D 11. Particles connected by springs are simulated on the GPU with compute shaders and drawn as textured point sprites. Every `m_fRandomizeInterval` seconds the spring topology is re-randomized. When `m_bAlternateUnbound` is set, every other change is an "unbound" fly-apart pattern instead.

## Build

- Visual Studio 2022, toolset **v143**, Windows 10 SDK (latest), **x64 only** (Debug|x64 and Release|x64). There is no Win32 configuration any more.
- Command line:
  ```
  MSBuild.exe InteractDX11.sln -p:Configuration=Release -p:Platform=x64 -m
  ```
  MSBuild is at `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`.
- Output goes to `x64\<Config>\`. The post-build step copies `InteractDX11.exe` to `InteractDX11.scr` and copies `particle.dds` next to it. HLSL files are compiled by FxCompile (SM 5.0) to `.cso` files in the same directory.
- The `Debug\` and `Release\` folders in the repo root are stale output from the old 32-bit builds. They are git-ignored.
- There are no tests. "Verified" means it builds with zero warnings at /W3 and runs.

## Running

The mode depends on the executable's extension and the standard screen-saver switches (`CSaverEngine::ParseForOptions` in `SaverEngine.cpp`):
- `InteractDX11.exe` with no arguments runs in a normal resizable window. This is the easiest way to debug.
- `.scr /s` is full-screen saver mode. Every monitor runs its own independent `CInteractSaver`, with its own D3D device on that monitor's adapter (`CWindowCluster::m_bMultiRender`, which is true only in this mode). The savers are ticked one after another in a single loop, so only the last *active* one (running and not occluded, chosen in each pass of `CWindowCluster::Run`) presents with vsync (`SetSyncInterval`) and the others use `Present(0)`. Otherwise N monitors would run at 1/N of the refresh rate. The `m_iSaverIndex`/`m_iNumSavers` values stagger each monitor's spring re-randomization. The black cover windows (`AddSecondary`) remain as code but aren't used in this mode.
- `.scr /p <hwnd>` is the preview pane nested inside the Control Panel window. The HWND is parsed as a 64-bit decimal value. The host lives in another process, so `CWindowCluster::Run` checks `IsWindow(hParentWnd)` every frame and quits once the host is gone. Otherwise the process could linger invisibly.
- `.scr /c[:hwnd]`, or `.scr` with no arguments, opens the config dialog.

At runtime the shaders (`*.cso`) and `particle.dds` are loaded **from the executable's directory** (`m_strShaderPath` and `m_strGFXResoucePath` in `SaverBase.cpp`). These files must sit next to the exe or `.scr`.

## Architecture

- `InteractDX11.cpp`: `_tWinMain`. It initializes COM and runs `CSaverEngine`. The `WndProc` and `About` functions in this file are leftovers from the VS wizard.
- `SaverEngine`: parses the command line, decides the display mode, and creates windows through `CWindowCluster`, using one window per monitor via `EnumDisplayMonitors`.
- `WindowCluster`: owns the window set and the shared window procedure (through the static `s_pTheCluster`). It also runs the message loop and the input handling that closes the saver.
- `SaverBase` (abstract): handles D3D11 device and swap-chain setup, resizing, the timer (`DrawTimer`), and shader and texture loading helpers. Subclasses implement `InitSaverData`, `IterateSaver(dt, T)`, `PauseSaver`, `ResumeSaver`, `OnResizeSaver` and `CleanUpSaver`.
- `InteractSaver`: the actual saver.
  - Particle position and velocity live in two structured buffers that swap roles each frame (`m_pSBPosVel` and `m_pSBPosVelNext`).
  - `ConfigSpringsCS` fills a spring lookup texture for the current pattern.
  - `PhysicsCS` integrates the forces.
  - `RenderVS`, `RenderGS` and `RenderPS` expand each particle into a billboarded, textured quad.
  - Resource setup runs in parallel in `concurrency::task_group m_InitTasks` (PPL).
- `ConfigData`: the tunable parameters. `Save()` is a stub and `Reload()` only sets hard-coded defaults, so nothing is persisted yet.
- `ConfigDialog`: the config dialog (`IDD_CONFIGDLG` in `InteractDX11.rc`).
- `DDSTextureLoader`: a vendored copy of Microsoft's DirectXTK loader. Avoid editing it.
- `Cube.*`, `CubeSaver.*`, `VertexShader.hlsl`, `PixelShader.hlsl` and `particleold.dds` are **not in the project**. They are unused samples.

## Conventions and gotchas

- The constant-buffer structs in `InteractSaver.h` (`WORLD_PHYSICS`, `FRAME_VARIABLES`, `SPRING_CONFIG`) must match the `cbuffer` layouts in the HLSL files exactly, including the 16-byte padding fields (`*Dummy*`). Change both sides together.
- `CConfigData::m_iBlockSize` (128) must agree with the thread-group size used in `PhysicsCS.hlsl`.
- The project uses precompiled headers through `stdafx.h`, so every `.cpp` must include it first. It also uses Unicode (`TCHAR`/`_T`), MS-style Hungarian naming, `ComPtr` from WRL, and DirectXMath.
- Write 64-bit-clean code. Use `INT_PTR`/`LRESULT` for callbacks and `*LongPtr` APIs, and never truncate pointers or HWNDs to `long` or `DWORD`. Use `static_cast<int>` when narrowing `size_t`.
- `targetver.h` pins `_WIN32_WINNT` to Windows 7.
- **Idle and power behavior:** keep these intact, since a screen saver runs for hours.
  - `CSaverBase::PresentFrame` treats `DXGI_STATUS_OCCLUDED` (for example a locked session) as "stop simulating". It freezes the clock and re-tests every 250 ms with `DXGI_PRESENT_TEST`.
  - `CWindowCluster` registers for `GUID_CONSOLE_DISPLAY_STATE` and for WTS session notifications, and pauses every saver while the displays are off or the session is locked (`IsHidden()`).
  - Occlusion alone isn't enough: on the Windows 11 lock screen, DXGI reports occlusion for only about half a second and then lets presents continue.
  - Resizes that arrive while hidden re-pause after `ResumeResized`.
  - When no saver rendered in a pass, `Run()` blocks in `MsgWaitForMultipleObjectsEx` (100 ms cap) instead of spinning. Always present through `PresentFrame()`, never directly.
- Savers that don't depth-test set `m_bUseDepthStencil = false` in their constructor (`CInteractSaver` does). No depth buffer is then created, and `m_pDepthStencilView` is null.
- **DPI:** `InteractDX11.manifest` (merged via `AdditionalManifestFiles`) declares Per-Monitor V2 DPI awareness. All window and monitor coordinates are therefore physical pixels: the saver renders at the native resolution (for example 3840×2160 at 200%), and the `/p` preview child fills the parent's real client rect. Don't remove the manifest or call DPI-unaware sizing APIs. `CSaverBase::Initialize` sizes the swap chain from the window's actual `GetClientRect`, and `WM_DPICHANGED` is handled for windowed mode.
