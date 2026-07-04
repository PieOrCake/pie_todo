#pragma once

#include <cstdint>
#include "imgui.h"

/* ── Pie UI theme broadcast (optional peer addon) ──────────────────────────────
 *
 * Pie UI broadcasts its active ImGui colour palette over the Nexus event bus so
 * other addons can match its look. This module subscribes to that broadcast and
 * exposes the copied palette for the Boring-mode theme to apply.
 *
 * Contract:
 *   EV_PIEUI_THEME          — Pie raises this with a PieUiTheme*. We subscribe.
 *   EV_PIEUI_REQUEST_THEME  — we raise this once on load to ask Pie to broadcast
 *                             (covers the case where Pie loaded before us).
 *
 * The event pointer is valid only for the duration of the handler call, so we
 * copy the struct immediately. The handler may run on a non-render thread, so
 * the copy is guarded and the palette is applied on the render thread.
 */

#define PIEUI_THEME_VERSION    1
#define PIEUI_THEME_MAX_COLORS 96      /* fixed cap; struct stays a flat POD */

struct PieUiTheme {
    uint32_t version;                        /* == PIEUI_THEME_VERSION; ignore if unknown */
    uint32_t accent;                         /* signature highlight; NOT an ImGuiCol */
    uint32_t count;                          /* valid entries in colors[] */
    uint32_t colors[PIEUI_THEME_MAX_COLORS]; /* IM_COL32, indexed by ImGuiCol_ */
};

namespace PieTheme {
    void Init();                       /* subscribe + raise request (needs APIDefs) */
    void Shutdown();                   /* unsubscribe */
    bool HasPalette();                 /* true once a valid palette has arrived */
    void ApplyTo(ImGuiStyle& style);   /* overwrite style.Colors from the copy */
}
