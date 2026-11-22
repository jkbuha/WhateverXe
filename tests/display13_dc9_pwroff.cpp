// Unit tests for Display13DC9PwrOff (-wex-d13-dc9-pwroff, aligned with XE-EDPWAKE):
//   1. Snapshot capture (DPLL0 & DPLL1, X1C10 CTL2 bit 3, MSA, DPCD, firmware hint)
//   2. icl_display_core_uninit teardown order (pipes/DDI/PPS/DPLL/DBUF S3..S0 at
//      exact Display13 offsets 0x44304,0x44300,0x44FE8,0x45008/CDCLK/AUX/DDI IO/
//      PW2+Pipes/PW1 BIOS+Driver/DC9, NEVER pulsing SOUTH_CHICKEN1 0xC2000!)
//   3. Full Display13 eDP wake restore (DC9 exit, PPS delays, DPLL0/1, DPCLKA,
//      TRANS_CLK_SEL, timings, M/N, TRANS_DDI_FUNC_CTL2 bit 3, TRANS_MSA_MISC,
//      Combo PHY levels, AUX CR+EQ training TPS3 & TPS2, TRANSCONF + 40 ms settle, BLC)
//   4. Post-prepareToEnterWake verifyOrRestoreEDP() paths:
//      - native hwSetMode succeeded cleanly (preserve without stomp)
//      - native hwSetMode zeroed TRANS_MSA_MISC (in-place MSA fix)
//      - native hwSetMode left TRANSCONF_A off (full XE-EDPWAKE replay)
//      - native hwSetMode left TRANSCONF_A on with dead DPCD link (quiesce + replay)
//   5. Safety fallback triggers (DPLL lock timeout, CR failure, EQ failure,
//      post-training link drop, transcoder fail, missing snapshot)
#include "kern_display13_dc9_pwroff.hpp"
#include "kern_display13_dbuf.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

using namespace Display13DC9PwrOff;

constexpr uint32_t TestSouthChicken1 = 0xC2000;

struct MockDisplay13IO {
    std::map<uint32_t, uint32_t> regs;
    std::map<uint32_t, uint8_t> dpcd;
    std::vector<std::pair<uint32_t, uint32_t>> writes;
    uint32_t preLutSram[PreCscGamcCount] {};
    uint32_t postLutSram[PalPrecCount] {};
    uint32_t preLutIdx {};
    uint32_t postLutIdx {};
    uint32_t auxData[5] {};
    uint32_t totalDelayUs {};
    uint32_t totalSleepMs {};

    // Fault injection knobs.
    bool failDpllLock {};
    bool failCr {};
    bool failEq {};
    bool dropAfterTrain {};
    bool failTransActive {};
    bool failAuxNack {};
    bool auxDeferOnce {};
    int  auxDeferCount {};
    bool failPanelOn {};
    uint32_t statusReadCount {};

    MockDisplay13IO() { resetLive(1, true); }

    void resetLive(uint32_t dpllIdx, bool cmtgBit3) {
        regs.clear();
        dpcd.clear();
        writes.clear();
        totalDelayUs = 0;
        totalSleepMs = 0;
        failDpllLock = failCr = failEq = dropAfterTrain = failTransActive = false;
        failAuxNack = auxDeferOnce = failPanelOn = false;
        auxDeferCount = 0;
        statusReadCount = 0;

        regs[PwrWellCtlBios]   = 0x00005405U;
        regs[PwrWellCtlDriver] = 0x0000FC0FU;
        regs[PwrWellAuxBios]   = 0x00000001U;
        regs[PwrWellAuxDriver] = 0x00000003U;
        regs[PwrWellDdiBios]   = 0x00000001U;
        regs[PwrWellDdiDriver] = 0x00000003U;
        regs[DcStateEn]        = 0x00000000U;
        regs[TestSouthChicken1] = 0x00000905U;
        regs[SouthDplsGateReg] = 0x00000000U;

        for (auto r : DbufSliceRegs) regs[r] = 0xC040C000U;
        regs[CdclkCtl]       = 0x00780164U;
        regs[CdclkPllEnable] = 0xC000000EU;

        regs[dpllEnableReg(0)] = dpllIdx == 0 ? 0xCC000000U : 0U;
        regs[dpllCfgcr0Reg(0)] = dpllIdx == 0 ? 0x0001C28AU : 0U;
        regs[dpllCfgcr1Reg(0)] = dpllIdx == 0 ? 0x00000084U : 0U;
        regs[dpllEnableReg(1)] = dpllIdx == 1 ? 0xCC000000U : 0U;
        regs[dpllCfgcr0Reg(1)] = dpllIdx == 1 ? 0x0001A555U : 0U;
        regs[dpllCfgcr1Reg(1)] = dpllIdx == 1 ? 0x00000042U : 0U;

        regs[DpcaCfgcr0]     = dpllIdx;
        regs[TransClkSelA]   = 0x10000000U;

        regs[TransHtotalA]     = 0x081F077FU;
        regs[TransHblankA]     = 0x081F077FU;
        regs[TransHsyncA]      = 0x07CF07AFU;
        regs[TransVtotalA]     = 0x04CF04AFU;
        regs[TransVblankA]     = 0x04CF04AFU;
        regs[TransVsyncA]      = 0x04B904B3U;
        regs[PipeSrcA]         = 0x077F04AFU;
        regs[TransVsyncShiftA] = 0x00000000U;
        regs[PipeDataM1A]      = 0x7E400000U;
        regs[PipeDataN1A]      = 0x00800000U;
        regs[PipeLinkM1A]      = 0x00040000U;
        regs[PipeLinkN1A]      = 0x00080000U;

        regs[TransDdiFuncCtlA]  = 0x8A000102U;
        regs[TransDdiFuncCtl2A] = cmtgBit3 ? TransDdiCtl2Cmtg : 0U;
        regs[TransMsaMiscA]     = 0x00000021U;
        regs[DpTpCtlA]          = 0x80040300U;
        regs[DpTpStatusA]       = 0x00000000U;
        regs[DdiBufCtlA]        = 0x80000002U;
        regs[AuxChCtlA]         = 0x00000000U;

        regs[PsWinPos1A]        = 0x00000000U;
        regs[PsWinSz1A]         = 0x078004B0U;
        regs[PsCtrl1A]          = 0x80000000U;

        regs[TransConfA]        = 0xC0000000U;
        regs[PipeMiscA]         = 0x00000014U; // 8-bpc + dithering enabled
        regs[PipeBottomColorA]  = 0xC0000000U;
        regs[PipeFrmCountA]     = 0x00001234U;
        regs[CurCtlA]           = 0x00000027U;
        regs[CurBaseA]          = 0x00100000U;
        regs[PlaneCtl1A]        = 0x84000000U;
        regs[PlaneSurf1A]       = 0x00200000U;
        regs[PlaneColorCtl1A]   = 0x00002000U;
        regs[GammaModeA]        = 0xE0000002U; // PRE_CSC | POST_CSC | MULTI_SEG | 10-bit
        regs[CscModeA]          = 0x80000000U; // ICL_CSC_ENABLE
        for (uint32_t i = 0; i < 6; ++i) {
            regs[CscCoeffA + 4U * i]  = 0x08000000U + i;
            regs[OcscCoeffA + 4U * i] = 0x04000000U + i;
        }
        for (uint32_t i = 0; i < 3; ++i) {
            regs[CscPreOffA + 4U * i]        = 0x00000000U;
            regs[CscPostOffA + 4U * i]       = 0x00000000U;
            regs[OcscPreOffA + 4U * i]       = 0x00000000U;
            regs[OcscPostOffA + 4U * i]      = 0x00000000U;
            regs[PalPrecGcMaxA + 4U * i]     = 0x00010000U;
            regs[PalPrecExtGcMaxA + 4U * i]  = 0x00010000U;
            regs[PalPrecExt2GcMaxA + 4U * i] = 0x00010000U;
        }
        memset(preLutSram, 0, sizeof(preLutSram));
        memset(postLutSram, 0, sizeof(postLutSram));
        preLutIdx = postLutIdx = 0;

        regs[PortClDw10A]        = 0x00000015U;
        regs[PortPcsDw1GrpA]    = 0x1C000004U;
        regs[PortPcsDw1Ln0A]    = 0x1C000004U;
        regs[PortTxDw5GrpA]     = 0x80000000U;
        regs[PortTxDw5Ln0A]     = 0x80000000U;
        for (uint32_t i = 0; i < 4; ++i) {
            regs[PortTxDw2LnA[i]] = 0x10000000U + i;
            regs[PortTxDw4LnA[i]] = 0x20000000U + i;
            regs[PortTxDw7LnA[i]] = 0x30000000U + i;
        }

        regs[PpStatus]    = 0x80000008U;
        regs[PpControl]   = 0x00000007U;
        regs[PpOnDelays]  = 0x01F40001U;
        regs[PpOffDelays] = 0x01F40001U;
        regs[PpDivisor]   = 0x00186905U;
        regs[BlcPwmCtl1]  = 0x80000000U;
        regs[BlcPwmFreq1] = 0x00004B00U;
        regs[BlcPwmDuty1] = 0x00002580U;

        uint8_t caps[16] = {0x14, 0x0A, 0xC2, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01, 0x80, 0};
        uint8_t cfg[16]  = {0x0A, 0x82, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0};
        for (uint32_t i = 0; i < 16; ++i) {
            dpcd[0x000 + i] = caps[i];
            dpcd[0x100 + i] = cfg[i];
        }
        dpcd[0x115] = 0x03;
        dpcd[0x202] = 0x77;
        dpcd[0x203] = 0x00;
        dpcd[0x204] = 0x01;
        dpcd[0x600] = 0x01;
    }

    void updatePowerWells() {
        auto syncPair = [&](uint32_t bios, uint32_t drv) {
            const uint32_t combined = (regs[bios] | regs[drv]) & RequestBitsMask;
            const uint32_t states = combined >> 1;
            regs[bios] = (regs[bios] & RequestBitsMask) | states;
            regs[drv]  = (regs[drv]  & RequestBitsMask) | states;
        };
        syncPair(PwrWellCtlBios, PwrWellCtlDriver);
        syncPair(PwrWellAuxBios, PwrWellAuxDriver);
        syncPair(PwrWellDdiBios, PwrWellDdiDriver);
    }

    void simulateDc9PowerLoss() {
        for (uint32_t i = 0; i < 2; ++i) {
            regs[dpllEnableReg(i)] = 0;
            regs[dpllCfgcr0Reg(i)] = 0;
            regs[dpllCfgcr1Reg(i)] = 0;
        }
        regs[DpcaCfgcr0] = DdiAClockOff;
        regs[TransClkSelA] = 0;
        for (auto r : TimingRegs) regs[r] = 0;
        regs[TransDdiFuncCtlA] = 0;
        regs[TransDdiFuncCtl2A] = 0;
        regs[TransMsaMiscA] = 0;
        regs[DpTpCtlA] = 0;
        regs[DdiBufCtlA] = DdiBufIdle;
        regs[TransConfA] = 0;
        regs[PipeMiscA] = 0;
        regs[PipeBottomColorA] = 0;
        regs[PlaneColorCtl1A] = 0;
        regs[GammaModeA] = 0x20000000U; // Display13 HW reset value observed in live ioreg!
        regs[CscModeA] = 0;
        for (uint32_t i = 0; i < 6; ++i) {
            regs[CscCoeffA + 4U * i] = 0;
            regs[OcscCoeffA + 4U * i] = 0;
        }
        for (uint32_t i = 0; i < 3; ++i) {
            regs[CscPreOffA + 4U * i] = 0;
            regs[CscPostOffA + 4U * i] = 0;
            regs[OcscPreOffA + 4U * i] = 0;
            regs[OcscPostOffA + 4U * i] = 0;
            regs[PalPrecGcMaxA + 4U * i] = 0;
            regs[PalPrecExtGcMaxA + 4U * i] = 0;
            regs[PalPrecExt2GcMaxA + 4U * i] = 0;
        }
        memset(preLutSram, 0, sizeof(preLutSram));
        memset(postLutSram, 0, sizeof(postLutSram));
        preLutIdx = postLutIdx = 0;
        regs[PsCtrl1A] = regs[PsWinPos1A] = regs[PsWinSz1A] = 0;
        regs[PortClDw10A] = 0;
        regs[PortTxDw5Ln0A] = 0;
        for (uint32_t i = 0; i < 4; ++i) {
            regs[PortTxDw2LnA[i]] = 0;
            regs[PortTxDw4LnA[i]] = 0;
            regs[PortTxDw7LnA[i]] = 0;
        }
        regs[PpOnDelays] = regs[PpOffDelays] = regs[PpDivisor] = 0;
        regs[BlcPwmCtl1] = regs[BlcPwmFreq1] = regs[BlcPwmDuty1] = 0;
        dpcd[0x202] = dpcd[0x203] = dpcd[0x204] = 0;
    }

    void restoreWellsForWake() {
        regs[PwrWellCtlDriver] |= 0x0000080AU;
        regs[PwrWellAuxDriver] |= 0x00000002U;
        regs[PwrWellDdiDriver] |= 0x00000002U;
        updatePowerWells();
    }

    void simulateNativeHwSetMode(uint32_t dpllIdx, bool goodLink, uint32_t msa = 0x21U) {
        regs[dpllEnableReg(dpllIdx)] = 0xCC000000U;
        regs[DpcaCfgcr0] = dpllIdx;
        regs[TransDdiFuncCtlA] = 0x8A000102U;
        regs[TransDdiFuncCtl2A] = TransDdiCtl2Cmtg;
        regs[TransMsaMiscA] = msa;
        regs[DdiBufCtlA] = 0x80000002U;
        regs[DpTpCtlA] = 0x80040300U;
        regs[TransConfA] = 0xC0000000U;
        regs[PpControl] = 0x00000007U;
        regs[PpStatus] = 0x80000008U;
        dpcd[0x202] = goodLink ? 0x77 : 0x00;
        dpcd[0x203] = 0x00;
        dpcd[0x204] = goodLink ? 0x01 : 0x00;
    }

    uint32_t read(uint32_t r) {
        if (r >= AuxChDataA && r < AuxChDataA + 20) return auxData[(r - AuxChDataA) / 4];
        auto it = regs.find(r);
        return it == regs.end() ? UINT32_MAX : it->second;
    }

    void handleAuxSend(uint32_t ctl) {
        uint8_t msg[20] = {};
        for (uint32_t i = 0; i < 5; ++i) {
            msg[4*i]     = uint8_t(auxData[i] >> 24);
            msg[4*i + 1] = uint8_t(auxData[i] >> 16);
            msg[4*i + 2] = uint8_t(auxData[i] >> 8);
            msg[4*i + 3] = uint8_t(auxData[i]);
        }
        const bool isWrite = ((msg[0] >> 4) & 0xFU) == 0x8U;
        const uint32_t addr = (uint32_t(msg[0] & 0xFU) << 16) | (uint32_t(msg[1]) << 8) | msg[2];
        const uint32_t len  = uint32_t(msg[3]) + 1U;

        uint8_t rep[20] = {};
        uint32_t repLen = 1;
        if (failAuxNack) {
            rep[0] = 0x10;
        } else if (auxDeferOnce && auxDeferCount == 0) {
            ++auxDeferCount;
            rep[0] = 0x20;
        } else if (isWrite) {
            for (uint32_t i = 0; i < len; ++i) dpcd[addr + i] = msg[4 + i];
            if (addr == 0x102) {
                const uint8_t pat = msg[4] & 0x03U;
                if (pat == 1) {
                    dpcd[0x202] = failCr ? 0x00 : 0x11;
                    dpcd[0x203] = failCr ? 0x00 : 0x11;
                    dpcd[0x204] = 0x00;
                } else if (pat == 2 || pat == 3) {
                    dpcd[0x202] = failEq ? 0x11 : 0x77;
                    dpcd[0x203] = failEq ? 0x11 : 0x77;
                    dpcd[0x204] = failEq ? 0x00 : 0x01;
                } else if (pat == 0 && dropAfterTrain) {
                    dpcd[0x202] = 0x00;
                    dpcd[0x203] = 0x00;
                    dpcd[0x204] = 0x00;
                }
            }
            rep[0] = 0x00;
        } else {
            rep[0] = 0x00;
            for (uint32_t i = 0; i < len; ++i) rep[1 + i] = dpcd[addr + i];
            repLen = 1U + len;
            if (addr == 0x202) ++statusReadCount;
        }
        for (uint32_t i = 0; i < 5; ++i) {
            auxData[i] = (uint32_t(rep[4*i]) << 24) | (uint32_t(rep[4*i + 1]) << 16) |
                         (uint32_t(rep[4*i + 2]) << 8) | uint32_t(rep[4*i + 3]);
        }
        regs[AuxChCtlA] = (ctl & ~((1U << 31) | (1U << 28) | (1U << 25) | (0x1FU << 20))) |
                          (1U << 30) | ((repLen & 0x1FU) << 20);
    }

    void write(uint32_t r, uint32_t v) {
        writes.emplace_back(r, v);
        if (r == PreCscGamcIndexA) {
            regs[r] = v;
            preLutIdx = v & 0xFFU;
            return;
        }
        if (r == PreCscGamcDataA) {
            if (preLutIdx < PreCscGamcCount) preLutSram[preLutIdx] = v;
            if (regs[PreCscGamcIndexA] & PreCscGamcAutoInc) ++preLutIdx;
            regs[r] = v;
            return;
        }
        if (r == PalPrecIndexA) {
            regs[r] = v;
            postLutIdx = v & 0x3FFU;
            return;
        }
        if (r == PalPrecDataA) {
            if (postLutIdx < PalPrecCount) postLutSram[postLutIdx] = v;
            if (regs[PalPrecIndexA] & PalPrecAutoInc) ++postLutIdx;
            regs[r] = v;
            return;
        }
        if (r >= AuxChDataA && r < AuxChDataA + 20) {
            auxData[(r - AuxChDataA) / 4] = v;
            return;
        }
        if (r == AuxChCtlA) {
            if (v & (1U << 31)) handleAuxSend(v);
            else regs[r] = v & ~((1U << 30) | (1U << 28) | (1U << 25));
            return;
        }
        if (r == PortTxDw5GrpA) {
            regs[PortTxDw5GrpA] = v;
            regs[PortTxDw5Ln0A] = v;
            return;
        }
        regs[r] = v;
        if (r == PwrWellCtlBios || r == PwrWellCtlDriver ||
            r == PwrWellAuxBios || r == PwrWellAuxDriver ||
            r == PwrWellDdiBios || r == PwrWellDdiDriver) {
            updatePowerWells();
        } else if (r == TransConfA) {
            if ((v & 0x80000000U) && !failTransActive) regs[r] |= (1U << 30);
            else regs[r] &= ~(1U << 30);
        } else if (r == DdiBufCtlA) {
            if (v & DdiBufEnable) regs[r] &= ~DdiBufIdle;
            else regs[r] |= DdiBufIdle;
        } else if (r == PpControl) {
            if ((v & 1U) && !failPanelOn) regs[PpStatus] = 0x80000008U;
            else regs[PpStatus] = 0x00000000U;
        } else if (r == CdclkPllEnable) {
            if (v & (1U << 31)) regs[r] |= (1U << 30);
            else regs[r] &= ~(1U << 30);
        } else if (r == dpllEnableReg(0) || r == dpllEnableReg(1)) {
            if (v & (1U << 27)) regs[r] |= (1U << 26); else regs[r] &= ~(1U << 26);
            if ((v & (1U << 31)) && !failDpllLock) regs[r] |= (1U << 30);
            else regs[r] &= ~(1U << 30);
        } else {
            for (auto dbuf : DbufSliceRegs) {
                if (r == dbuf) {
                    if (v & DbufPowerRequest) regs[r] |= DbufPowerState;
                    else regs[r] &= ~DbufPowerState;
                }
            }
        }
    }

    void delayUs(unsigned us) { totalDelayUs += us; }
    void sleepMs(unsigned ms) { totalSleepMs += ms; }
};

static int findWriteIndex(const MockDisplay13IO &io, uint32_t reg, int after = -1) {
    for (size_t i = static_cast<size_t>(after + 1); i < io.writes.size(); ++i) {
        if (io.writes[i].first == reg) return static_cast<int>(i);
    }
    return -1;
}

int main() {
    // Verify Display13 DBUF slice offsets match kern_display13_dbuf.hpp & Linux intel_display_regs.h!
    static_assert(DbufSliceRegs[0] == 0x45008U && DbufSliceRegs[1] == 0x44FE8U &&
                  DbufSliceRegs[2] == 0x44300U && DbufSliceRegs[3] == 0x44304U,
                  "Display13 DBUF slice register offsets");

    // Test 1: Happy path — X1Y7/X1C10 DPLL1 + CMTG bit 3, BIOS+Driver wells, full sleep & wake replay.
    {
        MockDisplay13IO io;
        io.resetLive(1, true);
        io.regs[PwrWellCtlBios] = 0x0000040FU;
        io.updatePowerWells();
        io.auxDeferOnce = true;

        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(st.snapshot.valid && st.snapshot.dpllIndex == 1);
        assert((st.snapshot.transDdiFuncCtl2 & TransDdiCtl2Cmtg) == TransDdiCtl2Cmtg);
        assert(st.snapshot.transMsaMisc == 0x21U);
        assert(resolvedLanes(st.snapshot) == 2);

        io.writes.clear();
        const auto td = teardown(io, st);
        assert(td.complete);
        assert(td.wellsBiosAfter == 0U);
        assert(td.wellsDriverAfter == 0U);
        assert(io.read(PwrWellAuxBios) == 0U && io.read(PwrWellAuxDriver) == 0U);
        assert(io.read(PwrWellDdiBios) == 0U && io.read(PwrWellDdiDriver) == 0U);
        for (auto r : DbufSliceRegs) assert((io.read(r) & (DbufPowerRequest | DbufPowerState)) == 0U);
        assert((io.read(CdclkPllEnable) & 0xC0000000U) == 0U);
        assert((io.read(DcStateEn) & Dc9Enable) == Dc9Enable);
        assert(st.poweredDown);

        // Must NEVER pulse/touch SOUTH_CHICKEN1 (0xC2000) or zero TRANS_DDI_FUNC_CTL2_A during teardown!
        assert(findWriteIndex(io, TestSouthChicken1) == -1);
        assert(findWriteIndex(io, TransDdiFuncCtl2A) == -1);

        // Strict icl_display_core_uninit ordering:
        // Transcoder off < DDI off < PPS off < DPLL off < DBUF S3..S0 off < CDCLK off < AUX off < PW2 off < PW1 off < DC9!
        const int idxTrans = findWriteIndex(io, TransConfA);
        const int idxDdi   = findWriteIndex(io, DdiBufCtlA);
        const int idxPp    = findWriteIndex(io, PpControl);
        const int idxDpll  = findWriteIndex(io, dpllEnableReg(1));
        const int idxDbuf3 = findWriteIndex(io, 0x44304U);
        const int idxDbuf2 = findWriteIndex(io, 0x44300U);
        const int idxDbuf1 = findWriteIndex(io, 0x44FE8U);
        const int idxDbuf0 = findWriteIndex(io, 0x45008U);
        const int idxCd    = findWriteIndex(io, CdclkPllEnable);
        const int idxAux   = findWriteIndex(io, PwrWellAuxDriver);
        const int idxPw2   = findWriteIndex(io, PwrWellCtlDriver);
        const int idxPw1   = findWriteIndex(io, PwrWellCtlDriver, idxPw2);
        const int idxDc9   = findWriteIndex(io, DcStateEn);
        assert(idxTrans >= 0 && idxTrans < idxDdi && idxDdi < idxPp && idxPp < idxDpll);
        assert(idxDpll < idxDbuf3 && idxDbuf3 < idxDbuf2 && idxDbuf2 < idxDbuf1 && idxDbuf1 < idxDbuf0 && idxDbuf0 < idxCd);
        assert(idxCd < idxAux && idxAux < idxPw2 && idxPw2 < idxPw1 && idxPw1 < idxDc9);
        assert(io.totalSleepMs == 0); // Teardown must never call sleepMs!

        // Simulate real DC9 hardware reset, exitDC9 (no SOUTH_CHICKEN1 write!), restore PPS delays, wake!
        io.simulateDc9PowerLoss();
        io.writes.clear();
        assert(exitDC9(io));
        assert(findWriteIndex(io, TestSouthChicken1) == -1);
        assert((io.read(DcStateEn) & Dc9Enable) == 0U);
        restorePpsDelaysIfZero(io, st.snapshot);
        assert(io.read(PpOnDelays) == 0x01F40001U && io.read(PpOffDelays) == 0x01F40001U && io.read(PpDivisor) == 0x00186905U);
        io.restoreWellsForWake();

        io.writes.clear();
        io.totalSleepMs = 0;
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified);
        assert(!(rs.flags & RsNativeActive));
        assert(rs.failReasons == FailNone);
        assert(io.totalSleepMs >= 41U); // 1 ms DPCD D0 + 40 ms post-TRANSCONF settle (XE-EDPWAKE)
        assert(!st.fallbackTripped && !st.poweredDown);
        assert(st.verifyPassCount == 1 && st.verifyFailCount == 0);
        assert((rs.flags & RsVerified) && (rs.flags & RsCmtgPreserved) && (rs.flags & RsMsaRestored));
        assert((rs.flags & RsPhyRestored) && (rs.flags & RsCrOk) && (rs.flags & RsEqOk) && (rs.flags & RsPostStatusOk));
        assert(io.read(TransDdiFuncCtl2A) == TransDdiCtl2Cmtg);
        assert(io.read(TransMsaMiscA) == 0x21U);
        assert(io.read(TransConfA) == 0xC0000000U);
        assert((io.read(DdiBufCtlA) & (DdiBufEnable | DdiBufIdle)) == DdiBufEnable);
        assert((io.read(PpControl) & 0x5U) == 0x5U);
        assert(io.read(BlcPwmCtl1) == 0x80000000U);
        assert(io.dpcd[0x100] == 0x0A && io.dpcd[0x101] == 0x82 && io.dpcd[0x102] == 0x00);
        assert(io.dpcd[0x600] == 0x01);
        assert(io.read(PortTxDw2LnA[0]) == 0x10000000U && io.read(PortClDw10A) == 0x15U);
        assert((io.read(SouthDplsGateReg) & SouthDplsGateBit) == 0U);
    }

    // Test 2: DPLL0 selection + rateSet (eDP 1.4 LINK_RATE_SET when LINK_BW_SET == 0) + TPS2 fallback.
    {
        MockDisplay13IO io;
        io.resetLive(0, false);
        io.dpcd[0x002] = 0x82;
        io.dpcd[0x100] = 0x00;
        io.dpcd[0x115] = 0x03;

        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(st.snapshot.dpllIndex == 0);
        assert(teardown(io, st).complete);
        assert((io.read(dpllEnableReg(0)) & 0xCC000000U) == 0U);

        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified && !st.fallbackTripped);
        assert((io.read(dpllEnableReg(0)) & (1U << 30)) != 0);
        assert(io.read(TransDdiFuncCtl2A) == 0U);
        assert(io.dpcd[0x115] == 0x03);
    }

    // Test 3: Safety fallback — Clock Recovery failure trips fallbackTripped.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.failCr = true;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert(rs.failReasons & FailClockRec);
        assert(st.fallbackTripped && (st.fallbackReason & FailClockRec));
        assert(st.verifyFailCount == 1);
    }

    // Test 4: Safety fallback — Channel Equalization failure trips fallbackTripped.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.failEq = true;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert(rs.failReasons & FailChannelEq);
        assert(st.fallbackTripped);
    }

    // Test 5: Safety fallback — Link drops right after training ends.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.dropAfterTrain = true;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert((rs.flags & RsCrOk) && (rs.flags & RsEqOk));
        assert(rs.failReasons & FailPostDpcd);
        assert(st.fallbackTripped);
    }

    // Test 6: Safety fallback — DPLL lock timeout trips fallbackTripped early.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.failDpllLock = true;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert(rs.failReasons & FailDpllLock);
        assert(st.fallbackTripped && !st.poweredDown);
    }

    // Test 7: Safety fallback — Transcoder fails to go active trips fallbackTripped.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.failTransActive = true;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert(rs.failReasons & FailTransActive);
        assert(st.fallbackTripped);
    }

    // Test 8: Safety fallback — No valid snapshot refuses teardown and fails restore cleanly.
    {
        MockDisplay13IO io;
        State st {};
        const auto td = teardown(io, st);
        assert(!td.complete && !st.poweredDown);
        assert(io.writes.empty());
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified && (rs.failReasons & FailNoSnapshot) && st.fallbackTripped);
    }

    // Test 9: FirmwareHint fills LINK_BW_SET when DPCD 0x100 and 0x115 are both 0.
    {
        MockDisplay13IO io;
        io.dpcd[0x100] = 0x00;
        io.dpcd[0x115] = 0x00;
        State st {};
        assert(captureSnapshot(io, st.snapshot, FirmwareHint{0x0A, 0x00, 0x0A}));
        assert(st.snapshot.dpcdCfg[0] == 0x0A);
    }

    // Test 10: Real WhateverXe Tahoe sleep order — donor already disabled pipe/DDI/PPS/CDCLK,
    // leaving DBUF S0..S3 + PWR_WELL_CTL1 (0x5405) + CTL2 (0xFC0F) + orphan DPLL0 on.
    {
        MockDisplay13IO io;
        io.resetLive(1, true);
        io.regs[dpllEnableReg(0)] = 0xCC000000U;
        State st {};
        assert(captureSnapshot(io, st.snapshot));

        io.write(PlaneCtl1A, 0U);
        io.write(CurCtlA, 0U);
        io.write(TransConfA, 0U);
        io.write(TransDdiFuncCtlA, 0U);
        io.write(DdiBufCtlA, DdiBufIdle);
        io.write(PpControl, 0U);
        io.write(CdclkPllEnable, 0x0000000EU);

        const auto td = teardown(io, st);
        assert(td.complete);
        assert(td.wellsBiosAfter == 0U && td.wellsDriverAfter == 0U);
        assert((io.read(dpllEnableReg(0)) & 0xCC000000U) == 0U);
        assert((io.read(dpllEnableReg(1)) & 0xCC000000U) == 0U);
        for (auto r : DbufSliceRegs) assert((io.read(r) & (DbufPowerRequest | DbufPowerState)) == 0U);
        assert((io.read(DcStateEn) & Dc9Enable) == Dc9Enable);

        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified && !st.fallbackTripped);
        assert(io.read(TransDdiFuncCtl2A) == TransDdiCtl2Cmtg);
        assert((io.read(PpControl) & 0x5U) == 0x5U);
    }

    // Test 11: 4-lane eDP panel.
    {
        MockDisplay13IO io;
        io.resetLive(1, true);
        io.dpcd[0x101] = 0x84;
        io.regs[TransDdiFuncCtlA] = 0x8A000106U;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(resolvedLanes(st.snapshot) == 4);
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        assert(verifyOrRestoreEDP(io, st).verified);

        uint8_t partial[3] = {0x77, 0x11, 0x01};
        assert(lanesClockRecovered(partial, 4));
        assert(!lanesChannelEqualized(partial, 4));
    }

    // Test 12: 1-lane eDP panel + extended caps bit 7 in DPCD 0x00E masked to 400 us.
    {
        MockDisplay13IO io;
        io.resetLive(1, false);
        io.dpcd[0x00E] = 0x80;
        io.dpcd[0x101] = 0x81;
        io.regs[TransDdiFuncCtlA] = 0x8A000100U;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(resolvedLanes(st.snapshot) == 1);
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.totalDelayUs = 0;
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified);
        assert(io.totalDelayUs < 20000U);
    }

    // Test 13: AUX well off rejects auxXfer cleanly; panel-on timeout trips FailPanelPower.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.failPanelOn = true;
        io.regs[PwrWellAuxDriver] = 0U;
        io.updatePowerWells();
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert((rs.failReasons & FailPanelPower) && (rs.failReasons & FailSinkConfig));
        assert(st.fallbackTripped);
    }

    // Test 14: 3 clean consecutive sleep/wake cycles, then 4th cycle fails EQ -> fallback trips.
    {
        MockDisplay13IO io;
        State st {};
        for (int cycle = 0; cycle < 3; ++cycle) {
            assert(captureSnapshot(io, st.snapshot));
            assert(teardown(io, st).complete);
            io.simulateDc9PowerLoss();
            assert(exitDC9(io));
            io.restoreWellsForWake();
            const auto rs = verifyOrRestoreEDP(io, st);
            assert(rs.verified && !st.fallbackTripped);
        }
        assert(st.teardownCount == 3 && st.restoreCount == 3 && st.verifyPassCount == 3 && st.verifyFailCount == 0);
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.failEq = true;
        const auto rs4 = verifyOrRestoreEDP(io, st);
        assert(!rs4.verified && st.fallbackTripped && st.verifyFailCount == 1);
    }

    // Test 15: Post-prepareToEnterWake native hwSetMode succeeded cleanly -> verifyOrRestoreEDP
    // preserves the running pipe without stomping (RsNativeActive | RsVerified, zero modeset writes).
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.simulateNativeHwSetMode(0, true, 0x21U); // Even if native chose DPLL0 instead of DPLL1!

        io.writes.clear();
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified);
        assert((rs.flags & RsNativeActive) && (rs.flags & RsVerified));
        assert(!st.poweredDown && !st.fallbackTripped);
        assert(findWriteIndex(io, TransConfA) == -1);
        assert(findWriteIndex(io, DdiBufCtlA) == -1);
        assert(findWriteIndex(io, TransMsaMiscA) == -1);
    }

    // Test 16: Post-prepareToEnterWake native hwSetMode succeeded but left TRANS_MSA_MISC_A == 0 ->
    // verifyOrRestoreEDP fixes MSA in place without tearing down the active link!
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.simulateNativeHwSetMode(1, true, 0U);

        io.writes.clear();
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified);
        assert((rs.flags & RsNativeActive) && (rs.flags & RsMsaRestored));
        assert(io.read(TransMsaMiscA) == 0x21U);
        assert(findWriteIndex(io, TransConfA) == -1);
    }

    // Test 17: Post-prepareToEnterWake native hwSetMode left TRANSCONF_A ON with dead DPCD link ->
    // verifyOrRestoreEDP cleanly quiesces and runs full XE-EDPWAKE replay!
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.simulateNativeHwSetMode(1, false, 0x21U); // Dead DPCD 0x202 == 0x00!

        io.writes.clear();
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified);
        assert(!(rs.flags & RsNativeActive));
        assert((rs.flags & RsVerified) && (rs.flags & RsCrOk) && (rs.flags & RsEqOk) && (rs.flags & RsPostStatusOk));
        assert(io.read(TransConfA) == 0xC0000000U);
        assert(io.dpcd[0x202] == 0x77 && io.dpcd[0x204] == 0x01);
        assert(!st.poweredDown && !st.fallbackTripped);
    }

    // Test 18: Native hwSetMode dead link AND replay EQ fails -> trips fallbackTripped cleanly.
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        io.simulateNativeHwSetMode(1, false, 0x21U);
        io.failEq = true;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(!rs.verified);
        assert(rs.failReasons & FailChannelEq);
        assert(st.fallbackTripped && !st.poweredDown);
    }

    // Test 19: LUT cache accepts valid non-zero tables and rejects all-zero fade-to-black tables.
    {
        State st {};
        uint32_t zerosPre[PreCscGamcCount] {};
        uint32_t zerosPost[PalPrecCount] {};
        assert(!cachePreGamma(st, zerosPre));
        assert(!st.color.preGammaValid && st.color.preZeroSkipCount == 1);
        assert(!cachePostGamma(st, zerosPost));
        assert(!st.color.postGammaValid && st.color.postZeroSkipCount == 1);

        uint32_t pre[PreCscGamcCount] {};
        for (uint32_t i = 0; i < PreCscGamcCount; ++i) pre[i] = i * 512U;
        pre[128] = 0x00010000U;
        assert(cachePreGamma(st, pre));
        assert(st.color.preGammaValid && st.color.preCacheCount == 1 && st.color.preGamma[128] == 0x00010000U);

        uint32_t post[PalPrecCount] {};
        for (uint32_t i = 0; i < PalPrecCount; ++i) post[i] = (i << 20) | (i << 10) | i;
        const uint32_t extMax[6] = {0x10000U, 0x0F800U, 0x0F000U, 0x10000U, 0x0F800U, 0x0F000U};
        assert(cachePostGamma(st, post, extMax, true));
        assert(st.color.postGammaValid && st.color.postExtMaxValid && st.color.postCacheCount == 1);

        // Pre-sleep fade-to-black (all-zero table) must NOT overwrite the cached real table!
        assert(!cachePostGamma(st, zerosPost, nullptr, false));
        assert(st.color.postGammaValid && st.color.postZeroSkipCount == 2);
        assert(st.color.postGamma[1023] == ((1023U << 20) | (1023U << 10) | 1023U));
        assert(st.color.postExtMax[1] == 0x0F800U);
    }

    // Test 20: Phase 2a pre-modeset restoreColorState() reloads 131-entry Pre-CSC SRAM,
    // 1024-entry Post-CSC SRAM + EXT/EXT2 GC_MAX, CSC matrices, GAMMA_MODE, and PIPE_MISC
    // BEFORE hwUpdateRegCache() runs!
    {
        MockDisplay13IO io;
        State st {};
        uint32_t pre[PreCscGamcCount] {};
        for (uint32_t i = 0; i < PreCscGamcCount; ++i) pre[i] = i * 512U + 1U;
        pre[128] = 0x00010000U;
        uint32_t post[PalPrecCount] {};
        for (uint32_t i = 0; i < PalPrecCount; ++i) post[i] = (i << 20) | (i << 10) | i;
        const uint32_t extMax[6] = {0x10000U, 0x0FC00U, 0x0F800U, 0x10000U, 0x0FC00U, 0x0F800U};
        assert(cachePreGamma(st, pre));
        assert(cachePostGamma(st, post, extMax, true));

        assert(captureSnapshot(io, st.snapshot));
        assert(st.snapshot.color.valid);
        assert(teardown(io, st).complete);
        assert(st.preGammaPromotePending && !st.colorRestoredOnWake);

        io.simulateDc9PowerLoss();
        assert(io.read(GammaModeA) == 0x20000000U);
        assert(io.read(PipeMiscA) == 0U);
        assert(io.preLutSram[128] == 0U && io.postLutSram[1023] == 0U);

        // Before PW_A is restored, restoreColorState() must cleanly no-op.
        assert(restoreColorState(io, st, true) == 0U);
        assert(!st.colorRestoredOnWake);

        assert(exitDC9(io));
        io.restoreWellsForWake();

        io.writes.clear();
        const uint32_t cf = restoreColorState(io, st, true);
        assert((cf & (RsColorPreLut | RsColorPostLut | RsColorCsc | RsColorMisc)) ==
               (RsColorPreLut | RsColorPostLut | RsColorCsc | RsColorMisc));
        assert(st.colorRestoredOnWake && st.color.preModesetRestoreCount == 1);
        // Verify Linux i915 SRAM load & auto-increment clear!
        assert(io.read(PreCscGamcIndexA) == 0U);
        assert(io.read(PalPrecIndexA) == 0U);
        assert(io.preLutSram[0] == 1U && io.preLutSram[128] == 0x00010000U && io.preLutSram[130] == 130U * 512U + 1U);
        assert(io.postLutSram[0] == 0U && io.postLutSram[1023] == ((1023U << 20) | (1023U << 10) | 1023U));
        assert(io.read(PalPrecExtGcMaxA + 4U) == 0x0FC00U);
        assert(io.read(PalPrecExt2GcMaxA + 8U) == 0x0F800U);
        assert(io.read(CscModeA) == 0x80000000U);
        assert(io.read(CscCoeffA) == 0x08000000U);
        // Verify hwUpdateRegCache() would now see real pre-sleep GAMMA_MODE & PIPE_MISC!
        assert(io.read(GammaModeA) == 0xE0000002U);
        assert(io.read(PipeMiscA) == 0x00000014U);
        assert(io.read(PipeBottomColorA) == 0xC0000000U);
        // Verify LUTs were loaded BEFORE GAMMA_MODE armed them (Linux i915 ordering)!
        const int idxPreData = findWriteIndex(io, PreCscGamcDataA);
        const int idxPostData = findWriteIndex(io, PalPrecDataA);
        const int idxGammaMode = findWriteIndex(io, GammaModeA);
        assert(idxPreData >= 0 && idxPostData >= 0 && idxGammaMode > idxPreData && idxGammaMode > idxPostData);
    }

    // Test 21: Native wake path fixes wiped PIPE_MISC BPC/dither bits even if hwSetMode zeroed them,
    // and propagates RsColor* flags to RestoreResult.
    {
        MockDisplay13IO io;
        State st {};
        uint32_t pre[PreCscGamcCount] {};
        pre[128] = 0x00010000U;
        assert(cachePreGamma(st, pre));
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        assert(restoreColorState(io, st, true) != 0U);
        io.simulateNativeHwSetMode(0, true, 0x21U);
        // Simulate native hwSetMode accidentally clearing PIPE_MISC dither bit 4.
        io.regs[PipeMiscA] = 0x00000000U;

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified && (rs.flags & RsNativeActive) && (rs.flags & RsColorPreLut) && (rs.flags & RsColorMisc));
        assert((io.read(PipeMiscA) & PipeMiscBpcDitherMask) == 0x14U);
    }

    // Test 22: Deferred color restore when PW_A was off during Phase 2a (dark wake -> full wake):
    // verifyOrRestoreEDP() runs restoreColorState(io, st, false) on native wake!
    {
        MockDisplay13IO io;
        State st {};
        uint32_t pre[PreCscGamcCount] {};
        pre[64] = 0x00008000U;
        uint32_t post[PalPrecCount] {};
        post[512] = 0x20080200U;
        assert(cachePreGamma(st, pre));
        assert(cachePostGamma(st, post));
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        // Phase 2a skipped (colorRestoredOnWake stays false), then native wake happens.
        io.restoreWellsForWake();
        io.simulateNativeHwSetMode(0, true, 0x21U);

        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified && (rs.flags & RsNativeActive));
        assert((rs.flags & (RsColorPreLut | RsColorPostLut | RsColorCsc | RsColorMisc)) ==
               (RsColorPreLut | RsColorPostLut | RsColorCsc | RsColorMisc));
        assert(st.colorRestoredOnWake && st.color.postModesetRestoreCount == 1);
        assert(io.preLutSram[64] == 0x00008000U && io.postLutSram[512] == 0x20080200U);
        assert(io.read(GammaModeA) == 0xE0000002U);
    }

    // Test 23: Full XE-EDPWAKE replay path restores LUTs + CSC + GAMMA_MODE BEFORE TRANSCONF_A enable!
    {
        MockDisplay13IO io;
        State st {};
        uint32_t pre[PreCscGamcCount] {};
        pre[128] = 0x00010000U;
        uint32_t post[PalPrecCount] {};
        post[1023] = 0x3FFFFFFFU;
        assert(cachePreGamma(st, pre));
        assert(cachePostGamma(st, post));
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();

        io.writes.clear();
        const auto rs = verifyOrRestoreEDP(io, st);
        assert(rs.verified && !(rs.flags & RsNativeActive));
        assert((rs.flags & (RsColorPreLut | RsColorPostLut | RsColorCsc | RsColorMisc)) ==
               (RsColorPreLut | RsColorPostLut | RsColorCsc | RsColorMisc));
        assert(io.read(PlaneColorCtl1A) == 0x00002000U);
        const int idxGamma = findWriteIndex(io, GammaModeA);
        const int idxTrans = findWriteIndex(io, TransConfA);
        assert(idxGamma >= 0 && idxTrans > idxGamma);
    }

    // Test 24: Safety masking — if Pre-CSC or Post-CSC LUT was NEVER cached before sleep,
    // restoreColorState() MUST NOT set PRE_CSC_GAMMA_ENABLE or POST_CSC_GAMMA_ENABLE onto empty SRAM!
    {
        MockDisplay13IO io;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();

        const uint32_t cf = restoreColorState(io, st, true);
        assert(!(cf & RsColorPreLut) && !(cf & RsColorPostLut));
        assert((cf & RsColorCsc) && (cf & RsColorMisc));
        assert((io.read(GammaModeA) & (GammaModePreEn | GammaModePostEn | GammaModeMultiSeg)) == 0U);
        assert((io.read(GammaModeA) & 0x3U) == 0x2U);
    }


    // Test 25: Fade-to-black snapshot GAMMA_MODE (0x20000001) with valid cached Pre/Post LUTs
    // restores PRE_CSC | POST_CSC enable bits (0xE0000001).
    {
        MockDisplay13IO io;
        io.regs[GammaModeA] = 0x20000001U;
        State st {};
        assert(captureSnapshot(io, st.snapshot));
        assert(st.snapshot.color.gammaMode == 0x20000001U);
        uint32_t pre[PreCscGamcCount], post[PalPrecCount];
        for (uint32_t i = 0; i < PreCscGamcCount; ++i) pre[i] = i + 1U;
        for (uint32_t i = 0; i < PalPrecCount; ++i) post[i] = (i << 20) | (i << 10) | i;
        assert(cachePreGamma(st, pre));
        assert(cachePostGamma(st, post));
        assert(teardown(io, st).complete);
        io.simulateDc9PowerLoss();
        assert(exitDC9(io));
        io.restoreWellsForWake();
        const uint32_t cf = restoreColorState(io, st, true);
        assert((cf & (RsColorPreLut | RsColorPostLut | RsColorMisc)) == (RsColorPreLut | RsColorPostLut | RsColorMisc));
        assert(io.read(GammaModeA) == 0xE0000001U);
    }

    // Test 26: Preserving Display13DBUF initialRequests (0x3 = S0|S1) across DC9 teardown
    // keeps Slice S1 powered when post-wake Complete modeset temporarily allocates 0x03FF0028 (S0 only).
    {
        struct DbufIO {
            uint32_t regs[0x80000 / 4] {};
            bool readable(uint32_t r) const { return (r & 3U) == 0 && r < sizeof(regs); }
            uint32_t read(uint32_t r) const { return readable(r) ? regs[r / 4] : UINT32_MAX; }
            void write(uint32_t r, uint32_t v) {
                if (!readable(r)) return;
                if (r == 0x45008U || r == 0x44FE8U || r == 0x44300U || r == 0x44304U) {
                    v = (v & ~(1U << 30)) | ((v & (1U << 31)) ? (1U << 30) : 0U);
                }
                regs[r / 4] = v;
            }
            void delayUs(unsigned) {}
        } dio;
        dio.regs[0x45404 / 4] = 0x0403U;
        dio.regs[0x4438C / 4] = 0x1C000700U; // Unjoined ADL-P MBUS 2x2 hashing
        dio.regs[0x70008 / 4] = 0xC0000000U;
        dio.regs[0x70180 / 4] = 0x84000000U;
        dio.regs[0x7027C / 4] = 0x07FF0028U;
        dio.regs[0x7017C / 4] = 0x00270000U;
        dio.regs[0x45008 / 4] = 0xC0010000U; // S0 ON
        dio.regs[0x44FE8 / 4] = 0xC0010000U; // S1 ON

        Display13DBUF::State db {};
        assert(Display13DBUF::begin(dio, db) == Display13DBUF::Status::Complete);
        assert(db.initialRequests == 3U);

        // Simulate DC9 teardown: all 4 slices powered off in HW; software resets owned/requested/powered
        // while preserving initialRequests (0x3).
        for (uint32_t r : Display13DBUF::regs) dio.write(r, 0U);
        dio.regs[0x70008 / 4] = 0U;
        dio.regs[0x70180 / 4] = 0U;
        db.owned = 0; db.requested = 0; db.powered = 0;
        db.restorePending |= db.selected;

        // Wake: resumeEngine brings S0+S1 back ON cleanly.
        assert(Display13DBUF::resumeEngine(dio, db) == Display13DBUF::Status::Complete);
        assert(db.owned == 3U && db.requested == 3U && db.powered == 3U);
        assert((dio.read(0x45008U) & (1U << 30)) && (dio.read(0x44FE8U) & (1U << 30)));

        // Post-wake Complete modeset temporarily shrinks Pipe A primary to 0x03FF0028 (S0 only) and retires.
        assert(Display13DBUF::preparePrimary(dio, db, 40, 0x3FF, 0) == Display13DBUF::Status::Complete);
        dio.regs[0x7027C / 4] = 0x03FF0028U;
        assert(Display13DBUF::finishPrimary(dio, db, 0) == Display13DBUF::Status::Complete);
        assert(Display13DBUF::retire(dio, db, 0x03FF0028U, 0x00270000U, 10, 11, 0) == Display13DBUF::Status::Complete);
        assert(db.requested == 3U && db.powered == 3U && db.owned == 3U);
        assert((dio.read(0x45008U) & (1U << 30)) && (dio.read(0x44FE8U) & (1U << 30)));

        // Subsequent stop() before Sleep #2 stays consistent without fault.
        assert(Display13DBUF::stop(dio, db) == Display13DBUF::Status::Complete);
        assert(!db.faulted && db.requested == 3U && db.owned == 3U);
    }


    // Test 27: Multi-sleep DC9 DBUF lifecycle (Sleep #1 -> Wake #1 -> 0x03FF0028 retire ->
    // Sleep #2 stop() clears driver-owned initialRequests so owned==0 for suspend() -> Wake #2).
    {
        struct DbufIO2 {
            uint32_t regs[0x80000 / 4] {};
            bool readable(uint32_t r) const { return (r & 3U) == 0 && r < sizeof(regs); }
            uint32_t read(uint32_t r) const { return readable(r) ? regs[r / 4] : UINT32_MAX; }
            void write(uint32_t r, uint32_t v) {
                if (!readable(r)) return;
                if (r == 0x45008U || r == 0x44FE8U || r == 0x44300U || r == 0x44304U) {
                    v = (v & ~(1U << 30)) | ((v & (1U << 31)) ? (1U << 30) : 0U);
                }
                regs[r / 4] = v;
            }
            void delayUs(unsigned) {}
        } dio;
        dio.regs[0x45404 / 4] = 0x0403U;
        dio.regs[0x4438C / 4] = 0x1C000700U;
        dio.regs[0x70008 / 4] = 0xC0000000U;
        dio.regs[0x70180 / 4] = 0x84000000U;
        dio.regs[0x7027C / 4] = 0x07FF0028U;
        dio.regs[0x7017C / 4] = 0x00270000U;
        for (uint32_t r : Display13DBUF::regs) dio.regs[r / 4] = 0xC0010000U; // BIOS S0..S3 ON

        Display13DBUF::State db {};
        assert(Display13DBUF::begin(dio, db) == Display13DBUF::Status::Complete);
        assert(db.initialRequests == 0xFU && db.owned == 0U);

        // Sleep #1: BIOS still owns S0..S3 -> guard MUST NOT clear initialRequests before stop()
        dio.regs[0x70008 / 4] = 0U; dio.regs[0x70180 / 4] = 0U;
        assert((db.initialRequests & ~db.owned) != 0U);
        assert(Display13DBUF::stop(dio, db) == Display13DBUF::Status::Complete);
        assert(db.owned == 0U);
        // DC9 teardown clears HW + software state
        for (uint32_t r : Display13DBUF::regs) dio.write(r, 0U);
        db.initialRequests = 0; db.owned = 0; db.requested = 0; db.powered = 0;
        db.restorePending |= db.selected;

        // Wake #1: enableDisplay13EngineLocked sets initialRequests=3U before resumeEngine()
        db.initialRequests = 3U;
        assert(Display13DBUF::resumeEngine(dio, db) == Display13DBUF::Status::Complete);
        assert(db.owned == 3U && db.requested == 3U && db.powered == 3U);
        assert((dio.read(0x45008U) & (1U << 30)) && (dio.read(0x44FE8U) & (1U << 30)));
        assert(!(dio.read(0x44300U) & (1U << 30)) && !(dio.read(0x44304U) & (1U << 30)));

        // Modeset 0x03FF0028 + retire: S1 MUST stay powered ON!
        assert(Display13DBUF::preparePrimary(dio, db, 40, 0x3FF, 0) == Display13DBUF::Status::Complete);
        dio.regs[0x7027C / 4] = 0x03FF0028U;
        assert(Display13DBUF::finishPrimary(dio, db, 0) == Display13DBUF::Status::Complete);
        assert(Display13DBUF::retire(dio, db, 0x03FF0028U, 0x00270000U, 20, 21, 0) == Display13DBUF::Status::Complete);
        assert(db.owned == 3U && db.powered == 3U && (dio.read(0x44FE8U) & (1U << 30)));

        // Sleep #2: guard sees (initialRequests & ~owned)==0 -> clears initialRequests -> stop() sets owned==0!
        assert((db.initialRequests & ~db.owned) == 0U);
        db.initialRequests = 0;
        assert(Display13DBUF::stop(dio, db) == Display13DBUF::Status::Complete);
        assert(db.owned == 0U && db.requested == 0U && db.powered == 0U && !db.faulted);
        assert(!(dio.read(0x45008U) & (1U << 30)) && !(dio.read(0x44FE8U) & (1U << 30)));
    }

    printf("display13_dc9_pwroff: ok (27 test groups passed)\n");
    return 0;
}
