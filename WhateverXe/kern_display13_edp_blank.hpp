// -wex-edp-blank (only together with -wex-edp-keep): display sleep for the
// kept firmware eDP link without running the donor's Doze -> Wake transition.
// That transition stops the link, runs a complete mode set and leaves OLED
// eDP 1.4 panels (Samsung ATNA family) dark. Instead, keep the pipe,
// transcoder and link running and only stop scanning out: Doze/Off clear the
// primary plane and cursor enables (the pipe then emits its black background,
// an OLED emits no light), Wake restores them.
// First implemented and verified on a Galaxy Book3 Ultra by vikenzo196
// (fork commit 4c2a262); the same idea as the "backlight only" DPMS of
// NootedGreen. Plane/cursor registers are latched by the following SURF/BASE write.
#ifndef kern_display13_edp_blank_hpp
#define kern_display13_edp_blank_hpp
#include <stdint.h>

namespace Display13EDPBlank {
constexpr uint32_t PlaneCtl=0x70180, PlaneSurf=0x7019C; // PLANE_CTL_1_A / PLANE_SURF_1_A
constexpr uint32_t CurCtl=0x70080, CurBase=0x70084;     // CUR_CTL_A / CUR_BASE_A
constexpr uint32_t PlaneEnable=0x80000000U;
constexpr uint32_t CursorMode=0x27; // MCURSOR_MODE_MASK; 0 disables the cursor.
// Donor framebuffer power states: 0 off, 1 doze, 2 wake.
constexpr uint32_t Wake=2;

struct State {
    bool blanked {};
    uintptr_t fb {};              // The framebuffer whose panel is blanked.
    uint32_t plane {}, cursor {}; // Values seen when blanking.
    uint32_t blanks {}, unblanks {}, forgets {};
};
enum class Action : uint32_t { Native, Blank, Unblank, Keep };

// eligible: the framebuffer scans out the adopted boot panel on a kept link.
// owner: the framebuffer is the one whose panel is blanked.
// current: the donor's recorded power state, requested: the new one.
inline Action decide(bool eligible,bool owner,uint32_t current,uint32_t requested,bool blanked) {
    // Another framebuffer (an external display, an offline connector) never
    // touches the blanked panel: only the panel's own wake brings it back.
    if(blanked && !owner)return Action::Native;
    // Off is a system sleep (S3, hibernation): the display hardware may lose
    // power and the firmware relights the panel on resume. Never intercept it:
    // the donor must run its own sleep preparation and, afterwards, its own
    // wake. The caller releases any blank state first without a register write.
    if(requested==0)return Action::Native;
    // The panel's own wake never leaves it dark, kept link or not.
    if(!eligible || requested>Wake || current>Wake)return blanked && requested==Wake ? Action::Unblank : Action::Native;
    if(requested<Wake)return blanked ? Action::Keep : Action::Blank;
    if(blanked)return Action::Unblank;
    // Already awake: nothing to do. A wake from a native doze stays native.
    return current==Wake ? Action::Keep : Action::Native;
}

template<class IO> void blank(IO &io,State &s) {
    s.plane=io.read(PlaneCtl);s.cursor=io.read(CurCtl);
    io.write(CurCtl,s.cursor&~CursorMode);io.write(CurBase,io.read(CurBase));
    io.write(PlaneCtl,s.plane&~PlaneEnable);io.write(PlaneSurf,io.read(PlaneSurf));
    s.blanked=true;++s.blanks;
}
// Restore only the enables: the donor may legitimately reprogram format,
// tiling or cursor size while the panel is dark (a later flip or mode set).
template<class IO> void unblank(IO &io,State &s) {
    const auto plane=io.read(PlaneCtl);
    io.write(PlaneCtl,plane|(s.plane&PlaneEnable));io.write(PlaneSurf,io.read(PlaneSurf));
    const auto cursor=io.read(CurCtl);
    if(!(cursor&CursorMode) && (s.cursor&CursorMode)) {
        io.write(CurCtl,(cursor&~CursorMode)|(s.cursor&CursorMode));io.write(CurBase,io.read(CurBase));
    }
    s.blanked=false;s.fb=0;++s.unblanks;
}
// The panel is no longer scanned out by pipe A, or the display hardware is not
// up (a real system sleep): nothing to restore, and no register is written.
inline void forget(State &s) {s.blanked=false;s.fb=0;++s.forgets;}
}
#endif
