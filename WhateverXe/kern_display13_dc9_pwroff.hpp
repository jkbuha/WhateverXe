// Opt-in (-wex-d13-dc9-pwroff) Display13 Deep Idle power-well teardown &
// full eDP wake restore, aligned with keenerdavid18 XE-EDPWAKE
// (thinkpad-e16-gen3-hackintosh main @ 8ad32741, 0003-deep-idle-sleep.diff).
//
// Why this exists:
//   On Alder Lake-P / Raptor Lake-P Display13 (XE_LPD), Display13Power marks
//   firmware/pre-existing PWR_WELL_CTL requests as borrowed and Display13DBUF
//   pins s.initialRequests, while PWR_WELL_CTL_BIOS (0x45400) is never cleared.
//   As a result, Apple's overridePowerWellsState(false) + enableDC9() leaves
//   PWR_WELL_CTL1 (0x45400) and PWR_WELL_CTL2 (0x45404) active during Deep
//   Idle, hard-capping the SoC package at PC2.
//
// Design (aligned with keenerdavid18 XE-EDPWAKE + Linux i915
// icl_display_core_uninit / bxt_enable_dc9 / tgl_ddi_pre_enable_dp /
// intel_dp_link_training):
//   1. Snapshot the live eDP Pipe A / DDI A state and DPCD link parameters
//      before sleep teardown (dynamic DPLL0/1/4, timings, M/N,
//      TRANS_DDI_FUNC_CTL, TRANS_DDI_FUNC_CTL2 with X1C10/X1Y7 CMTG bit 3,
//      TRANS_MSA_MISC, Combo PHY A swing/pre-emphasis, PPS & BLC).
//   2. On Deep Idle sleep (enableDC9): run icl_display_core_uninit order:
//      planes/cursor/transcoder/TRANS_DDI_FUNC_CTL off -> sink D3 + DDI/DP_TP
//      off -> PPS off -> active DPLLs off + DDI A clock gate -> all 4 Display13
//      DBUF slices S3..S0 (0x44304, 0x44300, 0x44FE8, 0x45008) off -> CDCLK PLL
//      off -> AUX/DDI IO wells off -> PW2+Pipe wells off (BIOS + Driver) ->
//      PW1 off (BIOS + Driver) -> DC_STATE_EN = DC9.
//      Wa_14010685332 (SOUTH_CHICKEN1 0xC2000 bit 7 held=1 through sleep and
//      cleared on wake) is delegated to S0ixStandby -s0ixrefclk, matching
//      XE-EDPWAKE (do not pulse 1->0 here).
//   3. On wake:
//      - Phase 2a (inside restoreDisplay13ResourcesLocked, BEFORE
//        prepareToExitSleep / prepareToEnterWake): exitDC9() clears DC_STATE_EN
//        bit 3 and restores zeroed PPS delays; WhateverXe restores PW1, PipeA,
//        Combo PHY A/B, Core registers, MBUS_CTL, CDCLK, and DBUF.
//      - Phase 2b (in wrapDisplay13SetPowerState, AFTER Apple's
//        prepareToExitSleep + prepareToEnterWake/hwSetMode complete — matching
//        XE-EDPWAKE's post-prepareToExitSleep ordering so hwSetMode never runs
//        on top of our replayed pipe): verifyOrRestoreEDP() checks TRANSCONF_A.
//        If native hwSetMode already brought Pipe A + DPCD link up cleanly,
//        preserve it (fixing zeroed TRANS_MSA_MISC if needed) without stomping;
//        otherwise run full XE-EDPWAKE restoreEDP() replay + 40 ms settle.
//   4. Verify panel came back (DPLL locked, DDI active, DPCD 0x202..0x204
//      CR+EQ+SymbolLock+interlane align, TRANSCONF active, PP_STATUS on). If
//      any check fails, latch fallbackTripped=true so subsequent sleeps fall
//      back to the existing no-poweroff path.
//
// Pure header, zero IOKit/XNU dependencies so every transition can be unit
// tested on host with ASan/UBSan against a hardware MMIO + DPCD model.
#ifndef kern_display13_dc9_pwroff_hpp
#define kern_display13_dc9_pwroff_hpp
#include <stdint.h>

namespace Display13DC9PwrOff {

// Power well registers (BIOS & Driver).
constexpr uint32_t PwrWellCtlBios    = 0x45400;
constexpr uint32_t PwrWellCtlDriver  = 0x45404;
constexpr uint32_t PwrWellAuxBios    = 0x45440;
constexpr uint32_t PwrWellAuxDriver  = 0x45444;
constexpr uint32_t PwrWellDdiBios    = 0x45450;
constexpr uint32_t PwrWellDdiDriver  = 0x45454;
constexpr uint32_t RequestBitsMask   = 0xAAAAAAAAU;
constexpr uint32_t StateBitsMask     = 0x55555555U;

// DC state & PCH chicken registers.
// Note: SOUTH_CHICKEN1 (0xC2000 bit 7, Wa_14010685332) is intentionally NOT
// written here; matching XE-EDPWAKE, S0ixStandby -s0ixrefclk holds bit 7=1
// through sleep once CTL1/CTL2==0 and clears it at wake before drivers resume.
constexpr uint32_t DcStateEn         = 0x45504;
constexpr uint32_t Dc56Mask          = 0x00000003U;
constexpr uint32_t Dc9Enable         = 1U << 3;
constexpr uint32_t SouthDplsGateReg  = 0xC2020; // WA 22019252566
constexpr uint32_t SouthDplsGateBit  = 1U << 29;

// DBUF slices S0..S3 (Display13 XE_LPD, matching kern_display13_dbuf.hpp &
// Linux intel_display_regs.h: S0=0x45008, S1=0x44FE8, S2=0x44300, S3=0x44304).
constexpr uint32_t DbufSliceRegs[4]  = {0x45008, 0x44FE8, 0x44300, 0x44304};
constexpr uint32_t DbufPowerRequest  = 1U << 31;
constexpr uint32_t DbufPowerState    = 1U << 30;

// CDCLK.
constexpr uint32_t CdclkCtl          = 0x46000;
constexpr uint32_t CdclkPllEnable    = 0x46070;

// DPLLs (DPLL0, DPLL1, DPLL4) & clock routing.
constexpr uint32_t DpcaCfgcr0        = 0x164280; // DPCLKA_CFGCR0
constexpr uint32_t DdiAClockOff      = 1U << 10;
constexpr uint32_t TransClkSelA      = 0x46140;

inline uint32_t dpllEnableReg(uint32_t index) {
    return index == 1 ? 0x46014U : index == 2 ? 0x46018U : 0x46010U;
}
inline uint32_t dpllCfgcr0Reg(uint32_t index) {
    return index == 1 ? 0x16428CU : index == 2 ? 0x164294U : 0x164284U;
}
inline uint32_t dpllCfgcr1Reg(uint32_t index) {
    return index == 1 ? 0x164290U : index == 2 ? 0x164298U : 0x164288U;
}

// Transcoder A / Pipe A / DDI A / Scaler / PPS / Backlight.
constexpr uint32_t TransHtotalA      = 0x60000;
constexpr uint32_t TransHblankA      = 0x60004;
constexpr uint32_t TransHsyncA       = 0x60008;
constexpr uint32_t TransVtotalA      = 0x6000C;
constexpr uint32_t TransVblankA      = 0x60010;
constexpr uint32_t TransVsyncA       = 0x60014;
constexpr uint32_t PipeSrcA          = 0x6001C;
constexpr uint32_t TransVsyncShiftA  = 0x60028;
constexpr uint32_t PipeDataM1A       = 0x60030;
constexpr uint32_t PipeDataN1A       = 0x60034;
constexpr uint32_t PipeLinkM1A       = 0x60040;
constexpr uint32_t PipeLinkN1A       = 0x60044;

constexpr uint32_t TransDdiFuncCtlA  = 0x60400;
constexpr uint32_t TransDdiFuncCtl2A = 0x60404;
constexpr uint32_t TransDdiCtl2Cmtg  = 1U << 3; // X1C10/X1Y7 GOP CMTG bit 3
constexpr uint32_t TransMsaMiscA     = 0x60410;
constexpr uint32_t DpTpCtlA          = 0x60540;
constexpr uint32_t DpTpStatusA       = 0x60544;

constexpr uint32_t DdiBufCtlA        = 0x64000;
constexpr uint32_t DdiBufEnable      = 1U << 31;
constexpr uint32_t DdiBufIdle        = 1U << 7;
constexpr uint32_t AuxChCtlA         = 0x64010;
constexpr uint32_t AuxChDataA        = 0x64014;

constexpr uint32_t PsWinPos1A        = 0x68170;
constexpr uint32_t PsWinSz1A         = 0x68174;
constexpr uint32_t PsCtrl1A          = 0x68180;

constexpr uint32_t TransConfA        = 0x70008;
constexpr uint32_t PipeMiscA         = 0x70030;
constexpr uint32_t PipeBottomColorA  = 0x70034;
constexpr uint32_t PipeFrmCountA     = 0x70040;
constexpr uint32_t CurCtlA           = 0x70080;
constexpr uint32_t CurBaseA          = 0x70084;
constexpr uint32_t PlaneCtl1A        = 0x70180;
constexpr uint32_t PlaneSurf1A       = 0x7019C;
constexpr uint32_t PlaneColorCtl1A   = 0x701CC;
constexpr uint32_t PipeMiscBpcDitherMask = 0x000000FCU; // bits 7:5 BPC + bit 4 dither en + bits 3:2 dither type

// Pipe A color management registers (Display13 / XE_LPD, Linux intel_color_regs.h).
// All reside in PW_A / PW_1 and lose MMIO + LUT SRAM state when DC9 powers off PW_A.
constexpr uint32_t CscCoeffA         = 0x49010; // 6 dwords: 0x49010..0x49024
constexpr uint32_t CscModeA          = 0x49028;
constexpr uint32_t CscPreOffA        = 0x49030; // 3 dwords: 0x49030..0x49038
constexpr uint32_t CscPostOffA       = 0x49040; // 3 dwords: 0x49040..0x49048
constexpr uint32_t OcscCoeffA        = 0x49050; // 6 dwords: 0x49050..0x49064
constexpr uint32_t OcscPreOffA       = 0x49068; // 3 dwords: 0x49068..0x49070
constexpr uint32_t OcscPostOffA      = 0x49074; // 3 dwords: 0x49074..0x4907C
constexpr uint32_t PalPrecIndexA     = 0x4A400;
constexpr uint32_t PalPrecDataA      = 0x4A404;
constexpr uint32_t PalPrecAutoInc    = 0x00008000U;
constexpr uint32_t PalPrecCount      = 1024U;
constexpr uint32_t PalPrecGcMaxA     = 0x4A410; // 3 dwords: 0x4A410..0x4A418
constexpr uint32_t PalPrecExtGcMaxA  = 0x4A420; // 3 dwords: 0x4A420..0x4A428
constexpr uint32_t PalPrecExt2GcMaxA = 0x4A430; // 3 dwords: 0x4A430..0x4A438
constexpr uint32_t GammaModeA        = 0x4A480;
constexpr uint32_t GammaModePreEn    = 1U << 31;
constexpr uint32_t GammaModePostEn   = 1U << 30;
constexpr uint32_t GammaModeMultiSeg = 1U << 29;
constexpr uint32_t PreCscGamcIndexA  = 0x4A484;
constexpr uint32_t PreCscGamcDataA   = 0x4A488;
constexpr uint32_t PreCscGamcAutoInc = 0x00000400U;
constexpr uint32_t PreCscGamcCount   = 131U;

// Combo PHY A signal swing / pre-emphasis registers.
constexpr uint32_t PortClDw10A       = 0x162028;
constexpr uint32_t PortPcsDw1GrpA    = 0x162604;
constexpr uint32_t PortPcsDw1Ln0A    = 0x162804;
constexpr uint32_t PortTxDw5GrpA     = 0x162694;
constexpr uint32_t PortTxDw5Ln0A     = 0x162894;
constexpr uint32_t PortTxDw2LnA[4]   = {0x162888, 0x162988, 0x162A88, 0x162B88};
constexpr uint32_t PortTxDw4LnA[4]   = {0x162890, 0x162990, 0x162A90, 0x162B90};
constexpr uint32_t PortTxDw7LnA[4]   = {0x16289C, 0x16299C, 0x162A9C, 0x162B9C};

// Panel power sequencer & backlight (PCH).
constexpr uint32_t PpStatus          = 0xC7200;
constexpr uint32_t PpControl         = 0xC7204;
constexpr uint32_t PpOnDelays        = 0xC7208;
constexpr uint32_t PpOffDelays       = 0xC720C;
constexpr uint32_t PpDivisor         = 0xC7210;
constexpr uint32_t BlcPwmCtl1        = 0xC8250;
constexpr uint32_t BlcPwmFreq1       = 0xC8254;
constexpr uint32_t BlcPwmDuty1       = 0xC8258;

// Teardown status flags (bitmask).
enum TeardownFlag : uint32_t {
    TdPipeStopped   = 1U << 0,
    TdBufIdle       = 1U << 1,
    TdPanelOff      = 1U << 2,
    TdDpllUnlocked  = 1U << 3,
    TdDpllPwrOff    = 1U << 4,
    TdDbufOff       = 1U << 5,
    TdCdclkOff      = 1U << 6,
    TdAuxDdiOff     = 1U << 7,
    TdPw2Off        = 1U << 8,
    TdPw1Off        = 1U << 9,
    TdDc9Set        = 1U << 10,
    TdSinkD3        = 1U << 11,
};

// Restore status flags (bitmask).
enum RestoreFlag : uint32_t {
    RsDc9Cleared    = 1U << 0,
    RsPanelOn       = 1U << 1,
    RsDpllLocked    = 1U << 2,
    RsPhyRestored   = 1U << 3,
    RsSinkConfigOk  = 1U << 4,
    RsDdiActive     = 1U << 5,
    RsCrOk          = 1U << 6,
    RsEqOk          = 1U << 7,
    RsPostStatusOk  = 1U << 8,
    RsTransActive   = 1U << 9,
    RsCmtgPreserved = 1U << 10,
    RsMsaRestored   = 1U << 11,
    RsVerified      = 1U << 12,
    RsNativeActive  = 1U << 13,
    RsColorPreLut   = 1U << 14,
    RsColorPostLut  = 1U << 15,
    RsColorCsc      = 1U << 16,
    RsColorMisc     = 1U << 17,
};

// Safety fallback failure reason bitmask.
enum FailReason : uint32_t {
    FailNone         = 0,
    FailNoSnapshot   = 1U << 0,
    FailPanelPower   = 1U << 1,
    FailDpllLock     = 1U << 2,
    FailSinkConfig   = 1U << 3,
    FailDdiActive    = 1U << 4,
    FailClockRec     = 1U << 5,
    FailChannelEq    = 1U << 6,
    FailPostDpcd     = 1U << 7,
    FailTransActive  = 1U << 8,
    FailPanelVerify  = 1U << 9,
};

struct FirmwareHint {
    uint8_t linkBW {};
    uint8_t rateSet {};
    uint8_t bw {};
};

struct Snapshot {
    bool valid {};
    uint32_t dpllIndex {}; // 0=DPLL0, 1=DPLL1, 2=DPLL4
    uint32_t dpllEnable {}, dpllCfgcr0 {}, dpllCfgcr1 {};
    uint32_t dpclka {}, transClkSel {};
    uint32_t timing[12] {}; // HTOTAL..VSYNC, PIPESRC, VSYNCSHIFT, DATA_M1/N1, LINK_M1/N1
    uint32_t transDdiFuncCtl {}, transDdiFuncCtl2 {}, transMsaMisc {};
    uint32_t dpTpCtl {}, ddiBufCtl {}, transconf {}, pipeMisc {};
    uint32_t psCtrl {}, psWinPos {}, psWinSz {};
    uint32_t clDw10 {}, pcsDw1Grp {}, pcsDw1Ln0 {}, txDw5Ln0 {};
    uint32_t txDw2[4] {}, txDw4[4] {}, txDw7[4] {};
    uint32_t ppCtl {}, ppOnDelays {}, ppOffDelays {}, ppDivisor {};
    uint32_t blcCtl {}, blcFreq {}, blcDuty {};
    uint32_t pwrWellDriver {};
    uint8_t dpcdCaps[16] {};
    uint8_t dpcdCfg[16] {};
    uint8_t dpcdRateSet {};
    bool dpcdValid {};
    struct ColorSnapshot {
        uint32_t pipeBottomColor {};
        uint32_t planeColorCtl {};
        uint32_t gammaMode {};
        uint32_t cscMode {};
        uint32_t cscCoeff[6] {};
        uint32_t cscPreOff[3] {};
        uint32_t cscPostOff[3] {};
        uint32_t ocscCoeff[6] {};
        uint32_t ocscPreOff[3] {};
        uint32_t ocscPostOff[3] {};
        uint32_t palPrecGcMax[3] {};
        uint32_t palPrecExtGcMax[3] {};
        uint32_t palPrecExt2GcMax[3] {};
        bool valid {};
    } color {};
};

struct TeardownResult {
    bool complete {};
    uint32_t flags {};
    uint32_t wellsBiosAfter {UINT32_MAX};
    uint32_t wellsDriverAfter {UINT32_MAX};
    uint32_t dbufS0After {UINT32_MAX};
    uint32_t dcStateAfter {UINT32_MAX};
};

struct RestoreResult {
    bool verified {};
    uint32_t flags {};
    uint32_t failReasons {};
    uint32_t dpllAfter {UINT32_MAX};
    uint32_t ddiBufAfter {UINT32_MAX};
    uint32_t transConfAfter {UINT32_MAX};
    uint32_t transFunc2After {UINT32_MAX};
    uint32_t transMsaAfter {UINT32_MAX};
    uint32_t ppStatusAfter {UINT32_MAX};
    uint8_t lastLaneStatus[3] {}; // DPCD 0x202, 0x203, 0x204
};

struct ColorCache {
    uint32_t preGamma[PreCscGamcCount] {};
    uint32_t postGamma[PalPrecCount] {};
    uint32_t postExtMax[3] {};
    uint32_t postExt2Max[3] {};
    bool preGammaValid {};
    bool postGammaValid {};
    bool postExtMaxValid {};
    uint32_t preCacheCount {};
    uint32_t preZeroSkipCount {};
    uint32_t postCacheCount {};
    uint32_t postZeroSkipCount {};
    uint32_t preModesetRestoreCount {};
    uint32_t postModesetRestoreCount {};
    uint32_t preGammaPromoteCount {};
    uint32_t lastRestoreFlags {};
    uint32_t lastPipeMiscAfter {};
    uint32_t lastGammaModeAfter {};
    uint32_t lastCscModeAfter {};
};

struct State {
    Snapshot snapshot {};
    ColorCache color {};
    bool poweredDown {};
    bool colorRestoredOnWake {};
    bool preGammaPromotePending {};
    bool fallbackTripped {};
    uint32_t fallbackReason {};
    uint32_t teardownPpCtl {};
    uint32_t snapshotCount {};
    uint32_t teardownCount {};
    uint32_t teardownSkipCount {};
    uint32_t lastSkipReason {};
    uint32_t restoreCount {};
    uint32_t verifyPassCount {};
    uint32_t verifyFailCount {};
    TeardownResult lastTeardown {};
    RestoreResult lastRestore {};
};

// Cache Pipe A 131-entry Pre-CSC LUT (u3.16), rejecting all-zero tables.
inline bool cachePreGamma(State &st, const uint32_t words[PreCscGamcCount]) {
    if (!words) return false;
    bool nonZero = false;
    for (uint32_t i = 0; i < PreCscGamcCount; ++i) {
        if (words[i]) { nonZero = true; break; }
    }
    if (!nonZero) {
        ++st.color.preZeroSkipCount;
        return false;
    }
    for (uint32_t i = 0; i < PreCscGamcCount; ++i) st.color.preGamma[i] = words[i];
    st.color.preGammaValid = true;
    ++st.color.preCacheCount;
    return true;
}

// Cache Pipe A 1024-entry Post-CSC 10-bit LUT (+ optional u3.16 EXT/EXT2 GC_MAX),
// rejecting all-zero fade-to-black tables uploaded during pre-sleep fade.
inline bool cachePostGamma(State &st, const uint32_t words[PalPrecCount],
                           const uint32_t extMax[6] = nullptr, bool extMaxValid = false) {
    if (!words) return false;
    bool nonZero = false;
    for (uint32_t i = 0; i < PalPrecCount; ++i) {
        if (words[i]) { nonZero = true; break; }
    }
    if (!nonZero) {
        ++st.color.postZeroSkipCount;
        return false;
    }
    for (uint32_t i = 0; i < PalPrecCount; ++i) st.color.postGamma[i] = words[i];
    st.color.postGammaValid = true;
    ++st.color.postCacheCount;
    if (extMax && extMaxValid) {
        for (uint32_t i = 0; i < 3; ++i) {
            st.color.postExtMax[i]  = extMax[i];
            st.color.postExt2Max[i] = extMax[3U + i];
        }
        st.color.postExtMaxValid = true;
    }
    return true;
}

constexpr uint32_t TimingRegs[12] = {
    TransHtotalA, TransHblankA, TransHsyncA,
    TransVtotalA, TransVblankA, TransVsyncA,
    PipeSrcA, TransVsyncShiftA,
    PipeDataM1A, PipeDataN1A, PipeLinkM1A, PipeLinkN1A
};

template<class IO>
inline bool waitMask(IO &io, uint32_t reg, uint32_t mask, uint32_t want, uint32_t timeoutUs, uint32_t stepUs = 10) {
    for (uint32_t t = 0;; t += stepUs) {
        const uint32_t v = io.read(reg);
        if (v == UINT32_MAX) return false;
        if ((v & mask) == want) return true;
        if (t >= timeoutUs) return false;
        io.delayUs(stepUs);
    }
}

// Raw native DP AUX transfer on DDI A (DP_AUX_CH_CTL_A 0x64010, DATA 0x64014..0x64024),
// matching XE-EDPWAKE ewAux / Linux skl_get_aux_send_ctl / intel_dp_aux_xfer.
// Caller must ensure PW1 + AUX A power well are enabled.
template<class IO>
inline int auxXfer(IO &io, bool write, uint32_t addr, uint8_t *buf, uint32_t len) {
    if (!buf || len == 0 || len > 16) return -1;
    const uint32_t auxPw = io.read(PwrWellAuxDriver);
    if (auxPw == UINT32_MAX || !(auxPw & 1U)) return -1;
    for (int attempt = 0; attempt < 7; ++attempt) {
        if (!waitMask(io, AuxChCtlA, 1U << 31, 0, 10000, 10)) return -1;
        uint8_t msg[20] = {};
        uint32_t n = 4;
        msg[0] = static_cast<uint8_t>(((write ? 0x8U : 0x9U) << 4) | ((addr >> 16) & 0xFU));
        msg[1] = static_cast<uint8_t>((addr >> 8) & 0xFFU);
        msg[2] = static_cast<uint8_t>(addr & 0xFFU);
        msg[3] = static_cast<uint8_t>(len - 1U);
        if (write) {
            for (uint32_t i = 0; i < len; ++i) msg[4 + i] = buf[i];
            n += len;
        }
        for (uint32_t i = 0; i < n; i += 4) {
            const uint32_t word = (uint32_t(msg[i]) << 24) | (uint32_t(msg[i + 1]) << 16) |
                                  (uint32_t(msg[i + 2]) << 8) | uint32_t(msg[i + 3]);
            io.write(AuxChDataA + i, word);
        }
        // SEND_BUSY | DONE(W1C) | TIMEOUT(W1C) | 4000us timeout | RX_ERR(W1C) |
        // message length | FW_SYNC(24 -> 23) | SYNC(32 -> 31).
        const uint32_t ctl = (1U << 31) | (1U << 30) | (1U << 28) | (3U << 26) |
                             (1U << 25) | (n << 20) | (23U << 5) | 31U;
        io.write(AuxChCtlA, ctl);
        if (!waitMask(io, AuxChCtlA, 1U << 31, 0, 10000, 10)) return -1;
        const uint32_t st = io.read(AuxChCtlA);
        if (st == UINT32_MAX) return -1;
        io.write(AuxChCtlA, st | (1U << 30) | (1U << 28) | (1U << 25));
        if (st & ((1U << 28) | (1U << 25))) {
            io.delayUs(400);
            continue;
        }
        const uint32_t got = (st >> 20) & 0x1FU;
        uint8_t rep[20] = {};
        for (uint32_t i = 0; i < got && i < 20; i += 4) {
            const uint32_t d = io.read(AuxChDataA + i);
            rep[i]     = static_cast<uint8_t>(d >> 24);
            rep[i + 1] = static_cast<uint8_t>(d >> 16);
            rep[i + 2] = static_cast<uint8_t>(d >> 8);
            rep[i + 3] = static_cast<uint8_t>(d);
        }
        if (got < 1) {
            io.delayUs(400);
            continue;
        }
        const uint8_t reply = (rep[0] >> 4) & 0x3U;
        if (reply == 2) { // AUX DEFER
            io.delayUs(500);
            continue;
        }
        if (reply != 0) return -1; // NACK / error
        if (write) return static_cast<int>(len);
        const uint32_t dlen = (got - 1U) < len ? (got - 1U) : len;
        for (uint32_t i = 0; i < dlen; ++i) buf[i] = rep[1U + i];
        return static_cast<int>(dlen);
    }
    return -1;
}

template<class IO>
inline bool dpcdWriteByte(IO &io, uint32_t addr, uint8_t value) {
    return auxXfer(io, true, addr, &value, 1) == 1;
}

inline uint32_t resolvedLanes(const Snapshot &s) {
    uint32_t lanes = s.dpcdCfg[1] & 0x1FU;
    if (lanes == 1 || lanes == 2 || lanes == 4) return lanes;
    const uint32_t portWidth = ((s.transDdiFuncCtl >> 1) & 0x7U) + 1U;
    return (portWidth == 1 || portWidth == 2 || portWidth == 4) ? portWidth : 0U;
}

inline bool lanesClockRecovered(const uint8_t st[3], uint32_t lanes) {
    if (lanes == 1) return (st[0] & 0x01U) == 0x01U;
    if (lanes == 2) return (st[0] & 0x11U) == 0x11U;
    if (lanes == 4) return (st[0] & 0x11U) == 0x11U && (st[1] & 0x11U) == 0x11U;
    return false;
}

inline bool lanesChannelEqualized(const uint8_t st[3], uint32_t lanes) {
    if (!(st[2] & 0x01U)) return false; // INTERLANE_ALIGN_DONE
    if (lanes == 1) return (st[0] & 0x07U) == 0x07U;
    if (lanes == 2) return (st[0] & 0x77U) == 0x77U;
    if (lanes == 4) return (st[0] & 0x77U) == 0x77U && (st[1] & 0x77U) == 0x77U;
    return false;
}

// Capture a complete eDP Pipe A / DDI A snapshot while the panel is live.
template<class IO>
inline bool captureSnapshot(IO &io, Snapshot &s, const FirmwareHint &hint = {}) {
    const uint32_t wells = io.read(PwrWellCtlDriver);
    if (wells == UINT32_MAX || (wells & 0x401U) != 0x401U) return false; // PW1 + PipeA must be on
    const uint32_t conf = io.read(TransConfA);
    const uint32_t buf  = io.read(DdiBufCtlA);
    const uint32_t func = io.read(TransDdiFuncCtlA);
    if (conf == UINT32_MAX || buf == UINT32_MAX || func == UINT32_MAX) return false;
    // Require running Transcoder A + enabled DDI A driving DP SST on DDI A.
    if ((conf & 0xC0000000U) != 0xC0000000U || !(buf & DdiBufEnable)) return false;
    if (!(func & 0x80000000U) || ((func >> 27) & 0xFU) != 1U || ((func >> 24) & 0x7U) != 2U) return false;

    const uint32_t dpclka = io.read(DpcaCfgcr0);
    if (dpclka == UINT32_MAX) return false;
    const uint32_t dpllIdx = dpclka & 0x3U;
    if (dpllIdx > 2U) return false;

    Snapshot next {};
    next.pwrWellDriver   = wells;
    next.dpllIndex       = dpllIdx;
    next.dpllEnable      = io.read(dpllEnableReg(dpllIdx));
    next.dpllCfgcr0      = io.read(dpllCfgcr0Reg(dpllIdx));
    next.dpllCfgcr1      = io.read(dpllCfgcr1Reg(dpllIdx));
    next.dpclka          = dpclka;
    next.transClkSel     = io.read(TransClkSelA);
    for (uint32_t i = 0; i < 12; ++i) next.timing[i] = io.read(TimingRegs[i]);
    next.transDdiFuncCtl  = func;
    next.transDdiFuncCtl2 = io.read(TransDdiFuncCtl2A);
    next.transMsaMisc     = io.read(TransMsaMiscA);
    next.dpTpCtl          = io.read(DpTpCtlA);
    next.ddiBufCtl        = buf;
    next.transconf        = conf;
    next.pipeMisc         = io.read(PipeMiscA);
    next.color.pipeBottomColor = io.read(PipeBottomColorA);
    next.color.planeColorCtl   = io.read(PlaneColorCtl1A);
    next.color.gammaMode       = io.read(GammaModeA);
    next.color.cscMode         = io.read(CscModeA);
    for (uint32_t i = 0; i < 6; ++i) {
        next.color.cscCoeff[i]  = io.read(CscCoeffA + 4U * i);
        next.color.ocscCoeff[i] = io.read(OcscCoeffA + 4U * i);
    }
    for (uint32_t i = 0; i < 3; ++i) {
        next.color.cscPreOff[i]        = io.read(CscPreOffA + 4U * i);
        next.color.cscPostOff[i]       = io.read(CscPostOffA + 4U * i);
        next.color.ocscPreOff[i]       = io.read(OcscPreOffA + 4U * i);
        next.color.ocscPostOff[i]      = io.read(OcscPostOffA + 4U * i);
        next.color.palPrecGcMax[i]     = io.read(PalPrecGcMaxA + 4U * i);
        next.color.palPrecExtGcMax[i]  = io.read(PalPrecExtGcMaxA + 4U * i);
        next.color.palPrecExt2GcMax[i] = io.read(PalPrecExt2GcMaxA + 4U * i);
    }
    next.color.valid = (next.pipeMisc != UINT32_MAX && next.color.gammaMode != UINT32_MAX &&
                        next.color.cscMode != UINT32_MAX && next.color.pipeBottomColor != UINT32_MAX);
    next.psCtrl           = io.read(PsCtrl1A);
    next.psWinPos         = io.read(PsWinPos1A);
    next.psWinSz          = io.read(PsWinSz1A);
    next.clDw10           = io.read(PortClDw10A);
    next.pcsDw1Grp        = io.read(PortPcsDw1GrpA);
    next.pcsDw1Ln0        = io.read(PortPcsDw1Ln0A);
    next.txDw5Ln0         = io.read(PortTxDw5Ln0A);
    for (uint32_t i = 0; i < 4; ++i) {
        next.txDw2[i] = io.read(PortTxDw2LnA[i]);
        next.txDw4[i] = io.read(PortTxDw4LnA[i]);
        next.txDw7[i] = io.read(PortTxDw7LnA[i]);
    }
    next.ppCtl       = io.read(PpControl);
    next.ppOnDelays  = io.read(PpOnDelays);
    next.ppOffDelays = io.read(PpOffDelays);
    next.ppDivisor   = io.read(PpDivisor);
    next.blcCtl      = io.read(BlcPwmCtl1);
    next.blcFreq     = io.read(BlcPwmFreq1);
    next.blcDuty     = io.read(BlcPwmDuty1);

    if (next.dpllEnable == UINT32_MAX || !(next.dpllEnable & (1U << 30)) ||
        next.transDdiFuncCtl2 == UINT32_MAX || next.transMsaMisc == UINT32_MAX ||
        next.ppCtl == UINT32_MAX) {
        return false;
    }

    const int gotCaps = auxXfer(io, false, 0x000, next.dpcdCaps, 16);
    const int gotCfg  = auxXfer(io, false, 0x100, next.dpcdCfg, 16);
    uint8_t rateSet   = 0;
    const int gotRate = auxXfer(io, false, 0x115, &rateSet, 1);
    next.dpcdRateSet  = gotRate == 1 ? rateSet : hint.rateSet;
    next.dpcdValid    = (gotCaps == 16 && gotCfg == 16);

    if (next.dpcdValid && !next.dpcdCfg[0] && !next.dpcdRateSet) {
        if (hint.bw) next.dpcdCfg[0] = hint.bw;
        else if (hint.linkBW) next.dpcdCfg[0] = hint.linkBW;
        else next.dpcdRateSet = hint.rateSet;
    }

    if (!next.dpcdValid || resolvedLanes(next) == 0) return false;
    next.valid = true;
    s = next;
    return true;
}

// Phase 1: Deep Idle sleep teardown in XE-EDPWAKE / Linux icl_display_core_uninit order.
// Must NOT call IOSleep (Apple clientNotify holds locks during enableDC9).
template<class IO>
inline TeardownResult teardown(IO &io, State &st) {
    TeardownResult r {};
    if (!st.snapshot.valid) return r;

    const uint32_t wellsBefore = io.read(PwrWellCtlDriver);
    const bool pipeAPowered = (wellsBefore != UINT32_MAX) && ((wellsBefore & 0x401U) == 0x401U);

    // 1. Planes, cursor, transcoder A, TRANS_DDI_FUNC_CTL off (idempotent if donor already stopped them).
    //    Matching XE-EDPWAKE (0003-deep-idle-sleep.diff:293-300), do not touch TRANS_DDI_FUNC_CTL2_A.
    bool stopped = true;
    if (pipeAPowered) {
        const uint32_t planeCtl = io.read(PlaneCtl1A);
        if (planeCtl != UINT32_MAX && (planeCtl & 0x80000000U)) {
            io.write(PlaneCtl1A, planeCtl & ~0x80000000U);
            const uint32_t surf = io.read(PlaneSurf1A);
            if (surf != UINT32_MAX) io.write(PlaneSurf1A, surf);
        }
        const uint32_t curCtl = io.read(CurCtlA);
        if (curCtl != UINT32_MAX && (curCtl & 0x3FU)) {
            io.write(CurCtlA, 0U);
            const uint32_t curBase = io.read(CurBaseA);
            if (curBase != UINT32_MAX) io.write(CurBaseA, curBase);
        }
        const uint32_t conf = io.read(TransConfA);
        if (conf != UINT32_MAX && (conf & 0x80000000U)) {
            io.write(TransConfA, conf & ~0x80000000U);
            stopped = waitMask(io, TransConfA, 1U << 30, 0, 60000, 50);
        }
        const uint32_t func = io.read(TransDdiFuncCtlA);
        if (func != UINT32_MAX && (func & 0x80000000U)) {
            io.write(TransDdiFuncCtlA, func & ~0x80000000U);
        }
    }
    if (stopped) r.flags |= TdPipeStopped;

    // 2. Link off: DPCD SET_POWER=D3 (if link active), DDI_BUF_CTL_A & DP_TP_CTL_A off, wait idle.
    bool bufIdle = true;
    if (wellsBefore != UINT32_MAX && (wellsBefore & 1U)) {
        const uint32_t buf = io.read(DdiBufCtlA);
        if (buf != UINT32_MAX && (buf & DdiBufEnable)) {
            if (dpcdWriteByte(io, 0x600, 0x02)) r.flags |= TdSinkD3;
            io.write(DdiBufCtlA, buf & ~DdiBufEnable);
        }
        if (pipeAPowered) {
            const uint32_t tp = io.read(DpTpCtlA);
            if (tp != UINT32_MAX && (tp & 0x80000000U)) {
                io.write(DpTpCtlA, tp & ~0x80000000U);
            }
        }
        bufIdle = waitMask(io, DdiBufCtlA, DdiBufIdle, DdiBufIdle, 2000, 10);
    }
    if (bufIdle) r.flags |= TdBufIdle;

    // 3. Panel power off (eDP PPS), if donor left it on.
    st.teardownPpCtl = 0;
    const uint32_t ppCtl = io.read(PpControl);
    bool panelOff = true;
    if (ppCtl != UINT32_MAX && (ppCtl & 0x1U)) {
        st.teardownPpCtl = ppCtl;
        io.write(PpControl, ppCtl & ~0x5U); // clear power target & backlight enable
        panelOff = waitMask(io, PpStatus, 0x80000000U, 0, 600000, 1000);
    } else {
        const uint32_t ppSt = io.read(PpStatus);
        panelOff = (ppSt != UINT32_MAX) && !(ppSt & 0x80000000U);
    }
    if (panelOff) r.flags |= TdPanelOff;

    // 4. All active combo DPLLs (DPLL0, DPLL1, and snapshot DPLL if DPLL4) off + DDI A clock gated.
    bool dpllUnlocked = true, dpllPwrOff = true;
    if (wellsBefore != UINT32_MAX && (wellsBefore & 1U)) {
        const uint32_t dpllRegs[3] = {dpllEnableReg(0), dpllEnableReg(1), dpllEnableReg(st.snapshot.dpllIndex)};
        for (uint32_t i = 0; i < 3; ++i) {
            if (i == 2 && st.snapshot.dpllIndex < 2) continue;
            const uint32_t dpllReg = dpllRegs[i];
            const uint32_t dpll = io.read(dpllReg);
            if (dpll == UINT32_MAX) continue;
            if (dpll & (1U << 31)) {
                io.write(dpllReg, dpll & ~(1U << 31));
                if (!waitMask(io, dpllReg, 1U << 30, 0, 5000, 10)) dpllUnlocked = false;
            }
            const uint32_t dpll2 = io.read(dpllReg);
            if (dpll2 != UINT32_MAX && (dpll2 & (1U << 27))) {
                io.write(dpllReg, dpll2 & ~(1U << 27));
                if (!waitMask(io, dpllReg, 1U << 26, 0, 5000, 10)) dpllPwrOff = false;
            }
        }
        const uint32_t dpclka = io.read(DpcaCfgcr0);
        if (dpclka != UINT32_MAX && !(dpclka & DdiAClockOff)) {
            io.write(DpcaCfgcr0, dpclka | DdiAClockOff);
        }
    }
    if (dpllUnlocked) r.flags |= TdDpllUnlocked;
    if (dpllPwrOff)   r.flags |= TdDpllPwrOff;

    // 5. All 4 Display13 DBUF slices S3..S0 off (0x44304, 0x44300, 0x44FE8, 0x45008).
    bool dbufOff = true;
    for (int i = 3; i >= 0; --i) {
        const uint32_t reg = DbufSliceRegs[i];
        const uint32_t v = io.read(reg);
        if (v == UINT32_MAX) { dbufOff = false; continue; }
        if (v & DbufPowerRequest) {
            io.write(reg, v & ~DbufPowerRequest);
            (void)io.read(reg);
            io.delayUs(10);
        }
        if (!waitMask(io, reg, DbufPowerState, 0, 2000, 10)) dbufOff = false;
    }
    if (dbufOff) r.flags |= TdDbufOff;

    // 6. CDCLK PLL off (if not already stopped by Display13Clock::stop).
    bool cdOff = true;
    const uint32_t cdPll = io.read(CdclkPllEnable);
    if (cdPll != UINT32_MAX && (cdPll & (1U << 31))) {
        io.write(CdclkPllEnable, cdPll & ~(1U << 31));
        cdOff = waitMask(io, CdclkPllEnable, 1U << 30, 0, 2000, 10);
    } else if (cdPll == UINT32_MAX || (cdPll & (1U << 30))) {
        cdOff = false;
    }
    if (cdOff) r.flags |= TdCdclkOff;

    // 7. Power wells off from BOTH BIOS and Driver requesters:
    //    AUX -> DDI IO -> PW2 & Pipe wells -> PW1 LAST.
    auto clearReqs = [&](uint32_t reg, uint32_t clearMask) {
        const uint32_t v = io.read(reg);
        if (v != UINT32_MAX && (v & clearMask)) {
            io.write(reg, v & ~clearMask);
            (void)io.read(reg);
        }
    };
    clearReqs(PwrWellAuxDriver, RequestBitsMask);
    clearReqs(PwrWellAuxBios,   RequestBitsMask);
    const bool auxOff = waitMask(io, PwrWellAuxDriver, StateBitsMask, 0, 5000, 10);

    clearReqs(PwrWellDdiDriver, RequestBitsMask);
    clearReqs(PwrWellDdiBios,   RequestBitsMask);
    const bool ddiOff = waitMask(io, PwrWellDdiDriver, StateBitsMask, 0, 5000, 10);
    if (auxOff && ddiOff) r.flags |= TdAuxDdiOff;

    constexpr uint32_t AbovePw1Req   = RequestBitsMask & ~0x2U;
    constexpr uint32_t AbovePw1State = StateBitsMask & ~0x1U;
    clearReqs(PwrWellCtlDriver, AbovePw1Req);
    clearReqs(PwrWellCtlBios,   AbovePw1Req);
    const bool pw2Off = waitMask(io, PwrWellCtlDriver, AbovePw1State, 0, 5000, 10);
    if (pw2Off) r.flags |= TdPw2Off;

    clearReqs(PwrWellCtlDriver, 0x2U);
    clearReqs(PwrWellCtlBios,   0x2U);
    const bool pw1Off = waitMask(io, PwrWellCtlDriver, 0x1U, 0, 5000, 10);
    if (pw1Off) r.flags |= TdPw1Off;

    // 8. Enter DC9 (clear DC5/DC6 bits, set bit 3).
    //    SOUTH_CHICKEN1 bit 7 (Wa_14010685332) is held=1 through sleep by
    //    S0ixStandby -s0ixrefclk once CTL1==0 && CTL2==0 && (DC_STATE_EN & 8).
    const uint32_t dcBefore = io.read(DcStateEn);
    if (dcBefore != UINT32_MAX) {
        io.write(DcStateEn, (dcBefore & ~Dc56Mask) | Dc9Enable);
        r.dcStateAfter = io.read(DcStateEn);
        if (r.dcStateAfter != UINT32_MAX && (r.dcStateAfter & Dc9Enable)) r.flags |= TdDc9Set;
    }

    r.wellsBiosAfter   = io.read(PwrWellCtlBios);
    r.wellsDriverAfter = io.read(PwrWellCtlDriver);
    r.dbufS0After      = io.read(DbufSliceRegs[0]);
    r.complete = pw2Off && pw1Off && dbufOff && (r.flags & TdDc9Set);

    st.poweredDown = true;
    st.colorRestoredOnWake = false;
    st.preGammaPromotePending = true;
    st.color.lastRestoreFlags = 0;
    ++st.teardownCount;
    st.lastTeardown = r;
    return r;
}

// Phase 2a: Exit DC9 before turning PW1 back on (Linux bxt_disable_dc9).
// SOUTH_CHICKEN1 bit 7 was already cleared by S0ixStandby::refclkWake().
template<class IO>
inline bool exitDC9(IO &io) {
    const uint32_t dc = io.read(DcStateEn);
    if (dc == UINT32_MAX) return false;
    if (dc & Dc9Enable) {
        io.write(DcStateEn, dc & ~Dc9Enable);
        (void)io.read(DcStateEn);
    }
    const uint32_t after = io.read(DcStateEn);
    return after != UINT32_MAX && !(after & Dc9Enable);
}

// Restore PCH Panel Power Sequencer delays defensively if DC9 zeroed them,
// before Apple's prepareToEnterWake / hwSetPanelPower(1) runs.
template<class IO>
inline void restorePpsDelaysIfZero(IO &io, const Snapshot &s) {
    if (!s.valid) return;
    if (s.ppOnDelays && io.read(PpOnDelays) == 0)   io.write(PpOnDelays, s.ppOnDelays);
    if (s.ppOffDelays && io.read(PpOffDelays) == 0) io.write(PpOffDelays, s.ppOffDelays);
    if (s.ppDivisor && io.read(PpDivisor) == 0)     io.write(PpDivisor, s.ppDivisor);
}

// Linux i915 intel_color.c wake restore (glk_load_degamma_lut -> bdw_load_lut_10 +
// icl_load_gcmax -> ilk_load_csc_matrix -> icl_color_commit_arm).
// Restores Pipe A LUT SRAMs, CSC matrices, GAMMA_MODE, PIPE_MISC, and PIPE_BOTTOM_COLOR
// lost when DC9 powers off PW_A. Runs both:
//   - preModeset=true: in restoreDisplay13ResourcesLocked right after PW_1/PW_A + Core
//     restore, BEFORE Apple's prepareToExitSleep -> hwUpdateRegCache() caches wiped regs.
//   - preModeset=false: inside restoreEDP() before TRANSCONF_A enable, or in
//     verifyOrRestoreEDP() if PW_A was not yet powered during Phase 2a.
template<class IO>
inline uint32_t restoreColorState(IO &io, State &st, bool preModeset) {
    const Snapshot &s = st.snapshot;
    if (!s.valid || !s.color.valid) return 0U;
    const uint32_t wells = io.read(PwrWellCtlDriver);
    if (wells == UINT32_MAX || (wells & 0x401U) != 0x401U) return 0U;

    uint32_t flags = 0U;

    // 1. Pre-CSC 131-entry LUT SRAM (glk_load_degamma_lut).
    if (st.color.preGammaValid) {
        io.write(PreCscGamcIndexA, PreCscGamcAutoInc);
        for (uint32_t i = 0; i < PreCscGamcCount; ++i) {
            io.write(PreCscGamcDataA, st.color.preGamma[i]);
        }
        io.write(PreCscGamcIndexA, 0U);
        flags |= RsColorPreLut;
    }

    // 2. Post-CSC 1024-entry 10-bit LUT SRAM + GC_MAX/EXT/EXT2 (bdw_load_lut_10 + icl_load_gcmax).
    if (st.color.postGammaValid) {
        io.write(PalPrecIndexA, PalPrecAutoInc);
        for (uint32_t i = 0; i < PalPrecCount; ++i) {
            io.write(PalPrecDataA, st.color.postGamma[i]);
        }
        io.write(PalPrecIndexA, 0U);

        const bool snapGcNonZero = (s.color.palPrecGcMax[0] | s.color.palPrecGcMax[1] | s.color.palPrecGcMax[2]) != 0U;
        const bool snapExtNonZero = (s.color.palPrecExtGcMax[0] | s.color.palPrecExtGcMax[1] | s.color.palPrecExtGcMax[2]) != 0U;
        const bool snapExt2NonZero = (s.color.palPrecExt2GcMax[0] | s.color.palPrecExt2GcMax[1] | s.color.palPrecExt2GcMax[2]) != 0U;
        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t gc = snapGcNonZero ? s.color.palPrecGcMax[i] : 0x00010000U;
            const uint32_t ext = st.color.postExtMaxValid ? st.color.postExtMax[i]
                                 : (snapExtNonZero ? s.color.palPrecExtGcMax[i] : 0x00010000U);
            const uint32_t ext2 = st.color.postExtMaxValid ? st.color.postExt2Max[i]
                                  : (snapExt2NonZero ? s.color.palPrecExt2GcMax[i] : 0x00010000U);
            io.write(PalPrecGcMaxA + 4U * i, gc);
            io.write(PalPrecExtGcMaxA + 4U * i, ext);
            io.write(PalPrecExt2GcMaxA + 4U * i, ext2);
        }
        flags |= RsColorPostLut;
    }

    // 3. Pipe CSC & Output CSC matrices + CSC_MODE (ilk_load_csc_matrix).
    for (uint32_t i = 0; i < 6; ++i) {
        io.write(CscCoeffA + 4U * i, s.color.cscCoeff[i]);
        io.write(OcscCoeffA + 4U * i, s.color.ocscCoeff[i]);
    }
    for (uint32_t i = 0; i < 3; ++i) {
        io.write(CscPreOffA + 4U * i, s.color.cscPreOff[i]);
        io.write(CscPostOffA + 4U * i, s.color.cscPostOff[i]);
        io.write(OcscPreOffA + 4U * i, s.color.ocscPreOff[i]);
        io.write(OcscPostOffA + 4U * i, s.color.ocscPostOff[i]);
    }
    io.write(CscModeA, s.color.cscMode);
    flags |= RsColorCsc;

    // 4. Arm GAMMA_MODE, PIPE_MISC (bpc + dither), and PIPE_BOTTOM_COLOR (icl_color_commit_arm).
    // Never enable Pre-CSC or Post-CSC gamma onto uninitialized SRAM if its LUT was not cached.
    uint32_t gammaMode = s.color.gammaMode;
    // If captureSnapshot() ran after WindowServer fade-to-black cleared PRE/POST enable bits
    // (0x20000001 instead of 0xE0000001), re-enable the active cached LUTs cleanly.
    if (st.color.preGammaValid)  gammaMode |= GammaModePreEn;
    else                         gammaMode &= ~GammaModePreEn;
    if (st.color.postGammaValid) gammaMode |= GammaModePostEn;
    else                         gammaMode &= ~(GammaModePostEn | GammaModeMultiSeg);
    io.write(GammaModeA, gammaMode);
    io.write(PipeMiscA, s.pipeMisc);
    io.write(PipeBottomColorA, s.color.pipeBottomColor);
    flags |= RsColorMisc;

    st.colorRestoredOnWake = true;
    st.color.lastRestoreFlags = flags;
    if (preModeset) ++st.color.preModesetRestoreCount;
    else            ++st.color.postModesetRestoreCount;
    st.color.lastPipeMiscAfter  = io.read(PipeMiscA);
    st.color.lastGammaModeAfter = io.read(GammaModeA);
    st.color.lastCscModeAfter   = io.read(CscModeA);
    return flags;
}

// Phase 2b (replay): Full Display13 eDP modeset & AUX link-training restore,
// matching keenerdavid18 XE-EDPWAKE xeEdpWakeReplay() (0003-deep-idle-sleep.diff:336-432).
// Preconditions:
//   - Runs AFTER prepareToExitSleep and prepareToEnterWake/hwSetMode (never before!)
//   - DC9 exited; PW1, PW2, PipeA, DDIA, AUXA powered on with fuses ready
//   - Combo PHY A/B, Core registers, MBUS_CTL, CDCLK, DBUF slices & allocation restored
template<class IO>
inline RestoreResult restoreEDP(IO &io, State &st) {
    RestoreResult r {};
    const Snapshot &s = st.snapshot;
    if (!s.valid) {
        r.failReasons |= FailNoSnapshot;
        st.fallbackTripped = true;
        st.fallbackReason = r.failReasons;
        ++st.verifyFailCount;
        st.lastRestore = r;
        return r;
    }
    ++st.restoreCount;

    const uint32_t dc = io.read(DcStateEn);
    if (dc != UINT32_MAX && !(dc & Dc9Enable)) r.flags |= RsDc9Cleared;

    // 1. Panel Power Sequencer: restore delays if DC9 cleared them, power panel on.
    restorePpsDelaysIfZero(io, s);

    const uint32_t ppStatusBefore = io.read(PpStatus);
    bool panelOn = (ppStatusBefore != UINT32_MAX) && ((ppStatusBefore & 0x80000000U) != 0);
    if (!panelOn) {
        // WA 22019252566: prevent PCH DPLS clock gating during panel-on.
        const uint32_t dplsBefore = io.read(SouthDplsGateReg);
        const bool dplsChanged = (dplsBefore != UINT32_MAX) && !(dplsBefore & SouthDplsGateBit);
        if (dplsChanged) io.write(SouthDplsGateReg, dplsBefore | SouthDplsGateBit);

        const uint32_t targetPp = (st.teardownPpCtl ? st.teardownPpCtl : s.ppCtl);
        // Enable panel power target (bit 0); hold backlight enable (bit 2) until link trained.
        io.write(PpControl, (targetPp | 0x1U) & ~0x4U);
        for (uint32_t ms = 0; ms <= 800; ms += 10) {
            const uint32_t stPp = io.read(PpStatus);
            if (stPp != UINT32_MAX && (stPp & 0xB0000000U) == 0x80000000U) {
                panelOn = true;
                break;
            }
            if (ms < 800) io.sleepMs(10);
        }
        if (dplsChanged) {
            const uint32_t dplsNow = io.read(SouthDplsGateReg);
            if (dplsNow != UINT32_MAX) io.write(SouthDplsGateReg, dplsNow & ~SouthDplsGateBit);
        }
    }
    if (panelOn) r.flags |= RsPanelOn;
    else r.failReasons |= FailPanelPower;

    // 2. DPLL restore (DPLL0 / DPLL1 / DPLL4 per snapshot).
    const uint32_t dpllReg  = dpllEnableReg(s.dpllIndex);
    const uint32_t cfg0Reg  = dpllCfgcr0Reg(s.dpllIndex);
    const uint32_t cfg1Reg  = dpllCfgcr1Reg(s.dpllIndex);
    bool dpllLocked = (io.read(dpllReg) & (1U << 30)) != 0;
    if (!dpllLocked) {
        const uint32_t cur = io.read(dpllReg);
        if (cur != UINT32_MAX) {
            io.write(dpllReg, cur | (1U << 27));
            (void)waitMask(io, dpllReg, 1U << 26, 1U << 26, 5000, 10);
            io.write(cfg0Reg, s.dpllCfgcr0);
            io.write(cfg1Reg, s.dpllCfgcr1);
            (void)io.read(cfg1Reg);
            io.write(dpllReg, io.read(dpllReg) | (1U << 31));
            dpllLocked = waitMask(io, dpllReg, 1U << 30, 1U << 30, 5000, 10);
        }
    }
    r.dpllAfter = io.read(dpllReg);
    if (dpllLocked) r.flags |= RsDpllLocked;
    else r.failReasons |= FailDpllLock;

    if (!dpllLocked) {
        r.ppStatusAfter = io.read(PpStatus);
        st.fallbackTripped = true;
        st.fallbackReason = r.failReasons;
        ++st.verifyFailCount;
        st.poweredDown = false;
        st.teardownPpCtl = 0;
        st.lastRestore = r;
        return r;
    }

    // 3. DDI A clock & Transcoder A clock select.
    io.write(DpcaCfgcr0, s.dpclka & ~DdiAClockOff);
    (void)io.read(DpcaCfgcr0);
    io.write(TransClkSelA, s.transClkSel);
    (void)io.read(TransClkSelA);

    // 4. Timings, M/N, PIPE_MISC, TRANS_DDI_FUNC_CTL2 (preserving X1C10 CMTG bit 3),
    //    TRANS_DDI_FUNC_CTL (disabled), and TRANS_MSA_MISC.
    for (uint32_t i = 0; i < 12; ++i) io.write(TimingRegs[i], s.timing[i]);
    io.write(PipeMiscA, s.pipeMisc);
    io.write(TransDdiFuncCtl2A, s.transDdiFuncCtl2);
    r.transFunc2After = io.read(TransDdiFuncCtl2A);
    if ((s.transDdiFuncCtl2 & TransDdiCtl2Cmtg) == (r.transFunc2After & TransDdiCtl2Cmtg)) {
        r.flags |= RsCmtgPreserved;
    }
    io.write(TransDdiFuncCtlA, s.transDdiFuncCtl & ~0x80000000U);
    io.write(TransMsaMiscA, s.transMsaMisc);
    r.transMsaAfter = io.read(TransMsaMiscA);
    if (r.transMsaAfter == s.transMsaMisc) r.flags |= RsMsaRestored;

    // 5. Combo PHY A lane signal levels (restore if DC9 reset them).
    bool phyLost = (io.read(PortTxDw5Ln0A) != s.txDw5Ln0) || (io.read(PortClDw10A) != s.clDw10);
    for (uint32_t i = 0; i < 4 && !phyLost; ++i) {
        if (io.read(PortTxDw2LnA[i]) != s.txDw2[i] ||
            io.read(PortTxDw4LnA[i]) != s.txDw4[i] ||
            io.read(PortTxDw7LnA[i]) != s.txDw7[i]) {
            phyLost = true;
        }
    }
    if (phyLost) {
        const uint32_t dw5Now = io.read(PortTxDw5Ln0A);
        if (dw5Now != UINT32_MAX) io.write(PortTxDw5GrpA, dw5Now & ~0x80000000U);
        for (uint32_t i = 0; i < 4; ++i) {
            io.write(PortTxDw2LnA[i], s.txDw2[i]);
            io.write(PortTxDw4LnA[i], s.txDw4[i]);
            io.write(PortTxDw7LnA[i], s.txDw7[i]);
        }
        io.write(PortClDw10A, s.clDw10);
        io.write(PortTxDw5GrpA, s.txDw5Ln0);
        r.flags |= RsPhyRestored;
    }

    // 6. Sink configuration over AUX A (D0, firmware link rate & lanes, downspread, coding).
    const uint32_t lanes = resolvedLanes(s);
    bool sinkOk = (lanes >= 1 && lanes <= 4) && dpcdWriteByte(io, 0x600, 0x01);
    if (sinkOk) {
        io.sleepMs(1);
        if (s.dpcdCfg[0]) sinkOk = dpcdWriteByte(io, 0x100, s.dpcdCfg[0]);
        else              sinkOk = dpcdWriteByte(io, 0x115, s.dpcdRateSet);
        if (sinkOk) sinkOk = dpcdWriteByte(io, 0x101, s.dpcdCfg[1]);
        if (sinkOk) sinkOk = dpcdWriteByte(io, 0x107, s.dpcdCfg[7]);
        if (sinkOk) sinkOk = dpcdWriteByte(io, 0x108, s.dpcdCfg[8] ? s.dpcdCfg[8] : 0x01U);
    }
    if (sinkOk) r.flags |= RsSinkConfigOk;
    else r.failReasons |= FailSinkConfig;

    // 7. Source DP_TP_CTL (TPS1) + DDI_BUF_CTL_A enable, wait non-idle.
    const uint32_t tpBase = (1U << 31) | (s.dpTpCtl & (1U << 18)); // enable + saved enhanced framing
    io.write(DpTpCtlA, tpBase);
    io.write(DdiBufCtlA, s.ddiBufCtl | DdiBufEnable);
    io.delayUs(600);
    const bool ddiActive = waitMask(io, DdiBufCtlA, DdiBufIdle, 0, 2000, 10);
    r.ddiBufAfter = io.read(DdiBufCtlA);
    if (ddiActive && (r.ddiBufAfter & DdiBufEnable)) r.flags |= RsDdiActive;
    else r.failReasons |= FailDdiActive;

    // 8. AUX Link Training (CR TPS1 -> EQ TPS3/TPS2) at firmware rate & lanes.
    bool crOk = false, eqOk = false;
    if (sinkOk && ddiActive) {
        const uint8_t lvl[4] = {s.dpcdCfg[3], s.dpcdCfg[4], s.dpcdCfg[5], s.dpcdCfg[6]};
        // Clock Recovery (TPS1, scrambling disabled).
        io.write(DpTpCtlA, tpBase | (0U << 8));
        uint8_t tp1[5] = {0x21U, lvl[0], lvl[1], lvl[2], lvl[3]};
        if (auxXfer(io, true, 0x102, tp1, 1U + lanes) == static_cast<int>(1U + lanes)) {
            uint8_t stBytes[6] = {};
            for (int tries = 0; tries < 6 && !crOk; ++tries) {
                io.delayUs(500);
                if (auxXfer(io, false, 0x202, stBytes, 6) == 6) {
                    r.lastLaneStatus[0] = stBytes[0];
                    r.lastLaneStatus[1] = stBytes[1];
                    r.lastLaneStatus[2] = stBytes[2];
                    crOk = lanesClockRecovered(stBytes, lanes);
                }
            }
        }
        if (crOk) r.flags |= RsCrOk;
        else r.failReasons |= FailClockRec;

        // Channel Equalization (TPS3 when sink caps[2] bit 6 set, else TPS2).
        if (crOk) {
            const bool tps3 = ((s.dpcdCaps[2] >> 6) & 1U) != 0;
            io.write(DpTpCtlA, tpBase | ((tps3 ? 4U : 1U) << 8));
            uint8_t tp2[5] = {static_cast<uint8_t>(0x20U | (tps3 ? 3U : 2U)), lvl[0], lvl[1], lvl[2], lvl[3]};
            if (auxXfer(io, true, 0x102, tp2, 1U + lanes) == static_cast<int>(1U + lanes)) {
                uint32_t eqUs = (s.dpcdCaps[14] & 0x7FU) ? uint32_t(s.dpcdCaps[14] & 0x7FU) * 4000U : 400U;
                if (eqUs > 65000U) eqUs = 65000U;
                uint8_t stBytes[6] = {};
                for (int tries = 0; tries < 6 && !eqOk; ++tries) {
                    io.delayUs(eqUs);
                    if (auxXfer(io, false, 0x202, stBytes, 6) == 6) {
                        r.lastLaneStatus[0] = stBytes[0];
                        r.lastLaneStatus[1] = stBytes[1];
                        r.lastLaneStatus[2] = stBytes[2];
                        eqOk = lanesChannelEqualized(stBytes, lanes);
                    }
                }
            }
            if (eqOk) r.flags |= RsEqOk;
            else r.failReasons |= FailChannelEq;
        }
    }

    // 9. Transition link to normal video & end DPCD training pattern.
    io.write(DpTpCtlA, tpBase | (2U << 8)); // Idle pattern
    io.delayUs(100);
    io.write(DpTpCtlA, tpBase | (3U << 8)); // Normal video
    (void)dpcdWriteByte(io, 0x102, 0x00);

    // 9b. Restore Pipe A LUT SRAMs, CSC, GAMMA_MODE, PIPE_MISC, PIPE_BOTTOM_COLOR
    //     before enabling Transcoder A (Linux i915 intel_color_load_luts + commit_arm order).
    r.flags |= restoreColorState(io, st, false);
    if (s.color.valid && s.color.planeColorCtl && s.color.planeColorCtl != UINT32_MAX &&
        io.read(PlaneColorCtl1A) == 0U) {
        io.write(PlaneColorCtl1A, s.color.planeColorCtl);
    }

    // 10. Enable TRANS_DDI_FUNC_CTL, re-assert TRANS_MSA_MISC, scaler (if saved), TRANSCONF,
    //     then settle 40 ms (matching XE-EDPWAKE 0003-deep-idle-sleep.diff:413).
    io.write(TransDdiFuncCtlA, s.transDdiFuncCtl | 0x80000000U);
    io.write(TransMsaMiscA, s.transMsaMisc);
    r.transMsaAfter = io.read(TransMsaMiscA);
    if (s.psCtrl & 0x80000000U) {
        io.write(PsWinPos1A, s.psWinPos);
        io.write(PsWinSz1A, s.psWinSz);
        io.write(PsCtrl1A, s.psCtrl);
    }
    io.write(TransConfA, (s.transconf | 0x80000000U) & ~(1U << 30));
    const bool transActive = waitMask(io, TransConfA, 1U << 30, 1U << 30, 50000, 100);
    io.sleepMs(40);
    r.transConfAfter = io.read(TransConfA);
    if (transActive && ((r.transConfAfter & 0xC0000000U) == 0xC0000000U)) r.flags |= RsTransActive;
    else r.failReasons |= FailTransActive;

    // 11. Restore backlight PWM & PP_CONTROL backlight enable if previously enabled.
    if (s.blcFreq && s.blcFreq != UINT32_MAX) io.write(BlcPwmFreq1, s.blcFreq);
    if (s.blcDuty && s.blcDuty != UINT32_MAX) io.write(BlcPwmDuty1, s.blcDuty);
    if (s.blcCtl != UINT32_MAX && (s.blcCtl & 0x80000000U)) io.write(BlcPwmCtl1, s.blcCtl);
    const uint32_t savedPp = st.teardownPpCtl ? st.teardownPpCtl : s.ppCtl;
    if (savedPp != UINT32_MAX && (savedPp & 0x4U)) {
        const uint32_t ppNow = io.read(PpControl);
        if (ppNow != UINT32_MAX) io.write(PpControl, ppNow | 0x5U);
    }

    // 12. Post-restore verification: confirm DPCD link stayed locked & panel stays powered.
    bool postDpcdOk = false;
    if (crOk && eqOk) {
        uint8_t verifySt[6] = {};
        if (auxXfer(io, false, 0x202, verifySt, 6) == 6) {
            r.lastLaneStatus[0] = verifySt[0];
            r.lastLaneStatus[1] = verifySt[1];
            r.lastLaneStatus[2] = verifySt[2];
            postDpcdOk = lanesClockRecovered(verifySt, lanes) && lanesChannelEqualized(verifySt, lanes);
        }
    }
    if (postDpcdOk) r.flags |= RsPostStatusOk;
    else if (crOk && eqOk) r.failReasons |= FailPostDpcd;

    r.ppStatusAfter = io.read(PpStatus);
    const bool panelVerified = (r.ppStatusAfter != UINT32_MAX) && ((r.ppStatusAfter & 0x80000000U) != 0);
    if (!panelVerified) r.failReasons |= FailPanelVerify;

    r.verified = (r.failReasons == FailNone) &&
                 (r.flags & RsDpllLocked) &&
                 (r.flags & RsDdiActive) &&
                 (r.flags & RsCrOk) &&
                 (r.flags & RsEqOk) &&
                 (r.flags & RsPostStatusOk) &&
                 (r.flags & RsTransActive) &&
                 panelVerified;

    if (r.verified) {
        r.flags |= RsVerified;
        ++st.verifyPassCount;
    } else {
        st.fallbackTripped = true;
        st.fallbackReason = r.failReasons;
        ++st.verifyFailCount;
    }

    st.poweredDown = false;
    st.teardownPpCtl = 0;
    st.lastRestore = r;
    return r;
}

// Phase 2b (entry): Runs AFTER Apple's prepareToExitSleep and
// prepareToEnterWake/hwSetMode complete, matching XE-EDPWAKE's post-wake
// ordering (0003-deep-idle-sleep.diff:339):
//   - If TRANSCONF_A is already enabled & active (native hwSetMode succeeded),
//     verify DPLL, DDI, DPCD 0x202..0x204, PP_STATUS, and TRANS_MSA_MISC
//     (fixing zeroed MSA in place) WITHOUT stomping on the running pipe.
//   - If TRANSCONF_A is off (or active with a dead/untrained link), cleanly
//     quiesce any half-enabled state and run full XE-EDPWAKE restoreEDP().
template<class IO>
inline RestoreResult verifyOrRestoreEDP(IO &io, State &st) {
    const Snapshot &s = st.snapshot;
    if (!s.valid) return restoreEDP(io, st);

    const uint32_t conf = io.read(TransConfA);
    if (conf != UINT32_MAX && (conf & 0xC0000000U) == 0xC0000000U) {
        RestoreResult r {};
        const uint32_t dc = io.read(DcStateEn);
        if (dc != UINT32_MAX && !(dc & Dc9Enable)) r.flags |= RsDc9Cleared;

        // If PW_A was not yet powered during Phase 2a, restore Pipe A LUTs/CSC/color now.
        if (!st.colorRestoredOnWake) {
            r.flags |= restoreColorState(io, st, false);
        } else {
            r.flags |= st.color.lastRestoreFlags;
            // Verify native hwSetMode / hwUpdateRegCache did not drop PIPE_MISC BPC/dithering.
            if (s.color.valid && (s.pipeMisc & PipeMiscBpcDitherMask) != 0U) {
                const uint32_t pmNow = io.read(PipeMiscA);
                if (pmNow != UINT32_MAX &&
                    (pmNow & PipeMiscBpcDitherMask) != (s.pipeMisc & PipeMiscBpcDitherMask)) {
                    io.write(PipeMiscA, (pmNow & ~PipeMiscBpcDitherMask) | (s.pipeMisc & PipeMiscBpcDitherMask));
                }
            }
            st.color.lastPipeMiscAfter  = io.read(PipeMiscA);
            st.color.lastGammaModeAfter = io.read(GammaModeA);
            st.color.lastCscModeAfter   = io.read(CscModeA);
        }

        // Guard against 6-bpc garble if native hwSetMode left TRANS_MSA_MISC_A == 0.
        if (s.transMsaMisc && io.read(TransMsaMiscA) == 0) {
            io.write(TransMsaMiscA, s.transMsaMisc);
        }
        r.transMsaAfter = io.read(TransMsaMiscA);
        if (r.transMsaAfter == s.transMsaMisc) r.flags |= RsMsaRestored;

        r.transFunc2After = io.read(TransDdiFuncCtl2A);
        if ((s.transDdiFuncCtl2 & TransDdiCtl2Cmtg) == (r.transFunc2After & TransDdiCtl2Cmtg)) {
            r.flags |= RsCmtgPreserved;
        }

        const uint32_t dpclkaNow = io.read(DpcaCfgcr0);
        const uint32_t activeDpllIdx =
            (dpclkaNow != UINT32_MAX && (dpclkaNow & 0x3U) <= 2U) ? (dpclkaNow & 0x3U) : s.dpllIndex;
        r.dpllAfter = io.read(dpllEnableReg(activeDpllIdx));
        const bool dpllLocked = (r.dpllAfter != UINT32_MAX) && ((r.dpllAfter & (1U << 30)) != 0);

        r.ddiBufAfter = io.read(DdiBufCtlA);
        const bool ddiActive = (r.ddiBufAfter != UINT32_MAX) &&
                               ((r.ddiBufAfter & (DdiBufEnable | DdiBufIdle)) == DdiBufEnable);

        r.ppStatusAfter = io.read(PpStatus);
        const bool panelOn = (r.ppStatusAfter != UINT32_MAX) && ((r.ppStatusAfter & 0x80000000U) != 0);

        const uint32_t lanes = resolvedLanes(s);
        bool linkTrained = false;
        uint8_t verifySt[6] = {};
        if (lanes >= 1 && lanes <= 4 && auxXfer(io, false, 0x202, verifySt, 6) == 6) {
            r.lastLaneStatus[0] = verifySt[0];
            r.lastLaneStatus[1] = verifySt[1];
            r.lastLaneStatus[2] = verifySt[2];
            linkTrained = lanesClockRecovered(verifySt, lanes) && lanesChannelEqualized(verifySt, lanes);
        }

        r.transConfAfter = conf;
        if (dpllLocked && ddiActive && panelOn && linkTrained) {
            r.flags |= RsNativeActive | RsPanelOn | RsDpllLocked | RsSinkConfigOk |
                       RsDdiActive | RsCrOk | RsEqOk | RsPostStatusOk | RsTransActive | RsVerified;
            r.verified = true;
            ++st.restoreCount;
            ++st.verifyPassCount;
            st.poweredDown = false;
            st.teardownPpCtl = 0;
            st.lastRestore = r;
            return r;
        }

        // Native modeset left TRANSCONF_A on but DPLL/DDI/DPCD/PP is unhealthy:
        // quiesce cleanly and fall through to full XE-EDPWAKE replay.
        io.write(TransConfA, conf & ~0x80000000U);
        (void)waitMask(io, TransConfA, 1U << 30, 0, 60000, 50);
        const uint32_t func = io.read(TransDdiFuncCtlA);
        if (func != UINT32_MAX && (func & 0x80000000U)) io.write(TransDdiFuncCtlA, func & ~0x80000000U);
        const uint32_t buf = io.read(DdiBufCtlA);
        if (buf != UINT32_MAX && (buf & DdiBufEnable)) io.write(DdiBufCtlA, buf & ~DdiBufEnable);
        const uint32_t tp = io.read(DpTpCtlA);
        if (tp != UINT32_MAX && (tp & 0x80000000U)) io.write(DpTpCtlA, tp & ~0x80000000U);
        (void)waitMask(io, DdiBufCtlA, DdiBufIdle, DdiBufIdle, 2000, 10);
    } else if (conf != UINT32_MAX && (conf & 0x80000000U)) {
        io.write(TransConfA, conf & ~0x80000000U);
        (void)waitMask(io, TransConfA, 1U << 30, 0, 60000, 50);
    }

    return restoreEDP(io, st);
}

} // namespace Display13DC9PwrOff
#endif
