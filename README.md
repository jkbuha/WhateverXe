# WhateverXe

[日本語](README_ja.md)

WhateverXe is a Lilu plugin that adapts Apple graphics drivers for selected
Intel Xe-LP integrated GPUs. It is derived from WhateverGreen.

This project is experimental and requires the signed graphics package from
[WhateverXe Graphics](https://github.com/b00t0x/WhateverXe-Graphics). It installs
the graphics stack under `/Library` and updates the Auxiliary Kernel Collection
(AuxKC), without patching System files or snapshots. WEX and the graphics package
have independent release versions.

## Compatibility

One WEX binary and one graphics pkg cover the following macOS versions:

| macOS | Required version |
| --- | --- |
| Big Sur | 11.7.11 |
| Monterey | 12.7.6 |
| Ventura | 13.7.8 |
| Sonoma | 14.8.9 |
| Sequoia | 15.8.x |
| Tahoe | 26.7 or newer |

Big Sur through Sonoma require the qualified kernel identities. Sequoia and
Tahoe accept servicing updates within their OS family while retaining runtime
instruction and ABI checks. Future updates are not prevalidated.

WEX selects the OS ABI and GPU backend independently. OS-specific graphics
payloads and the common AppleIntelTGLCompat provider are installed by WhateverXe
Graphics; GPU generation and connector routing are handled by WEX.

| GPU family | Device IDs with an implemented backend |
| --- | --- |
| Tiger Lake | `9A40`, `9A49`, `9A78` |
| Alder Lake-N / Twin Lake | `46D0`–`46D4` |

Support varies by system and connector layout. The table includes implemented
backends, not a guarantee for every machine. See [device selection](docs/device-selection.md)
for detailed selection conditions and overrides.

## Required components

Download the **RELEASE** ZIP from [Releases](https://github.com/b00t0x/WhateverXe/releases).
The DEBUG ZIP is for troubleshooting. Each ZIP contains only `WhateverXe.kext`
and its matching dSYM. `SHA256SUMS` contains checksums for both archives.
See [CHANGELOG](CHANGELOG.md) for release notes.

| Component | Role |
| --- | --- |
| Lilu | Runtime required by WhateverXe |
| WhateverXe.kext | Load through OpenCore after Lilu |
| [WhateverXe Graphics pkg](https://github.com/b00t0x/WhateverXe-Graphics/releases) | Install the signed graphics stack into `/Library` and rebuild AuxKC |
| [AMFIPass v1.4.1 Mod](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/extras/AMFIPass-v1.4.1-Mod-RELEASE.zip) | Load through OpenCore for the modified graphics stack |

Configure SIP with `csr-active-config=03080000` (value `0x803`). Full SIP is not
supported. With the signed graphics package and linked Mod AMFIPass,
`amfi=0x80` and `ipc_control_port_options=0` are not required.

Tahoe requires `CoreMedia / AllowOnePassMetalScaling` to be disabled; the
WhateverXe Graphics installer applies this setting.

Boot the target OS with WEX **0.3.0 or newer** before installing the graphics
pkg. If an older Xe System patch is installed, restore it using its original
recovery procedure first. The pkg does not change EFI, System files, snapshots
or SIP settings. See [installation and removal](https://github.com/b00t0x/WhateverXe-Graphics#install)
for the package instructions and the standalone uninstall script.

Extract `AMFIPass.kext` from the linked **Mod** archive, place it in
`EFI/OC/Kexts`, and enable it in `Kernel > Add`. Replace an existing AMFIPass
with this Mod build; do not load both copies. Upstream AMFIPass does not support
the Mod signing chain.

If you use OCLP for other root patches, use the
[custom OCLP build](https://github.com/b00t0x/OpenCore-Legacy-Patcher), which uses
the same Mod signing chain. Xe graphics installation itself uses WhateverXe
Graphics independently of OCLP.

WhateverGreen and WhateverXe must not be enabled together, including in
mixed-GPU configurations.

## Configuration

GPU selection is automatic from the PCI device ID. Connector routing is read
from firmware VBT where supported; fixed HDMI and fixed DisplayPort layouts
normally need no manual wiring properties. Advanced `device-id` overrides are
described in [device selection](docs/device-selection.md).

On supported GPUs, WhateverXe also normalizes the detected Intel Management
Engine device name to `IMEI`, as WhateverGreen does. A separate ACPI rename
from `HECI` or `MEI` to `IMEI` is unnecessary.

### DVMT

Set **DVMT Pre-Allocated to 96MB** in the BIOS/UEFI settings where possible.

If it is fixed at **60MB**, add `-igfxdvmt` to `boot-args` and set both properties
below on the iGPU under OpenCore's `DeviceProperties > Add`
(usually `PciRoot(0x0)/Pci(0x2,0x0)`).

| Property | Type | Value (hex) |
| --- | --- | --- |
| `framebuffer-patch-enable` | Data | `01000000` |
| `framebuffer-stolenmem` | Data | `0000C003` |

These Data values use little-endian byte order. `-igfxdvmt` corrects the capacity
calculation, while the properties limit the display memory pool to fit within
60MB. Neither setting increases the memory allocated by the firmware.
With 96MB, these DVMT-specific settings are unnecessary.

### Required boot arguments

Add the following to OpenCore's `boot-args`:

```text
-allow3d -disablegfxfirmware
```

| Argument | Purpose |
| --- | --- |
| `-allow3d` | Enable the Apple driver's 3D path. |
| `-disablegfxfirmware` | Disable the Apple driver's graphics-firmware loading path for the current backend. |
| `-wex-nopanel` | Additionally required on Tiger Lake systems without an internal display. Alder Lake-N / Twin Lake currently builds an external-display topology automatically and does not use this flag. |

No family-selection or per-feature enable arguments are required for
Alder Lake-N / Twin Lake.

### Optional boot arguments

| Argument | Purpose |
| --- | --- |
| `gfxrst=` | Boot framebuffer handoff: 0 automatic (default), 1 reset, 2 copy, 3 clear, 4 disable. |
| `-wexdbg` | Enable debug logging in a debug build. |
| `-wexoff` | Disable WhateverXe. |
| `-wexbeta` | Bypass the plugin's OS-version check. Exact kernel/driver checks still apply. |
| `-wex-rplp-probe` | Diagnostic-only GPU fuse/engine, powered-pipe and read-only firmware-memory capture for physical Raptor Lake-P A7A0 revision 4 on Tahoe. Uses a bounded GT/render forcewake lease; does not enable acceleration or program displays. |
| `-wex-rplp-bcs` | Experimental bounded copy-engine command probe for physical A7A0 revision 4 on Tahoe. Requires `-wex-rplp-probe` and a validated firmware-memory handoff. Allocates and retains its test context until reboot; does not enable normal acceleration. |
| `-wex-rplp-rcs` | Experimental bounded render-command probe for physical A7A0 revision 4 on Tahoe. Requires `-wex-rplp-probe`; use separately from `-wex-rplp-bcs`. Retains its test context until reboot and does not enable normal acceleration. |
| `-wex-rplp-native-probe` | Experimental native accelerator probe for physical A7A0 revision 4 on Tahoe. Requires `-wex-rplp-probe` and separate use from the command probes. Verifies the exact signed donor, enables device selection, and blocks native `start()` before GPU initialization. |
| `-wex-rplp-native-memory` | Experimental native memory-mapping diagnostic. Requires both RPL-P probe arguments. Initializes native BAR mapping and address segments, then deliberately fails before dummy-page allocation or GGTT initialization; acceleration remains disabled. |
| `-wex-rplp-native-pages` | Experimental native dummy-page allocation diagnostic. Requires both RPL-P probe arguments and `-wex-rplp-native-memory`. Applies allocation-failure cleanup protection, checks both CPU buffers, and stops before GGTT initialization. |
| `-wex-rplp-gtt-shadow` | Experimental native GGTT constructor test on owned RAM. Requires all RPL-P native probe, memory and page arguments. Preserves the reserved prefix, checks 64-bit PTE stores and cleanup, and keeps the real GGTT untouched unless the separate slot diagnostic is requested. |
| `-wex-rplp-cache-transaction` | Experimental 103-register cache-policy write/readback/restore test. Requires a successful cache-shadow test, verified idle 96-EU/GOP state and administrator-controlled manual arming. Preserves firmware MOCS61, PAT0 and L3 layout; restores every attempted write. Does not enable rendering or acceleration. |
| `-wex-rplp-caps-shadow` | Experimental read-only capability capture and native capability/mode replay on owned RAM. Requires successful cache-shadow/cache-transaction diagnostics and manual arming. Does not program hardware engine modes or admit acceleration. |
| `-wex-rplp-native-engines` | Experimental grouped render/blitter command test using native ring-field, mode and HWS helpers with the verified Gen12 register template. Requires successful native-memory/capability tests and manual arming. Uses separate GGTT ranges; submitted contexts remain pinned until reboot. Does not enable shaders or macOS acceleration. |
| `-wex-rplp-donor-fixes` | Experimental exact-donor RPL-P context power, preemption, Tahoe plane-count and compression-policy adaptations. Requires manual arming and the native-engine diagnostic. Accelerator startup remains blocked. |
| `-wex-rplp-reference-backend` | Enable the optional exact-machine RPL-P reference backend; requires its special build and root manual arming. The ordinary TGL and diagnostic backends are bypassed. |
| `-wex-rplp-fb` | Enable the optional RPL-P panel backend only in a build supplied with its exact reference framebuffer donor. Requires the reference backend and manual root arming; the default signed framebuffer is not compatible with these hooks. |
| `-wex-rplp-native-barriers` | Experimental RPL-P dependency barriers after native accelerator publication. Requires the optional panel backend and manual arming; validates native contexts before leaving the legacy startup path. |
| `-wex-rplp-backlight` | Opt-in UID-19, 16-bit PWM brightness profile for the optional RPL-P panel backend. Preserves Apple/custom profiles and excludes external displays. |
| `-wex-rplp-gpu-recovery` | Opt-in RPL-P shared render-domain reset recovery for the exact reference GPU image. Restores and replays a qualified sibling engine after a native reset; requires native accelerator publication. |
| `-wex-rplp-forcewake-balance` | Opt-in RPL-P forcewake counter balancing for the exact reference GPU image. Automatic release of idle holds remains disabled. |
| `-wex-rplp-manual-arm` | Hold the RPL-P native diagnostic until an administrator signals the `WhateverXeRPLPControl` service with `BeginDiagnostic=true`. Requires the existing native-probe prerequisites. Allows restoring working boot files before an experimental diagnostic starts. Does not grant normal accelerator admission. |
| `-wex-rplp-subranges` | Experimental native descriptor allocation, exhaustion, mapped backing-memory aliasing, restoration, release/reuse and non-custom page-pool registration. Requires the complete factory and successful cache-shadow diagnostics. Uses an owned RAM page; uncertain child ownership retains its manager until reboot. Does not enable acceleration. |
| `-wex-rplp-cache-shadow` | Experimental native cache initializer replay on owned RAM, with balanced mock forcewake and full RAM/guard verification. Requires the full native-factory diagnostic. Also inspects existing cache registers under a bounded forcewake lease; does not write hardware caches or enable acceleration. |
| `-wex-rplp-native-factory` | Experimental complete memory-factory construction and cleanup test. Requires the integrated manager-pool diagnostic. Continues only after stable capability reads show eDRAM disabled, uses that measured result instead of the legacy forcewake probe, then explicitly releases the pool array and manager. Does not start the accelerator or program GT caches. |
| `-wex-rplp-manager-pools` | Experimental real-manager pool-array construction and cleanup test. Requires the page-array and empty page-pool diagnostics and their prerequisites. Builds the array inside the native manager, then forces the original failure unwind unless `-wex-rplp-native-factory` is enabled. Stops before cache programming. Does not enable acceleration. |
| `-wex-rplp-page-array` | Experimental per-CPU native page-pool array test after the empty page-pool diagnostic passes. Uses a private CPU-memory manager record, checked pool construction and corrected partial-failure cleanup, then releases the array. Does not enable the full memory manager or GPU engines. |
| `-wex-rplp-page-pool` | Experimental empty native page-pool constructor test. Requires the native-chain and native-pools diagnostics to pass. Uses checked allocations, validates the empty containers, then releases the object. The memory-manager page-pool gate remains blocked; no GPU engines or acceleration are enabled. |
| `-wex-rplp-native-chain` | Experimental memory-manager construction test after the slot/aperture checks. Requires all native pool diagnostic prerequisites. Supplies owned RAM for the native pool, allows the factory to construct its pool and fence allocator, then stops before page-pool initialization unless `-wex-rplp-manager-pools` is enabled. Verifies cleanup before releasing backing RAM. Does not allocate physical stolen memory or enable GPU engines. |
| `-wex-rplp-native-pools` | Experimental native allocator-construction test after a successful aperture diagnostic. Uses one owned RAM page as the backing for a native stolen-memory pool, checks its descriptor and a native fence allocator, then releases the native objects before their backing page. Does not allocate physical stolen memory, program fences, or submit GPU commands. |
| `-wex-rplp-gtt-aperture` | Experimental read-only aperture check during the bounded GGTT slot test. Requires `-wex-rplp-gtt-slot` and all its prerequisites. Flushes the native dummy page, checks all 1024 words through an uncached aperture mapping, then restores the firmware PTE even on a mismatch. No GPU commands or aperture writes. |
| `-wex-rplp-gtt-slot` | Experimental one-entry hardware GGTT test. Requires the shadow and pin-pages diagnostics and every native probe prerequisite. Rechecks idle GT and the panel handoff, requires the exact identity mapping of a spare page at 10MiB beyond the active scanout, verifies the bounded native write, then restores the original PTE. Retains pinned buffers until reboot if restoration fails or another entry changes. Does not start the accelerator or submit GPU work. |
| `-wex-rplp-pin-pages` | Experimental buffer-lifetime check during `-wex-rplp-gtt-shadow`. Adds an extra prepare/retain hold, verifies preparation IDs and physical stability after native cleanup, then balances and releases the extra holds. |
| `-wex-tgl-off` | Disable the Tiger Lake path. |
| `-wex-noaccel` | Tiger Lake framebuffer-only mode; blocks accelerator probing. |
| `-wex-edp-probe` | Enable bounded internal-panel AUX/DPCD diagnostics. Required with `-wex-edp-routing` for the guarded panel path; this flag alone does not enable the panel. |
| `-wex-edp-routing` | Experimental panel-port routing, pre-training clock selection, native display/AUX power control, Combo PHY/two-lane training, source framing and null hotplug-path protection on Tahoe for physical Tiger Lake 9A49 revision 1. Requires `-wex-edp-probe` and exact donor instruction checks. |

## Tested features and known issues

Tiger Lake and Alder Lake-N have been tested across all six OS families with
dual external 1920×1080 at 60 Hz output, Metal/MPS, OpenGL, OpenCL, hardware
H.264 encode/decode, hardware HEVC decode, display sleep/wake, Safari hardware
video decode and Widevine playback. The common TGLCompat build has also passed
installation and functional checks on Alder Lake-N across all six OS families.
These checks do not establish compatibility with every codec, application or GPU
in the table.

The guarded Tahoe path for physical Tiger Lake `9A49` revision 1 has also passed
internal 1920×1200 panel startup and Metal 2 command completion with GPU buffer
readback. It holds the qualified native render-compression policy off at startup
and subsequent platform updates. External-display hotplug, system sleep/wake and
extended stability remain unverified for this internal-panel configuration.

### Known issues and limits

- VA image conversion can crash with a null-pointer access.
- DisplayPort reconnection can cause a black screen or system hang, including
  on Sequoia and Tahoe.
- Occasional Safari frame drops have been observed.
- Three simultaneous displays, system sleep, higher resolutions and other
  connector layouts require further validation. USB-C and internal display
  support are planned below.

## Roadmap

### Planned

- Alder Lake and Raptor Lake S/H/P/U support
- USB-C display output
- Internal displays

### Not Planned

- Xe-LPG support
- Xe2 and later architectures

## Build and test

Use macOS with Xcode command-line tools, Python 3.9 or newer, MacKernelSDK and
Lilu's development headers/library. CI uses the Lilu
[1.7.2 release tag](https://github.com/acidanthera/Lilu/tree/1.7.2),
which includes the Tahoe kernel definition. Place `MacKernelSDK/` and `Lilu.kext/` in the
repository root, or pass their paths with `--sdk` and `--lilu`.

```sh
python3 tools/build.py --release
python3 tools/test.py
```

WhateverXe and AppleIntelTGLCompat kexts and a source manifest are written to
`build/`. TGLCompat source and shared headers are maintained here; its signed
binary is distributed through WhateverXe Graphics. The WEX release ZIP contains
only the OpenCore plugin, not the graphics package. Optional donor replay tests
accept `--donor /path/to/AppleIntelTGLGraphicsFramebuffer` and
`--accelerator /path/to/AppleIntelTGLGraphics`, using the exact binaries accepted
by each test. CPU tests do not establish physical GPU compatibility.
WhateverGreen source is not a build dependency.

## License and credits

WhateverXe retains WhateverGreen's copyright notices and BSD-3-Clause license.
See [LICENSE.txt](LICENSE.txt). Lilu and MacKernelSDK are separate dependencies.

Optional RPL-P reference integration: build with `--rplp-reference-source /path/to/Hackintosh-XPS13Plus-IrisXe-Tahoe` at commit `d0c046a441fa467ac492770aeeefe9636b97c65e`. This experimental build incorporates separately licensed TSNPL source and retains its notices in the build output. It is not a BSD-only distribution. The default build excludes that source. The optional `--rplp-reference-framebuffer /path/to/AppleIntelTGLGraphicsFramebuffer.kext` adds the exact matching panel donor contract; Apple binaries remain separate build inputs.

Panel builds also produce `WhateverXeRPLPBridge.kext` for loading the reference framebuffer after its system graphics dependencies are available. This provider depends only on kernel KPIs and forwards its six operations to WhateverXe's qualified framebuffer owner. Loading it does not arm the GPU or enable native framebuffer matching. After manual arming, the panel backend uses IOKit matching with the signed personality's original probe priority; display-pipe creation requires an owned native framebuffer.
