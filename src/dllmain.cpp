#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstdio>
#include <cstring>

#include "nexus/Nexus.h"
#include "imgui.h"
#include "TodoManager.h"
#include "Shared.h"
#include "ptd_icon.h"
#include "ptd_float_icon.h"

// Version constants
#define V_MAJOR 0
#define V_MINOR 9
#define V_BUILD 1
#define V_REVISION 0

/* ── UI Constants ──────────────────────────────────────────────────────────── */

static const char* WINDOW_NAME        = "Pie's Awesome ToDo List";
static const char* ICON_WINDOW_NAME   = "##PieTodoIcon";
static const char* FLOAT_ICON_TEX_ID  = "PieTodo_float_icon_v2";

static constexpr float ROW_PADDING        = 8.f;
static constexpr float INPUT_WIDTH        = 132.f;
static constexpr float COMBO_WIDTH        = 64.f;
static constexpr float EDIT_FIELD_WIDTH   = 300.f;
static constexpr float WRAP_WIDTH         = 280.f;
static constexpr float DRAG_HANDLE_HEIGHT = 65.f;
static constexpr float RESIZE_GRIP_SIZE   = 28.f;

/* ── GW2-style ImGui theme (copied from tyrian_home_garden) ─────────────────── */

static ImGuiStyle              g_GW2Style;
static std::vector<ImGuiStyle> g_StyleStack;

static void PushGW2Theme() {
    g_StyleStack.push_back(ImGui::GetStyle());
    ImGui::GetStyle() = g_GW2Style;
}

static void PopGW2Theme() {
    if (!g_StyleStack.empty()) {
        ImGui::GetStyle() = g_StyleStack.back();
        g_StyleStack.pop_back();
    }
}

struct ThemeGuard {
    ThemeGuard()  { PushGW2Theme(); }
    ~ThemeGuard() { PopGW2Theme(); }
};

static void BuildGW2Theme() {
    g_GW2Style = ImGui::GetStyle();
    ImGuiStyle& s = g_GW2Style;

    s.WindowRounding    = 6.0f;  s.ChildRounding  = 4.0f;
    s.FrameRounding     = 4.0f;  s.PopupRounding  = 4.0f;
    s.ScrollbarRounding = 6.0f;  s.GrabRounding   = 3.0f;
    s.TabRounding       = 4.0f;

    s.WindowPadding    = ImVec2(10, 10); s.FramePadding     = ImVec2(6, 4);
    s.ItemSpacing      = ImVec2(8, 5);   s.ItemInnerSpacing = ImVec2(6, 4);
    s.ScrollbarSize    = 12.0f;          s.GrabMinSize      = 8.0f;
    s.WindowBorderSize = 1.0f;           s.ChildBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;           s.FrameBorderSize  = 0.0f;
    s.TabBorderSize    = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = ImVec4(0.08f, 0.08f, 0.10f, 0.96f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.07f, 0.07f, 0.09f, 0.80f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.10f, 0.12f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.28f, 0.25f, 0.18f, 0.50f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.13f, 0.11f, 0.80f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.20f, 0.14f, 0.80f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.25f, 0.16f, 0.90f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.09f, 0.07f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.16f, 0.14f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.08f, 0.07f, 0.05f, 0.75f);
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.11f, 0.09f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.06f, 0.07f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.27f, 0.18f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.36f, 0.22f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.44f, 0.26f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.90f, 0.75f, 0.25f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.70f, 0.58f, 0.20f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.85f, 0.70f, 0.25f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.22f, 0.20f, 0.12f, 0.80f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.38f, 0.16f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.18f, 0.16f, 0.10f, 0.70f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.24f, 0.12f, 0.80f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_Separator]            = ImVec4(0.28f, 0.25f, 0.18f, 0.40f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.50f, 0.42f, 0.20f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.65f, 0.55f, 0.25f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.27f, 0.18f, 0.30f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.44f, 0.26f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.65f, 0.55f, 0.25f, 0.90f);
    c[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.13f, 0.10f, 0.86f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.30f, 0.14f, 0.90f);
    c[ImGuiCol_TabActive]            = ImVec4(0.28f, 0.24f, 0.10f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.10f, 0.09f, 0.07f, 0.97f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.18f, 0.16f, 0.10f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.87f, 0.78f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.47f, 0.40f, 1.00f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.70f, 0.58f, 0.20f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.13f, 0.10f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.28f, 0.25f, 0.18f, 0.60f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.22f, 0.20f, 0.15f, 0.40f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.10f, 0.10f, 0.08f, 0.30f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.65f, 0.55f, 0.15f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.80f, 0.68f, 0.20f, 1.00f);
}

/* ── Forward declarations ──────────────────────────────────────────────────── */

static void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
static void RenderTodoWindow();
static void RenderTodoWindowBoring();
static void RenderOptions();
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

/* ── Keybind handler ───────────────────────────────────────────────────────── */

static void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;
    if (strcmp(aIdentifier, KB_TOGGLE) == 0)
        g.pendingToggle.store(true, std::memory_order_release);
}

/* ── Main window rendering ─────────────────────────────────────────────────── */

static void RenderTodoWindow() {
    if (APIDefs && APIDefs->ImguiContext)
        ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);

    double now = ImGui::GetTime();

    /* Poll for external file changes and flush pending saves (always, even when hidden) */
    if (now - g.lastFilePollTime >= FILE_POLL_INTERVAL) {
        g.lastFilePollTime = now;
        if (g.cachedConfigPath.empty())
            g.cachedConfigPath = GetConfigPath("todos.json");
        if (!g.cachedConfigPath.empty()) {
            FILETIME current = GetFileModTime(g.cachedConfigPath);
            if (CompareFileTime(&current, &g.lastFileWriteTime) != 0) {
                LoadTodos();
            }
        }
    }
    FlushIfDirty();

    if (g.pendingToggle.exchange(false, std::memory_order_acquire)) {
        g.windowVisible = !g.windowVisible;
        if (g.windowVisible) {
            g.collapsed = false;
            g.lastHoverTime = now;
        }
    }
    if (!g.windowVisible) return;

    /* Periodic reset check */
    if (now - g.lastResetCheckTime >= RESET_CHECK_INTERVAL) {
        g.lastResetCheckTime = now;
        CheckResetTimes();
    }

    /* Rebuild cache if data or filter changed */
    if (g.cacheDirty || g.searchFilter != g.cachedSearchFilter || g.completedMode != g.cachedCompletedMode)
        RebuildCache();

    /* Collapsed icon mode (shared by Fancy and Boring) */
    if (g.collapseEnabled && g.collapsed) {
        const float sz = g.floatIconSize;
        float iconX = (g.displayMode == DisplayMode_Boring) ? g.boringX : g.winX;
        float iconY = (g.displayMode == DisplayMode_Boring) ? g.boringY : g.winY;
        ImGui::SetNextWindowPos(ImVec2(iconX, iconY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sz, sz));
        ImGuiWindowFlags iconFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (ImGui::Begin(ICON_WINDOW_NAME, nullptr, iconFlags)) {
            Texture_t* tex = APIDefs->Textures_Get(FLOAT_ICON_TEX_ID);
            if (tex && tex->Resource) {
                ImVec2 wp = ImGui::GetWindowPos();
                ImDrawList* dl  = ImGui::GetWindowDrawList();
                ImDrawList* fdl = ImGui::GetForegroundDrawList();
                ImVec2 p0(wp.x, wp.y);
                ImVec2 p1(wp.x + sz, wp.y + sz);
                dl->AddImage((ImTextureID)tex->Resource, p0, p1);

                /* Completion text centred on icon, scaled with icon size */
                char dBuf[16], wBuf[16];
                snprintf(dBuf, sizeof(dBuf), "D:%d/%d", g.cachedDailyDone, g.cachedDailyTotal);
                snprintf(wBuf, sizeof(wBuf), "W:%d/%d", g.cachedWeeklyDone, g.cachedWeeklyTotal);
                ImFont* font     = ImGui::GetFont();
                float   fontSize = sz * 0.15f;
                float   gap      = fontSize * 0.2f;
                ImVec2  dSz      = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, dBuf);
                ImVec2  wSz      = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, wBuf);
                float   totalH   = dSz.y + gap + wSz.y;
                float   centreX  = p0.x + sz * 0.5f;
                float   startY   = p0.y + (sz - totalH) * 0.5f;
                ImU32   textCol  = IM_COL32(40, 20, 10, 220);
                fdl->AddText(font, fontSize, ImVec2(centreX - dSz.x * 0.5f, startY),              textCol, dBuf);
                fdl->AddText(font, fontSize, ImVec2(centreX - wSz.x * 0.5f, startY + dSz.y + gap), textCol, wBuf);
            }

            if (ImGui::IsWindowHovered()) {
                if (!g.expandOnClick || ImGui::IsMouseClicked(0)) {
                    g.collapsed = false;
                    g.lastHoverTime = now;
                }
            }
        }
        ImGui::End();
        return;
    }

    /* Boring mode — entirely separate render path */
    if (g.displayMode == DisplayMode_Boring) { RenderTodoWindowBoring(); return; }

    /* Window setup */
    ImGui::SetNextWindowPos(ImVec2(g.winX, g.winY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(g.winW, g.winH), ImGuiCond_Always);
    ImGuiWindowFlags wflags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
                            | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 4.f));
    if (!ImGui::Begin(WINDOW_NAME, nullptr, wflags)) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }
    ImGui::PopStyleVar();

    /* ── Sepia colour theme ─────────────────────────────────────────────────── */
    ImGui::PushStyleColor(ImGuiCol_Text,                 IM_COL32(40,  20,  10,  220));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,              IM_COL32(210, 180, 140, 120));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,       IM_COL32(210, 180, 140, 180));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,        IM_COL32(180, 150, 110, 200));
    ImGui::PushStyleColor(ImGuiCol_CheckMark,            IM_COL32(80,  40,  10,  255));
    ImGui::PushStyleColor(ImGuiCol_Button,               IM_COL32(180, 150, 110, 160));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,        IM_COL32(180, 150, 110, 220));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,         IM_COL32(140, 110, 80,  255));
    ImGui::PushStyleColor(ImGuiCol_Header,               IM_COL32(180, 150, 110, 80));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,        IM_COL32(180, 150, 110, 140));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,         IM_COL32(140, 110, 80,  180));
    ImGui::PushStyleColor(ImGuiCol_Separator,            IM_COL32(120, 80,  40,  140));
    ImGui::PushStyleColor(ImGuiCol_PopupBg,              IM_COL32(240, 220, 180, 240));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,              IM_COL32(0,   0,   0,   0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,          IM_COL32(0,   0,   0,   0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,        IM_COL32(120, 80,  40,  140));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(120, 80,  40,  220));
    static constexpr int SEPIA_COLOUR_COUNT = 17;

    ImGui::SetWindowFontScale((13.0f / ImGui::GetFontSize()) * (ImGui::GetWindowWidth() / DEFAULT_WINDOW_W));

    /* ── Background image ───────────────────────────────────────────────────── */
    {
        ImVec2 wp  = ImGui::GetWindowPos();
        ImVec2 wsz = ImGui::GetWindowSize();
        Texture_t* bg = APIDefs->Textures_Get(FLOAT_ICON_TEX_ID);
        if (bg && bg->Resource) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(wp, ImVec2(wp.x + wsz.x, wp.y + wsz.y), false);
            dl->AddImage((ImTextureID)bg->Resource, wp, ImVec2(wp.x + wsz.x, wp.y + wsz.y));
            dl->PopClipRect();
        }
    }

    /* ── Whole-window drag (excludes interactive regions) ───────────────────── */
    {
        static bool s_winDragging = false;
        ImVec2 wp    = ImGui::GetWindowPos();
        float  ww    = ImGui::GetWindowWidth();
        float  wh    = ImGui::GetWindowHeight();
        float  pad   = 4.f;
        ImVec2 mouse = ImGui::GetIO().MousePos;

        bool overWindow = mouse.x >= wp.x && mouse.x < wp.x + ww
                       && mouse.y >= wp.y && mouse.y < wp.y + wh;

        /* Exclude interactive regions */
        auto inRect = [&](float x0, float y0, float x1, float y1) {
            return mouse.x >= x0 && mouse.x < x1 && mouse.y >= y0 && mouse.y < y1;
        };
        bool overInteractive =
            inRect(wp.x + pad + g.posSearchX, wp.y + pad + g.posSearchY,
                   wp.x + pad + g.posSearchX + g.posSearchW + 35.f, wp.y + pad + g.posSearchY + 26.f) ||
            inRect(wp.x + pad + g.posTaskX,   wp.y + pad + g.posTaskY,
                   wp.x + pad + g.posTaskX + g.posTaskW, wp.y + pad + g.posTaskBot) ||
            inRect(wp.x + pad + g.posAddX,    wp.y + pad + g.posAddY,
                   wp.x + pad + g.posAddX + 200.f, wp.y + pad + g.posAddY + 26.f);

        if (overWindow && !overInteractive && ImGui::GetIO().MouseClicked[0] && !g.lockPosition)
            s_winDragging = true;
        if (!ImGui::GetIO().MouseDown[0])
            s_winDragging = false;
        if (s_winDragging && !g.lockPosition) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            g.winX += delta.x;
            g.winY += delta.y;
            ImGui::SetWindowPos(ImVec2(g.winX, g.winY));
            MarkDirty();
        }
        if (overWindow && !overInteractive && !g.lockPosition)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    /* ── Search bar ─────────────────────────────────────────────────────────── */
    {
        char searchBuf[256];
        strncpy(searchBuf, g.searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';
        ImGui::SetCursorPos(ImVec2(g.posSearchX, g.posSearchY));
        ImGui::SetNextItemWidth(g.posSearchW);
        if (ImGui::InputTextWithHint("##search", "Search...", searchBuf, sizeof(searchBuf)))
            g.searchFilter = searchBuf;
        ImGui::SameLine();
        if (ImGui::Button("X")) g.searchFilter.clear();
    }

    /* Use cached visible indices (rebuilt only on data/filter change) */
    const std::vector<int>& visibleIndices = g.cachedVisibleIndices;

    /* ── Task list ──────────────────────────────────────────────────────────── */
    {
        float taskW = g.posTaskW;
        float taskH = g.posTaskBot - g.posTaskY;
        ImGui::SetCursorPos(ImVec2(g.posTaskX, g.posTaskY));
        ImGui::SetNextWindowBgAlpha(0.f);
        if (ImGui::BeginChild("TaskList", ImVec2(taskW, taskH), false)) {
        const float listWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 winPos = ImGui::GetWindowPos();
        float winWidth = ImGui::GetWindowWidth();
        /* Reuse persistent row rect vectors instead of allocating every frame */
        g.rowMin.resize(visibleIndices.size());
        g.rowMax.resize(visibleIndices.size());
        const float repeatColWidth = ImGui::CalcTextSize("W").x + ImGui::GetStyle().ItemSpacing.x * 2.f;

        for (size_t vi = 0; vi < visibleIndices.size(); vi++) {
            int idx = visibleIndices[vi];
            TodoItem& item = g.todos[idx];
            ImGui::PushID(item.uid.c_str());

            /* Record row start Y for background rect */
            ImVec2 rowStartPos = ImGui::GetCursorScreenPos();

            /* Alternating row background (zebra stripe on odd rows) */
            if (vi % 2 == 1) {
                ImVec2 zMin = ImVec2(winPos.x, rowStartPos.y);
                ImVec2 zMax = ImVec2(winPos.x + winWidth, rowStartPos.y + ImGui::GetFrameHeightWithSpacing());
                ImGui::GetWindowDrawList()->AddRectFilled(zMin, zMax, IM_COL32(80, 40, 10, 20));
            }

            /* Full-row drag source (invisible selectable spanning available width) */
            ImGui::Selectable("##dragrow", false, ImGuiSelectableFlags_AllowItemOverlap);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                g.dragSourceIdx = idx;
                ImGui::SetDragDropPayload("PIE_TODO_ROW", &idx, sizeof(int));
                ImGui::TextUnformatted("Move task");
                ImGui::EndDragDropSource();
            }
            ImGui::SameLine(0, 0);

            /* Checkbox */
            bool completed = item.completed;
            if (ImGui::Checkbox("##done", &completed)) {
                item.completed = completed;
                MarkDirty();
            }
            ImGui::SameLine(0, ROW_PADDING);

            /* Task text */
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(item.text.c_str());

            /* Repeat label right-aligned */
            ImGui::SameLine(winWidth - repeatColWidth);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(item.repeat == Repeat_Weekly ? "W" : "D");

            /* Draw row background highlight for completed tasks (full row height) */
            if (completed) {
                ImVec2 rMin = ImVec2(winPos.x, rowStartPos.y);
                ImVec2 rMax = ImVec2(winPos.x + winWidth, rowStartPos.y + ImGui::GetFrameHeightWithSpacing());
                ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(80, 40, 10, 50));
            }

            /* Store full row rect for drag-drop and right-click hit test */
            ImVec2 rEnd = ImGui::GetItemRectMax();
            g.rowMin[vi] = ImVec2(winPos.x, rowStartPos.y);
            g.rowMax[vi] = ImVec2(winPos.x + listWidth, rEnd.y);

            ImGui::PopID();
        }

        /* Drop target indicator and drop handling */
        if (g.dragSourceIdx >= 0) {
            ImVec2 mouse = ImGui::GetMousePos();
            int dropVisIdx = -1;
            bool dropAfter = false;

            /* Find which row the cursor is over and whether it's in the top or bottom half */
            for (size_t vi = 0; vi < visibleIndices.size(); vi++) {
                if (mouse.y >= g.rowMin[vi].y && mouse.y < g.rowMax[vi].y) {
                    float midY = (g.rowMin[vi].y + g.rowMax[vi].y) * 0.5f;
                    dropVisIdx = (int)vi;
                    dropAfter = (mouse.y >= midY);
                    break;
                }
            }
            /* If cursor is below all rows, drop after the last row */
            if (dropVisIdx < 0 && !visibleIndices.empty() && mouse.y >= g.rowMax.back().y) {
                dropVisIdx = (int)visibleIndices.size() - 1;
                dropAfter = true;
            }

            /* Draw a very obvious drop indicator: thick line + arrow markers */
            if (dropVisIdx >= 0) {
                float lineY = dropAfter ? g.rowMax[dropVisIdx].y : g.rowMin[dropVisIdx].y;
                float x1 = winPos.x + ROW_PADDING;
                float x2 = winPos.x + winWidth - ROW_PADDING;
                ImU32 lineCol = IM_COL32(255, 200, 0, 255);
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                fg->AddLine(ImVec2(x1, lineY), ImVec2(x2, lineY), lineCol, 3.0f);
                /* Left arrow */
                fg->AddTriangleFilled(
                    ImVec2(x1, lineY),
                    ImVec2(x1 - 6.f, lineY - 5.f),
                    ImVec2(x1 - 6.f, lineY + 5.f), lineCol);
                /* Right arrow */
                fg->AddTriangleFilled(
                    ImVec2(x2, lineY),
                    ImVec2(x2 + 6.f, lineY - 5.f),
                    ImVec2(x2 + 6.f, lineY + 5.f), lineCol);
            }

            /* Perform the move on mouse release */
            if (ImGui::IsMouseReleased(0)) {
                if (dropVisIdx >= 0) {
                    int targetIdx = visibleIndices[dropVisIdx];
                    if (dropAfter && targetIdx < (int)g.todos.size() - 1)
                        targetIdx++;
                    if (g.dragSourceIdx != targetIdx)
                        MoveTodo(g.dragSourceIdx, targetIdx);
                }
                g.dragSourceIdx = -1;
            }
        }

        g.rowVisibleIndices = visibleIndices;
        }
        ImGui::EndChild();
    }

    /* Right-click on a row opens context menu (after EndChild so popup isn't clipped) */
    if (ImGui::IsMouseClicked(1) && !g.rowMin.empty()) {
        ImVec2 mouse = ImGui::GetMousePos();
        for (size_t vi = 0; vi < g.rowMin.size(); vi++) {
            if (mouse.x >= g.rowMin[vi].x && mouse.x < g.rowMax[vi].x &&
                mouse.y >= g.rowMin[vi].y && mouse.y < g.rowMax[vi].y) {
                g.contextMenuUid = g.todos[g.rowVisibleIndices[vi]].uid;
                ImGui::SetNextWindowPos(mouse);
                ImGui::OpenPopup("TaskContextMenu");
                break;
            }
        }
    }

    if (ImGui::BeginPopup("TaskContextMenu")) {
        if (ImGui::Selectable("Edit")) {
            g.editingUid = g.contextMenuUid;
            int i = IndexForUid(g.contextMenuUid);
            if (i >= 0) {
                g.editText   = g.todos[i].text;
                g.editRepeat = g.todos[i].repeat;
            }
            g.editPopupPending = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Delete")) {
            g.deleteConfirmUid   = g.contextMenuUid;
            g.deletePopupPending = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    /* Edit popup */
    if (g.editPopupPending) {
        ImGui::OpenPopup("Edit Todo");
        g.editPopupPending = false;
    }
    if (ImGui::BeginPopupModal("Edit Todo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        char editBuf[512];
        strncpy(editBuf, g.editText.c_str(), sizeof(editBuf) - 1);
        editBuf[sizeof(editBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(EDIT_FIELD_WIDTH);
        bool enter = ImGui::InputText("Task", editBuf, sizeof(editBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        g.editText = editBuf;
        const char* editRepeatLabels[] = { "Daily", "Weekly" };
        ImGui::Combo("Repeat", (int*)&g.editRepeat, editRepeatLabels, 2);
        if (ImGui::Button("OK") || enter) {
            int i = IndexForUid(g.editingUid);
            if (i >= 0) {
                std::string trimmed = TrimWhitespace(g.editText);
                if (!trimmed.empty())
                    g.todos[i].text = std::move(trimmed);
                g.todos[i].repeat = g.editRepeat;
                MarkDirty();
            }
            g.editingUid.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            g.editingUid.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    /* Delete confirmation popup */
    if (g.deletePopupPending) {
        ImGui::OpenPopup("Delete Todo");
        g.deletePopupPending = false;
    }
    if (ImGui::BeginPopupModal("Delete Todo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int idx = IndexForUid(g.deleteConfirmUid);
        if (idx >= 0) {
            ImGui::Text("Delete this task?");
            ImGui::Text("Task: ");
            ImGui::SameLine();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + WRAP_WIDTH);
            ImGui::TextUnformatted(g.todos[idx].text.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button("Yes")) {
                g.todos.erase(g.todos.begin() + idx);
                MarkDirty();
                g.deleteConfirmUid.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No")) {
                g.deleteConfirmUid.clear();
                ImGui::CloseCurrentPopup();
            }
        } else {
            g.deleteConfirmUid.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    /* ── Add task row ───────────────────────────────────────────────────────── */
    {
        ImGui::SetCursorPos(ImVec2(g.posAddX, g.posAddY));
        char newBuf[512];
        strncpy(newBuf, g.newTaskText.c_str(), sizeof(newBuf) - 1);
        newBuf[sizeof(newBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(INPUT_WIDTH);
        if (ImGui::InputTextWithHint("##newtask", "New task...", newBuf, sizeof(newBuf),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
            g.newTaskText = newBuf;
            AddNewTodo();
        } else {
            g.newTaskText = newBuf;
        }
        ImGui::SameLine();
        const char* repeatLabels[] = { "Daily", "Weekly" };
        ImGui::SetNextItemWidth(COMBO_WIDTH);
        ImGui::Combo("##repeat", (int*)&g.newTaskRepeat, repeatLabels, 2);
        ImGui::SameLine();
        if (ImGui::Button("+")) AddNewTodo();
    }


    /* Collapse timer logic */
    if (g.collapseEnabled) {
        bool anyHovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_ChildWindows |
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
            ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        bool popupOpen = ImGui::IsPopupOpen("TaskContextMenu")
            || ImGui::IsPopupOpen("Edit Todo")
            || ImGui::IsPopupOpen("Delete Todo");
        if (anyHovered || popupOpen)
            g.lastHoverTime = now;
        else if (g.lastHoverTime > 0.0 && (now - g.lastHoverTime) >= (double)g.collapseDelaySec)
            g.collapsed = true;
    }

    /* W/H are always set via SetNextWindowSize(Always) so they stay in sync.
       X/Y are managed by SetNextWindowPos(Always) + drag handler above. */

    /* ── Layout edit mode overlays ──────────────────────────────────────────── */
    if (g.layoutEditMode) {
        static int  s_dragHandle = -1;
        static bool s_dragResize = false;
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        ImVec2 wpos = ImGui::GetWindowPos();
        float  winW = ImGui::GetWindowWidth();
        float  winH = ImGui::GetWindowHeight();
        float  pad  = 4.f;
        const float RC = 10.f; /* resize corner hit size */

        /* Window boundary */
        fdl->AddRect(wpos, ImVec2(wpos.x + winW, wpos.y + winH),
                     IM_COL32(255, 80, 80, 200), 0.f, 0, 2.f);

        struct Handle {
            const char* label;
            ImU32       col;
            float       sx, sy, sw, sh;
            float*      px;
            float*      py;
            float*      pw;
            float*      ph;
        } handles[] = {
            { "DRAG",   IM_COL32(60, 120, 255, 120),
              wpos.x,             wpos.y + pad + g.posDragY,     winW,              g.posDragH,
              nullptr,            &g.posDragY,   nullptr,          &g.posDragH },
            { "SEARCH", IM_COL32(50, 200, 80,  120),
              wpos.x + pad + g.posSearchX, wpos.y + pad + g.posSearchY, g.posSearchW + 30.f, 22.f,
              &g.posSearchX,      &g.posSearchY, &g.posSearchW,    nullptr },
            { "TASKS",  IM_COL32(220, 160, 40, 100),
              wpos.x + pad + g.posTaskX,   wpos.y + pad + g.posTaskY,   g.posTaskW, g.posTaskBot - g.posTaskY,
              &g.posTaskX,        &g.posTaskY,   &g.posTaskW,      &g.posTaskBot },
            { "ADD",    IM_COL32(180, 60, 220, 120),
              wpos.x + pad + g.posAddX,    wpos.y + pad + g.posAddY,    160.f, 22.f,
              &g.posAddX,         &g.posAddY,    nullptr,          nullptr },
        };

        ImVec2 mouse = ImGui::GetIO().MousePos;
        for (int i = 0; i < 4; i++) {
            Handle& h = handles[i];
            bool over = mouse.x >= h.sx && mouse.x < h.sx + h.sw
                     && mouse.y >= h.sy && mouse.y < h.sy + h.sh;
            if (over && ImGui::GetIO().MouseClicked[0] && s_dragHandle < 0) {
                s_dragHandle = i;
                bool inCorner = (h.pw || h.ph)
                             && mouse.x >= h.sx + h.sw - RC
                             && mouse.y >= h.sy + h.sh - RC;
                s_dragResize = inCorner;
            }
            if (s_dragHandle == i) {
                if (ImGui::GetIO().MouseDown[0]) {
                    ImVec2 d = ImGui::GetIO().MouseDelta;
                    if (s_dragResize) {
                        if (h.pw) *h.pw += d.x;
                        if (h.ph) *h.ph += d.y;
                    } else {
                        if (h.px) *h.px += d.x;
                        if (h.py) *h.py += d.y;
                        auto fclamp = [](float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; };
                        if (h.px) *h.px = fclamp(*h.px, 0.f, winW - 4.f);
                        if (h.py) *h.py = fclamp(*h.py, 0.f, winH - 4.f);
                    }
                    MarkDirty();
                } else {
                    s_dragHandle = -1;
                }
            }
            bool active = (s_dragHandle == i);
            ImU32 fill = active ? IM_COL32(255,255,255,60) : h.col;
            fdl->AddRectFilled(ImVec2(h.sx, h.sy), ImVec2(h.sx+h.sw, h.sy+h.sh), fill, 3.f);
            fdl->AddRect(ImVec2(h.sx, h.sy), ImVec2(h.sx+h.sw, h.sy+h.sh),
                         IM_COL32(255,255,255,200), 3.f, 0, 1.5f);
            fdl->AddText(ImVec2(h.sx+4.f, h.sy+4.f), IM_COL32(255,255,255,255), h.label);
            if (h.pw || h.ph) {
                ImU32 cornerCol = (active && s_dragResize)
                    ? IM_COL32(255, 255, 100, 240)
                    : IM_COL32(255, 255, 100, 160);
                fdl->AddRectFilled(
                    ImVec2(h.sx + h.sw - RC, h.sy + h.sh - RC),
                    ImVec2(h.sx + h.sw,      h.sy + h.sh),
                    cornerCol, 2.f);
            }
        }
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor(SEPIA_COLOUR_COUNT);
    ImGui::End();
}

/* ── Boring mode window ─────────────────────────────────────────────────────── */

static void RenderTodoWindowBoring() {
    if (APIDefs && APIDefs->ImguiContext)
        ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);

    ThemeGuard themeGuard;

    ImGui::SetNextWindowSizeConstraints(ImVec2(300.f, 200.f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::SetNextWindowPos(ImVec2(g.boringX, g.boringY), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(g.boringW, g.boringH), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Pie's Awesome ToDo List", &g.windowVisible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    /* Track geometry so it's saved on unload */
    g.boringX = ImGui::GetWindowPos().x;
    g.boringY = ImGui::GetWindowPos().y;
    g.boringW = ImGui::GetWindowWidth();
    g.boringH = ImGui::GetWindowHeight();

    /* ── Add row (top) ─────────────────────────────────────────────────────── */
    {
        char newBuf[512];
        strncpy(newBuf, g.newTaskText.c_str(), sizeof(newBuf) - 1);
        newBuf[sizeof(newBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(INPUT_WIDTH);
        if (ImGui::InputTextWithHint("##newtask", "New task...", newBuf, sizeof(newBuf),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
            g.newTaskText = newBuf;
            AddNewTodo();
        } else {
            g.newTaskText = newBuf;
        }
        ImGui::SameLine();
        const char* repeatLabels[] = { "Daily", "Weekly" };
        ImGui::SetNextItemWidth(COMBO_WIDTH);
        ImGui::Combo("##repeat", (int*)&g.newTaskRepeat, repeatLabels, 2);
        ImGui::SameLine();
        if (ImGui::Button("Add")) AddNewTodo();
    }
    ImGui::Separator();

    /* ── Search ────────────────────────────────────────────────────────────── */
    {
        char searchBuf[256];
        strncpy(searchBuf, g.searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(-40.f);
        if (ImGui::InputTextWithHint("##search", "Search tasks...", searchBuf, sizeof(searchBuf)))
            g.searchFilter = searchBuf;
        ImGui::SameLine();
        if (ImGui::Button("X")) { g.searchFilter.clear(); InvalidateCache(); }
    }
    ImGui::Separator();

    /* ── Task list ─────────────────────────────────────────────────────────── */
    const std::vector<int>& visibleIndices = g.cachedVisibleIndices;
    float repeatColWidth = ImGui::CalcTextSize("Weekly").x + ImGui::GetStyle().ItemSpacing.x * 2.f;
    float footerH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

    g.rowMin.resize(visibleIndices.size());
    g.rowMax.resize(visibleIndices.size());

    if (ImGui::BeginChild("##borelist", ImVec2(0.f, -footerH), false)) {
        ImVec2 winPos  = ImGui::GetWindowPos();
        float  winW    = ImGui::GetWindowWidth();

        for (size_t vi = 0; vi < visibleIndices.size(); vi++) {
            int idx = visibleIndices[vi];
            TodoItem& item = g.todos[idx];
            ImGui::PushID(item.uid.c_str());

            ImVec2 rowPos = ImGui::GetCursorScreenPos();

            /* Full-row selectable (drag-drop source) */
            ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowItemOverlap,
                              ImVec2(ImGui::GetContentRegionAvail().x, 0.f));
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                g.dragSourceIdx = idx;
                ImGui::SetDragDropPayload("PIE_TODO_ROW", &idx, sizeof(int));
                ImGui::TextUnformatted("Move task");
                ImGui::EndDragDropSource();
            }
            ImGui::SameLine(0, 0);
            ImGui::SetCursorScreenPos(rowPos);

            /* Checkbox */
            bool completed = item.completed;
            if (ImGui::Checkbox("##done", &completed)) {
                item.completed = completed;
                MarkDirty();
                InvalidateCache();
            }
            ImGui::SameLine(0, ROW_PADDING);

            /* Task text (green if completed and colour mode) */
            ImGui::AlignTextToFramePadding();
            if (completed && g.completedMode == CompletedMode_Colour)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.82f, 0.35f, 1.0f));
            ImGui::TextUnformatted(item.text.c_str());
            if (completed && g.completedMode == CompletedMode_Colour)
                ImGui::PopStyleColor();

            /* Repeat label right-aligned */
            ImGui::SameLine(winW - repeatColWidth - ImGui::GetStyle().WindowPadding.x);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(item.repeat == Repeat_Weekly ? "Weekly" : "Daily");

            /* Row background highlight for completed tasks */
            if (completed && g.completedMode == CompletedMode_Colour) {
                ImVec2 rMin(winPos.x, rowPos.y);
                ImVec2 rMax(winPos.x + winW, rowPos.y + ImGui::GetFrameHeightWithSpacing());
                ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(35, 82, 35, 40));
            }

            /* Store row rects for context menu hit test */
            ImVec2 rEnd = ImGui::GetItemRectMax();
            g.rowMin[vi] = ImVec2(winPos.x, rowPos.y);
            g.rowMax[vi] = ImVec2(winPos.x + winW, rEnd.y);

            ImGui::PopID();
        }

        /* Drop target and reorder (same logic as fancy mode) */
        if (g.dragSourceIdx >= 0) {
            ImVec2 mouse = ImGui::GetMousePos();
            int dropVisIdx = -1; bool dropAfter = false;
            for (size_t vi = 0; vi < visibleIndices.size(); vi++) {
                if (mouse.y >= g.rowMin[vi].y && mouse.y < g.rowMax[vi].y) {
                    dropVisIdx = (int)vi;
                    dropAfter  = mouse.y >= (g.rowMin[vi].y + g.rowMax[vi].y) * 0.5f;
                    break;
                }
            }
            if (dropVisIdx < 0 && !visibleIndices.empty() && mouse.y >= g.rowMax.back().y)
                { dropVisIdx = (int)visibleIndices.size() - 1; dropAfter = true; }
            if (dropVisIdx >= 0) {
                float lineY = dropAfter ? g.rowMax[dropVisIdx].y : g.rowMin[dropVisIdx].y;
                float x1 = ImGui::GetWindowPos().x + ROW_PADDING;
                float x2 = ImGui::GetWindowPos().x + winW - ROW_PADDING;
                ImU32 lc = IM_COL32(255, 200, 0, 255);
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                fg->AddLine(ImVec2(x1, lineY), ImVec2(x2, lineY), lc, 3.f);
                fg->AddTriangleFilled(ImVec2(x1,lineY),ImVec2(x1-6.f,lineY-5.f),ImVec2(x1-6.f,lineY+5.f),lc);
                fg->AddTriangleFilled(ImVec2(x2,lineY),ImVec2(x2+6.f,lineY-5.f),ImVec2(x2+6.f,lineY+5.f),lc);
            }
            if (ImGui::IsMouseReleased(0)) {
                if (dropVisIdx >= 0) {
                    int targetIdx = visibleIndices[dropVisIdx];
                    if (dropAfter && targetIdx < (int)g.todos.size() - 1) targetIdx++;
                    if (g.dragSourceIdx != targetIdx) MoveTodo(g.dragSourceIdx, targetIdx);
                }
                g.dragSourceIdx = -1;
            }
        }
        g.rowVisibleIndices = visibleIndices;
    }
    ImGui::EndChild();

    /* Right-click context menu */
    if (ImGui::IsMouseClicked(1) && !g.rowMin.empty()) {
        ImVec2 mouse = ImGui::GetMousePos();
        for (size_t vi = 0; vi < g.rowMin.size(); vi++) {
            if (mouse.x >= g.rowMin[vi].x && mouse.x < g.rowMax[vi].x &&
                mouse.y >= g.rowMin[vi].y && mouse.y < g.rowMax[vi].y) {
                g.contextMenuUid = g.todos[g.rowVisibleIndices[vi]].uid;
                ImGui::SetNextWindowPos(mouse);
                ImGui::OpenPopup("TaskContextMenu");
                break;
            }
        }
    }
    if (ImGui::BeginPopup("TaskContextMenu")) {
        if (ImGui::Selectable("Edit")) {
            g.editingUid = g.contextMenuUid;
            int i = IndexForUid(g.contextMenuUid);
            if (i >= 0) { g.editText = g.todos[i].text; g.editRepeat = g.todos[i].repeat; }
            g.editPopupPending = true; ImGui::CloseCurrentPopup();
        }
        if (ImGui::Selectable("Delete")) {
            g.deleteConfirmUid = g.contextMenuUid;
            g.deletePopupPending = true; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    /* Edit popup */
    if (g.editPopupPending) { ImGui::OpenPopup("Edit Todo"); g.editPopupPending = false; }
    if (ImGui::BeginPopupModal("Edit Todo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        char editBuf[512];
        strncpy(editBuf, g.editText.c_str(), sizeof(editBuf) - 1); editBuf[sizeof(editBuf)-1] = '\0';
        ImGui::SetNextItemWidth(EDIT_FIELD_WIDTH);
        bool enter = ImGui::InputText("Task", editBuf, sizeof(editBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        g.editText = editBuf;
        const char* editRepeatLabels[] = { "Daily", "Weekly" };
        ImGui::Combo("Repeat", (int*)&g.editRepeat, editRepeatLabels, 2);
        if (ImGui::Button("OK") || enter) {
            int i = IndexForUid(g.editingUid);
            if (i >= 0) {
                std::string trimmed = TrimWhitespace(g.editText);
                if (!trimmed.empty()) g.todos[i].text = std::move(trimmed);
                g.todos[i].repeat = g.editRepeat; MarkDirty();
            }
            g.editingUid.clear(); ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { g.editingUid.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    /* Delete confirmation popup */
    if (g.deletePopupPending) { ImGui::OpenPopup("Delete Todo"); g.deletePopupPending = false; }
    if (ImGui::BeginPopupModal("Delete Todo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int idx = IndexForUid(g.deleteConfirmUid);
        if (idx >= 0) {
            ImGui::Text("Delete this task?");
            ImGui::Text("Task: "); ImGui::SameLine();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + WRAP_WIDTH);
            ImGui::TextUnformatted(g.todos[idx].text.c_str()); ImGui::PopTextWrapPos();
            if (ImGui::Button("Yes")) {
                g.todos.erase(g.todos.begin() + idx);
                MarkDirty(); g.deleteConfirmUid.clear(); ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No")) { g.deleteConfirmUid.clear(); ImGui::CloseCurrentPopup(); }
        } else { g.deleteConfirmUid.clear(); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    /* ── Footer ────────────────────────────────────────────────────────────── */
    ImGui::Separator();
    int done = 0;
    for (int idx : visibleIndices) if (g.todos[idx].completed) done++;
    ImGui::Text("%d/%d completed", done, (int)visibleIndices.size());

    /* Collapse timer */
    if (g.collapseEnabled) {
        bool anyHovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_ChildWindows |
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
            ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        bool popupOpen = ImGui::IsPopupOpen("TaskContextMenu")
            || ImGui::IsPopupOpen("Edit Todo")
            || ImGui::IsPopupOpen("Delete Todo");
        double now = ImGui::GetTime();
        if (anyHovered || popupOpen)
            g.lastHoverTime = now;
        else if (g.lastHoverTime > 0.0 && (now - g.lastHoverTime) >= (double)g.collapseDelaySec)
            g.collapsed = true;
    }

    ImGui::End();
}

/* ── Options panel (Nexus addon settings) ──────────────────────────────────── */

static void RenderOptions() {
    if (APIDefs && APIDefs->ImguiContext)
        ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ThemeGuard themeGuard;
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "Pie's Awesome ToDo List");
    if (ImGui::SmallButton("Homepage")) {
        ShellExecuteA(NULL, "open", "https://pie.rocks.cc/", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Buy me a coffee!")) {
        ShellExecuteA(NULL, "open", "https://ko-fi.com/pieorcake", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::Separator();
    ImGui::Text("Completed tasks:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Colour", g.completedMode == CompletedMode_Colour)) {
        g.completedMode = CompletedMode_Colour;
        MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Hide", g.completedMode == CompletedMode_Hide)) {
        g.completedMode = CompletedMode_Hide;
        MarkDirty();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show completed tasks with a green row (Colour) or hide them (Hide).");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Display style:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Fancy", g.displayMode == DisplayMode_Fancy)) {
        g.displayMode = DisplayMode_Fancy; MarkDirty();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Boring", g.displayMode == DisplayMode_Boring)) {
        g.displayMode = DisplayMode_Boring; MarkDirty();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Fancy: scroll artwork, fixed size.\nBoring: plain resizable window.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Lock window position", &g.lockPosition))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Prevent the window from being dragged.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Show Quick Access icon", &g.showQuickAccess)) {
        if (g.showQuickAccess)
            APIDefs->QuickAccess_Add(QA_ID, QA_ICON_ID, QA_ICON_HOV_ID, KB_TOGGLE, "ToDo List");
        else
            APIDefs->QuickAccess_Remove(QA_ID);
        MarkDirty();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show or hide the shortcut icon in the Quick Access bar.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Open on launch", &g.openOnLaunch))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically show the window when the game starts.");

    /* Layout edit mode — disabled; layout is hard-coded. Preserved for future reference.
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Layout edit mode", &g.layoutEditMode)) MarkDirty();
    ImGui::SameLine(); ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag the coloured overlays on the window to reposition each element. Yellow corner = resize.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset layout")) {
        g.posDragY=6.f; g.posDragH=29.f;
        g.posSearchX=90.f; g.posSearchY=40.f; g.posSearchW=189.f;
        g.posTaskX=53.f; g.posTaskW=258.f; g.posTaskY=97.f; g.posTaskBot=320.f;
        g.posAddX=83.f; g.posAddY=355.f;
        g.winW=DEFAULT_WINDOW_W; g.winH=DEFAULT_WINDOW_H; MarkDirty();
    }
    if (g.layoutEditMode) {
        // Win X/Y/W/H sliders, DH/SR/TL/AR sliders ...
    }
    */

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Collapse to icon", &g.collapseEnabled)) {
        if (g.collapseEnabled) {
            g.windowVisible = true;
            g.collapsed = true;
        } else {
            g.collapsed = false;
        }
        MarkDirty();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The window collapses to an icon after the mouse leaves.\nHover or click the icon to expand.");
    }
    if (g.collapseEnabled) {
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::InputFloat("Delay (seconds)", &g.collapseDelaySec, 0.5f, 1.0f, "%.1f")) {
            if (g.collapseDelaySec < 0.5f) g.collapseDelaySec = 0.5f;
            if (g.collapseDelaySec > 30.0f) g.collapseDelaySec = 30.0f;
            MarkDirty();
        }
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::SliderFloat("Icon size", &g.floatIconSize, 32.f, 128.f, "%.0f")) {
            g.floatIconSize = std::max(32.f, std::min(g.floatIconSize, 128.f));
            MarkDirty();
        }
        ImGui::Text("Expand icon on:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Hover", !g.expandOnClick)) {
            g.expandOnClick = false;
            MarkDirty();
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Click", g.expandOnClick)) {
            g.expandOnClick = true;
            MarkDirty();
        }
    }
}

/* ── Addon lifecycle ───────────────────────────────────────────────────────── */

void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    BuildGW2Theme();
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree);

    APIDefs->Log(LOGL_INFO, ADDON_NAME, "Loading addon...");

    LoadTodos();
    LoadSettings();

    if (g.openOnLaunch) {
        g.windowVisible = true;
        if (g.collapseEnabled)
            g.collapsed = true;
    }

    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE, ProcessKeybind, "CTRL+SHIFT+T");
    APIDefs->GUI_Register(RT_Render, RenderTodoWindow);
    APIDefs->GUI_Register(RT_OptionsRender, RenderOptions);

    /* Load icons from embedded data and register Quick Access shortcut */
    APIDefs->Textures_LoadFromMemory(QA_ICON_ID,     (void*)PTD_ICON_NORMAL, PTD_ICON_NORMAL_size, nullptr);
    APIDefs->Textures_LoadFromMemory(QA_ICON_HOV_ID, (void*)PTD_ICON_NORMAL, PTD_ICON_NORMAL_size, nullptr);
    APIDefs->Textures_GetOrCreateFromMemory(FLOAT_ICON_TEX_ID, (void*)PTD_FLOAT_ICON, PTD_FLOAT_ICON_len);
    if (g.showQuickAccess)
        APIDefs->QuickAccess_Add(QA_ID, QA_ICON_ID, QA_ICON_HOV_ID, KB_TOGGLE, "ToDo List");

    std::string today  = GetCurrentUtcDate();
    std::string monday = GetThisMondayDate();
    if (g.lastDailyReset.empty())  g.lastDailyReset  = today;
    if (g.lastWeeklyReset.empty()) g.lastWeeklyReset = monday;
    CheckResetTimes();
    SaveTodos();

    APIDefs->Log(LOGL_INFO, ADDON_NAME, "Addon loaded successfully");
}

void AddonUnload() {
    if (APIDefs) {
        APIDefs->Log(LOGL_INFO, ADDON_NAME, "Unloading addon...");
        APIDefs->QuickAccess_Remove(QA_ID);
        APIDefs->InputBinds_Deregister(KB_TOGGLE);
        APIDefs->GUI_Deregister(RenderTodoWindow);
        APIDefs->GUI_Deregister(RenderOptions);
    }
    SaveTodos();
    SaveSettings();
    APIDefs = nullptr;
}

/* ── Addon definition ──────────────────────────────────────────────────────── */

AddonDefinition_t AddonDef{};

AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature   = 0xa597f7f8;
    AddonDef.APIVersion  = NEXUS_API_VERSION;
    AddonDef.Name        = "Pie Todo";
    AddonDef.Version.Major    = V_MAJOR;
    AddonDef.Version.Minor    = V_MINOR;
    AddonDef.Version.Build    = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author      = "PieOrCake.7635";
    AddonDef.Description = "ToDo list to keep track of your daily and weekly gaming activities.";
    AddonDef.Load        = AddonLoad;
    AddonDef.Unload      = AddonUnload;
    AddonDef.Flags       = AF_None;
    AddonDef.Provider    = UP_GitHub;
    AddonDef.UpdateLink  = "https://github.com/PieOrCake/pie_todo";
    return &AddonDef;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hModule);
    return TRUE;
}
