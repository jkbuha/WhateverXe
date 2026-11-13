# xps-unified — one WhateverXe build for the Dell XPS 13 9320 and XPS 17 9720

This branch is WhateverXe 0.8.2 (b00t0x/WhateverXe `main`) plus the fixes both laptops need. Both machines run the **same kext**; the only per-machine difference is boot arguments.

| Machine | iGPU | Extra boot-args on top of the usual WhateverXe set |
| --- | --- | --- |
| XPS 13 9320 | RPL-P `8086:A7A0` | `-wex-resume-park` (hibernation resume) |
| XPS 17 9720 | ADL-H `8086:46A6` rev `0x0C` | `-wex-panel-handoff -wex-edp-keep -wex-edp-blank -wex-no-edp-hpd -wex-resume-park -wex-tc1-legacy -wex-tc4-legacy` |

## What's in it

- **9320** (from `xps9320-wex081`): mode-0 internal boot-port admission through the protected AUX lease; firmware CMTG capture/preserve; validated EDID sentinel; hibernation resume park (firmware eDP park, GGTT mode restore, deferred IRQ re-arm); scanout snapshot / busy-gate diagnostics.
- **9720** (from `xps9720-igpu` / `xps9720-hibernate` / `diag-9720-tc-live`):
  - renedis' ADL-H port: hand-off mode-0 admission (**only with `-wex-panel-handoff`**), single-output CDCLK, unaligned aperture, and the display IRQ master parking set;
  - `-wex-edp-keep` / `-wex-edp-blank` / `-wex-no-edp-hpd` (eDP freeze fix), the edp-blank system-sleep handback, `-wex-tcN-legacy` (VBT Type-C ports wired as legacy HDMI), `-wex-no-gva[-scaler]`.
- **Diagnostics** (opt-in or read-only): `-wex-canary` (memory canary), Type-C live/FIA readouts, display-audio / HDA readouts, `-wex-audio-wake` (iDisp codec wake; see b00t0x/WhateverXe#124).

## Rollback

- 9320: `/Volumes/EFI/xe-rollback-082.sh [kext|nowex]` (or `/boot/efi/…` from CachyOS) restores WhateverXe 0.8.2 `AF833022` and the previous `config.plist`.
- 9720: `/Volumes/EFI/rollback-unified.sh` restores the previous 9720 config/kext.

## Superseded branches

`xps9320-wex081`, `xps9720-igpu`, `xps9720-hibernate` and `diag-9720-tc-live` are kept for history. New work goes on `xps-unified`.
