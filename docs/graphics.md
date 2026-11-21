# Graphics: Direct3D over the Vulkan renderer

The renderer (`recomp/runtime/gfx/`) is in since v0.2.0. What is missing is the layer that turns the game's Direct3D 8 calls into its draws: GBTGhoulies--QilinRecomp's `recomp/runtime/host/d3d.cpp` (3,700 lines; its `docs/graphics.md` describes the design). This file records what bringing it to this game's XDK build (5558; the sister game's is 5659) has established so far, so the work continues from facts and not from that game's addresses.

**Nothing of Direct3D runs yet.** The run stops at `Direct3D_CreateDevice` (`0x173C40`), called from the engine's display start-up at `0x0F1B67`.

## What the design needs from the build
`d3d.cpp` keeps the library's state tables in guest memory in the original layout (the game writes render and texture-stage states inline), lifts the library's pure resource code, and replaces what drives the GPU. It therefore depends on about 110 addresses: the library's globals, fields of its device object, a few of its constant tables, and some forty of its functions. Each must be found in this build.

## Findings

### The same source, linked in another order
- **The device object's fields are where the sister build has them**, at every one checked by reading this build's code: the fence (`+0x2C`, `D3DDevice_SetRenderTarget`), the vertex shader (`+0x790`, `D3DDevice_SetVertexShader`), the reference count (`+0x934`, `D3DDevice_Release`), the depth range and format (`+0x944`, `+0x948`), the render target and depth buffer (`+0x19F4`, `+0x19F8`), the frame buffers (`+0x1A04`, `D3DDevice_GetBackBuffer2`). The device is 0x924 dwords (`D3DDevice_Release` clears that many).
- **The library's functions are in a different order** (the offset between the two builds' addresses changes from function to function, and is even negative for some), so no function address carries over by arithmetic.
- **The state tables keep their layout and move together**, by `0xA0C0` from the sister build's, wherever a symbol or this build's code confirms it:

| What | Here | Confirmed by |
|---|---|---|
| `D3D__DirtyFlags` | `0x1778A8` | read: `D3DDevice_SetTransform`, `SetTextureStageStateNotInline` |
| `D3D__TextureState` (32 dwords a stage) | `0x1778B0` | symbol |
| `D3D__RenderState` | `0x177AB0` | symbol; `SetRenderStateNotInline` |
| the deferred and complex render states | `0x177C20`, `0x177CD0` | symbols (each `D3DRS_` slot named) |
| `D3D__pDevice`, the device object | `0x177D48`, `0x177D50` | symbol; `Direct3D_CreateDevice` |
| the vertex shader constants' shadow (16 bytes a register) | `0x176C90` | read: `D3DDevice_SetVertexShaderConstant1` |
| the stream table | `0x17A1E0` | symbol |

- **The library's own variables at the end of its section do not follow that offset.** The push buffer's size and kick-off size are at `0x17A4D4` and `0x17A4D0` (`Direct3D_CreateDevice` gives them their defaults, `0x80000` and `0x8000`), not where the offset would put them. Everything in that area (the fixed-function shader objects among them) is to be read, not computed.
- **Its constant tables in `.rdata` moved by another amount.** The simple render states' table is at `0x20B630` (`SetRenderStateNotInline`).

### Functions by name
XbSymbolDatabase names 151 Direct3D symbols here (`xdk_symbols.inc`), which covers every function the game calls but three, and these that `d3d.cpp` calls or refers to: `D3DDevice_GetViewportOffsetAndScale` `0x168D40`, `D3D_CommonSetMultiSampleModeAndScale` `0x169BA0`, the complex render-state setters, `D3DDevice_SetVertexShader` `0x16B0B0`, `D3DDevice_LoadVertexShader` `0x16AD60`, `D3DDevice_SelectVertexShader` `0x16ADC0`, `D3DDevice_CreateVertexShader` `0x16A960`, `D3DDevice_SetRenderTarget` `0x167530`, `D3D_DestroyResource` `0x16D440`, `D3D_LazySetPointParams` `0x1700E0`, `CDevice_SetStateVB` `0x171720`, `CDevice_SetStateUP` `0x1718C0`, `D3DDevice_MakeSpace` `0x16C290`.

### Unnamed helpers identified by reading
| Here | What | The sister build's |
|---|---|---|
| `0x174900` | the library's object creation: `mov eax, 1; ret 4` (lifted) | |
| `0x16D520` | a surface's internal reference (and its texture's): called by `SetRenderTarget` on the new target and depth buffer | `0x175970` |
| `0x16D750` | dropping one, destroying the resource at none (calls `D3D_DestroyResource`) | `0x175B90` |
| `0x16DB50` | a depth buffer's largest depth value, into the device (`+0x944`) | `0x17B080` |
| `0x167370` | a depth buffer's format bits (`+0x948`) | `0x174080` |
| `0x1716A0` | the state flush that calls the point, fog and light set-up in turn (`D3D_LazySetPointParams` first) | `0x1799A0` |
| `0x1707C0` | the fog unit's set-up (called from that flush) | `0x178AC0` |
| `0x16CD70` | the device's initialisation, called by `Direct3D_CreateDevice` with the presentation parameters | |
| `0x16D240` | the device's teardown (`Direct3D_CreateDevice` on failure, `D3DDevice_Release`) | |

Still to find, by following calls from the named functions: the device's default state; the fixed-function vertex shader object and the function that builds it for an FVF (`SetVertexShader` here goes through `0x16A460` for one); the vertex shader object's builders (`CreateVertexShader` calls `0x16A860`); `LoadVertexShader`'s shadow copies (`0x16A5A0`); the viewport program (`0x169880` calls `GetViewportOffsetAndScale`); the render target's anti-aliasing mode (`0x16DA30` or `0x169A70`, both called by `SetRenderTarget`); the texture and surface code (`CreateTexture2`, `GetSurfaceLevel2`, `LockRect`, `Lock2DSurface`, `Get2DSurfaceDesc` are named; their shared helpers are not).

### What differs between the builds
`D3DDevice_SetVertexShader` here loads and selects a programmable shader through `D3DDevice_LoadVertexShader` and `D3DDevice_SelectVertexShader`; the sister project's notes describe its build's differently. So the library's code is not identical between the two XDKs, and each function `d3d.cpp` mirrors ("as the original does") has to be read here before its mirror is trusted.

### What the game uses
From the stubs (`docs/code-analysis.md`): 48 functions. No index buffers and no `DrawIndexedVertices`; `DrawVertices` and `DrawVerticesUP`; vertex shaders the engine generates (`baGenerateShader`), with `SetVertexShaderConstant1`/`4`/`NotInline` and `SetShaderConstantMode`; fixed-function lights and materials are also set (`SetLight`, `LightEnable`, `SetMaterial`, `SetTransform`); palettes (`SetPalette`, `D3DPalette_Lock2`), which the sister game doesn't use; the overlay for movies; `PersistDisplay`. Functions the sister project replaces that this game never calls (pixel shaders by `CreatePixelShader`, `BeginPush`/`EndPush`, immediate mode, fences, `SetModelView`) can wait.

## Plan
1. **First frame:** `Direct3D_CreateDevice` (the renderer, the frame buffers as guest surfaces, the device's fields as the lifted code reads them), `Clear`, `Swap` with the vertical-blank callback, the window on the first host thread, the settings menu over it. Each mirror of the library's own code checked against this build.
2. **Draws:** the state flush, streams and vertex shaders, textures and palettes, `DrawVertices`/`DrawVerticesUP`.
3. **The rest the game calls:** lights and materials through the fixed-function path, render targets, the overlay with XMV.
4. The game-specific parts of the sister project's file (its frame clock, its HUD and aspect code) have no counterpart here yet: the frame clock and the projection are to be found in the engine (`docs/roadmap-and-design-targets.md`, steps 7 and 8), and until then only the original frame rate and 4:3 apply.
