#include "PieTheme.h"
#include "TodoManager.h"
#include "Shared.h"

#include <mutex>
#include <atomic>

static const char* EV_PIEUI_THEME         = "EV_PIEUI_THEME";
static const char* EV_PIEUI_REQUEST_THEME = "EV_PIEUI_REQUEST_THEME";

namespace {
    std::mutex        s_mutex;
    PieUiTheme        s_theme{};
    std::atomic<bool> s_has{false};

    /* Runs on Nexus's event thread (may not be the render thread). Copy the
     * struct immediately — the incoming pointer is only valid during this call. */
    void OnPieTheme(void* aEventArgs) {
        if (!aEventArgs) return;
        const PieUiTheme* incoming = (const PieUiTheme*)aEventArgs;
        if (incoming->version != PIEUI_THEME_VERSION) return; /* version guard */
        std::lock_guard<std::mutex> lk(s_mutex);
        s_theme = *incoming;
        s_has.store(true, std::memory_order_release);
    }
}

namespace PieTheme {

void Init() {
    if (!APIDefs) return;
    APIDefs->Events_Subscribe(EV_PIEUI_THEME, OnPieTheme);
    /* Ask Pie to (re-)broadcast, in case it loaded before us. No-op if absent. */
    APIDefs->Events_Raise(EV_PIEUI_REQUEST_THEME, nullptr);
}

void Shutdown() {
    if (!APIDefs) return;
    APIDefs->Events_Unsubscribe(EV_PIEUI_THEME, OnPieTheme);
}

bool HasPalette() {
    return s_has.load(std::memory_order_acquire);
}

void ApplyTo(ImGuiStyle& style) {
    if (!s_has.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lk(s_mutex);
    int n = (int)s_theme.count;
    if (n > ImGuiCol_COUNT)          n = ImGuiCol_COUNT;
    if (n > PIEUI_THEME_MAX_COLORS)  n = PIEUI_THEME_MAX_COLORS;
    for (int i = 0; i < n; ++i)
        style.Colors[i] = ImGui::ColorConvertU32ToFloat4(s_theme.colors[i]);
}

} // namespace PieTheme
