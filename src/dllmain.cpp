#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstdio>
#include <cstring>

#include "nexus/Nexus.h"
#include "imgui.h"
#include "TodoManager.h"
#include "Shared.h"
#include "PieTheme.h"
#include "ptd_icon.h"
#include "ptd_float_icon.h"

// Version constants
#define V_MAJOR 0
#define V_MINOR 9
#define V_BUILD 3
#define V_REVISION 0

/* ── UI Constants ──────────────────────────────────────────────────────────── */

static const char* ICON_WINDOW_NAME   = "##PieTodoIcon";
static const char* FLOAT_ICON_TEX_ID  = "PieTodo_float_icon_v2";

static constexpr float ROW_PADDING        = 8.f;
static constexpr float INPUT_WIDTH        = 132.f;
static constexpr float COMBO_WIDTH        = 64.f;
static constexpr float EDIT_FIELD_WIDTH   = 300.f;
static constexpr float WRAP_WIDTH         = 280.f;

/* ── GW2-style ImGui theme (copied from tyrian_home_garden) ─────────────────── */

static ImGuiStyle              g_GW2Style;
static std::vector<ImGuiStyle> g_StyleStack;

static void PushGW2Theme() {
    g_StyleStack.push_back(ImGui::GetStyle());
    ImGui::GetStyle() = g_GW2Style;
    /* If Pie UI is broadcasting a palette and the user wants it, recolour on top
     * of the GW2 layout (keeps our rounding/spacing, swaps only the colours). */
    if (g.usePieTheme && PieTheme::HasPalette())
        PieTheme::ApplyTo(ImGui::GetStyle());
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

/* Centre the next popup on the current todo window. Call while that window is the
 * active ImGui window, just before BeginPopupModal. */
static void CenterPopupOnWindow() {
    ImVec2 wpos = ImGui::GetWindowPos();
    ImVec2 wsz  = ImGui::GetWindowSize();
    ImGui::SetNextWindowPos(ImVec2(wpos.x + wsz.x * 0.5f, wpos.y + wsz.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

/* Draw the always-visible floating status pip (independent of the main window).
 * Shows D/W counts; opens/toggles the list on hover or click; draggable when
 * unlocked, fixed when locked. */
static void RenderFloatingIcon(double now) {
    const float sz     = g.floatIconSize;
    const bool  locked = g.floatIconLocked;

    ImGui::SetNextWindowPos(ImVec2(g.floatX, g.floatY),
                            locked ? ImGuiCond_Always : ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(sz, sz));
    ImGuiWindowFlags iconFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoFocusOnAppearing;
    if (locked)
        iconFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

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
            float   fontSize = sz * 0.19f;
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

        bool hovered = ImGui::IsWindowHovered();
        if (locked) {
            /* Fixed position. Hover opens (and keeps it alive), or click toggles. */
            if (hovered) {
                if (g.expandOnClick) {
                    if (ImGui::IsMouseClicked(0)) { g.windowVisible = !g.windowVisible; g.lastHoverTime = now; }
                } else {
                    g.windowVisible = true;
                    g.lastHoverTime = now;
                }
            }
        } else {
            /* Draggable: persist the moved position; a click without a real drag
             * toggles the main window. */
            ImVec2 wp = ImGui::GetWindowPos();
            if (wp.x != g.floatX || wp.y != g.floatY) { g.floatX = wp.x; g.floatY = wp.y; MarkDirty(); }
            if (hovered && ImGui::IsMouseReleased(0)) {
                ImVec2 dd = ImGui::GetMouseDragDelta(0);
                if (dd.x * dd.x + dd.y * dd.y < 25.0f) { g.windowVisible = !g.windowVisible; g.lastHoverTime = now; }
            }
        }
    }
    ImGui::End();
}

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
        if (g.windowVisible)
            g.lastHoverTime = now;
    }

    /* Refresh reset state and the render cache whenever anything is on screen —
     * the pip shows completion counts even while the main window is hidden. */
    if (g.windowVisible || g.floatIconEnabled) {
        if (now - g.lastResetCheckTime >= RESET_CHECK_INTERVAL) {
            g.lastResetCheckTime = now;
            CheckResetTimes();
        }
        if (g.cacheDirty || g.searchFilter != g.cachedSearchFilter || g.completedMode != g.cachedCompletedMode)
            RebuildCache();
    }

    /* Always-visible floating status pip, independent of the main window. */
    if (g.floatIconEnabled)
        RenderFloatingIcon(now);

    if (!g.windowVisible) return;

    RenderTodoWindowBoring();
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

            /* Full-row selectable (drag-drop source). Height matches the checkbox
             * frame so the hover highlight covers the whole row, not just the text line. */
            ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowItemOverlap,
                              ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()));
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
            /* Completed rows are green by default, or the Pie UI accent when that theme is active. */
            ImU32 completedAccent = (g.usePieTheme && PieTheme::HasPalette()) ? PieTheme::Accent() : 0;
            if (completed && g.completedMode == CompletedMode_Colour)
                ImGui::PushStyleColor(ImGuiCol_Text,
                    completedAccent ? ImGui::ColorConvertU32ToFloat4(completedAccent)
                                    : ImVec4(0.35f, 0.82f, 0.35f, 1.0f));
            ImGui::TextUnformatted(item.text.c_str());
            if (completed && g.completedMode == CompletedMode_Colour)
                ImGui::PopStyleColor();

            /* Repeat label right-aligned */
            ImGui::SameLine(winW - repeatColWidth - ImGui::GetStyle().WindowPadding.x);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(item.repeat == Repeat_Weekly ? "Weekly" : "Daily");

            /* Row background highlight for completed tasks */
            if (completed && g.completedMode == CompletedMode_Colour) {
                ImU32 rowCol = completedAccent ? ((completedAccent & 0x00FFFFFFu) | (40u << 24))
                                               : IM_COL32(35, 82, 35, 40);
                ImVec2 rMin(winPos.x, rowPos.y);
                ImVec2 rMax(winPos.x + winW, rowPos.y + ImGui::GetFrameHeightWithSpacing());
                ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, rowCol);
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
    CenterPopupOnWindow();
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
    CenterPopupOnWindow();
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

    /* Auto-hide timer — hide the main window after the mouse leaves it. */
    if (g.autoHideEnabled) {
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
        else if (g.lastHoverTime > 0.0 && (now - g.lastHoverTime) >= (double)g.autoHideDelaySec)
            g.windowVisible = false;
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
    if (ImGui::Checkbox("Use Pie UI theme (if available)", &g.usePieTheme))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Match the list's colours to Pie UI when that addon is running.");

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
    if (ImGui::Checkbox("Open on launch", &g.openOnLaunch))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Automatically show the window when the game starts.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* ── Floating icon ─────────────────────────────────────────────────────── */
    if (ImGui::Checkbox("Show floating icon", &g.floatIconEnabled))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("A small always-visible icon showing your daily/weekly progress.\nClick or hover it to open the list.");
    if (g.floatIconEnabled) {
        ImGui::Indent();
        if (ImGui::Checkbox("Lock icon position", &g.floatIconLocked))
            MarkDirty();
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Unlock to drag the icon anywhere; click it to open the list.");
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::SliderFloat("Icon size", &g.floatIconSize, 32.f, 128.f, "%.0f")) {
            g.floatIconSize = std::max(32.f, std::min(g.floatIconSize, 128.f));
            MarkDirty();
        }
        if (g.floatIconLocked) {
            ImGui::Text("Open on:");
            ImGui::SameLine();
            if (ImGui::RadioButton("Hover", !g.expandOnClick)) { g.expandOnClick = false; MarkDirty(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("Click", g.expandOnClick))  { g.expandOnClick = true;  MarkDirty(); }
        }
        ImGui::Unindent();
    }

    ImGui::Spacing();

    /* ── Auto-hide ─────────────────────────────────────────────────────────── */
    if (ImGui::Checkbox("Auto-hide window", &g.autoHideEnabled))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hides the list a moment after the mouse leaves it.\nBring it back with the floating icon, the Quick Access icon, or the hotkey.");
    if (g.autoHideEnabled) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::InputFloat("Hide after (seconds)", &g.autoHideDelaySec, 0.5f, 1.0f, "%.1f")) {
            if (g.autoHideDelaySec < 0.5f)  g.autoHideDelaySec = 0.5f;
            if (g.autoHideDelaySec > 30.0f) g.autoHideDelaySec = 30.0f;
            MarkDirty();
        }
        ImGui::Unindent();
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

    if (g.openOnLaunch)
        g.windowVisible = true;

    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE, ProcessKeybind, "CTRL+SHIFT+T");
    APIDefs->GUI_Register(RT_Render, RenderTodoWindow);
    APIDefs->GUI_Register(RT_OptionsRender, RenderOptions);

    /* Subscribe to Pie UI's theme broadcast and ask it to (re-)send. */
    PieTheme::Init();

    /* Load icons from embedded data and register Quick Access shortcut */
    APIDefs->Textures_LoadFromMemory(QA_ICON_ID,     (void*)PTD_ICON_NORMAL, PTD_ICON_NORMAL_size, nullptr);
    APIDefs->Textures_LoadFromMemory(QA_ICON_HOV_ID, (void*)PTD_ICON_HOV,    PTD_ICON_HOV_size,    nullptr);
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
        PieTheme::Shutdown();
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
