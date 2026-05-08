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
static const char* FLOAT_ICON_TEX_ID  = "PieTodo_float_icon";

static constexpr float ROW_PADDING        = 8.f;
static constexpr float INPUT_WIDTH        = 86.f;
static constexpr float COMBO_WIDTH        = 64.f;
static constexpr float EDIT_FIELD_WIDTH   = 300.f;
static constexpr float WRAP_WIDTH         = 280.f;
static constexpr float DRAG_HANDLE_HEIGHT = 65.f;
static constexpr float RESIZE_GRIP_SIZE   = 28.f;

/* ── Forward declarations ──────────────────────────────────────────────────── */

static void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
static void RenderTodoWindow();
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

    /* Collapsed icon mode */
    if (g.collapseEnabled && g.collapsed) {
        const float sz = g.floatIconSize;
        ImGui::SetNextWindowPos(ImVec2(g.winX, g.winY), ImGuiCond_Always);
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

    ImGui::SetWindowFontScale(ImGui::GetWindowWidth() / DEFAULT_WINDOW_W);

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

    /* ── Drag handle (invisible strip, visual dots) ──────────────────────────── */
    ImGui::SetCursorPos(ImVec2(0.f, g.posDragY));
    ImGui::InvisibleButton("##drag", ImVec2(ImGui::GetWindowWidth(), g.posDragH));
    bool dragHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0) && !g.lockPosition) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        g.winX += delta.x;
        g.winY += delta.y;
        ImGui::SetWindowPos(ImVec2(g.winX, g.winY));
    }
    if (dragHovered && !g.lockPosition)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    {
        ImDrawList* fdl = ImGui::GetForegroundDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float cx = wp.x + ImGui::GetWindowWidth() * 0.5f;
        float cy = wp.y + 4.f + g.posDragY + g.posDragH * 0.5f;
        ImU32 dotCol = IM_COL32(80, 40, 10, dragHovered ? 180 : 80);
        for (int i = -2; i <= 2; i++)
            fdl->AddCircleFilled(ImVec2(cx + i * 6.f, cy), 2.5f, dotCol);
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
        float taskW = ImGui::GetWindowWidth() - 8.f - 2.f * g.posTaskX;
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

    /* Track window geometry (saved on unload, not every frame) */
    g.winX = ImGui::GetWindowPos().x;
    g.winY = ImGui::GetWindowPos().y;
    g.winW = ImGui::GetWindowWidth();
    g.winH = ImGui::GetWindowHeight();

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
              wpos.x + pad + g.posTaskX,   wpos.y + pad + g.posTaskY,   winW - 8.f - 2.f*g.posTaskX, g.posTaskBot - g.posTaskY,
              &g.posTaskX,        &g.posTaskY,   nullptr,          &g.posTaskBot },
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

/* ── Options panel (Nexus addon settings) ──────────────────────────────────── */

static void RenderOptions() {
    if (APIDefs && APIDefs->ImguiContext)
        ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
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
            APIDefs->QuickAccess_Add(QA_ID, QA_ICON_ID, QA_ICON_ID, KB_TOGGLE, "ToDo List");
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Layout edit mode", &g.layoutEditMode))
        MarkDirty();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Drag the coloured overlays on the window\nto reposition each element.\nYellow corner = resize.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset layout")) {
        g.posDragY = 40.f; g.posDragH = 29.f;
        g.posSearchX = 151.f; g.posSearchY = 79.f; g.posSearchW = 189.f;
        g.posTaskX = 112.f; g.posTaskY = 138.f; g.posTaskBot = 351.f;
        g.posAddX = 149.f; g.posAddY = 380.f;
        g.winW = DEFAULT_WINDOW_W; g.winH = DEFAULT_WINDOW_H;
        MarkDirty();
    }

    if (g.layoutEditMode) {
        ImGui::TextDisabled("Window size (aspect-locked)");
        ImGui::SetNextItemWidth(110.f);
        if (ImGui::SliderFloat("Win W", &g.winW, 100.f, 800.f, "%.0f")) {
            g.winH = g.winW * SCROLL_ASPECT;
            ImGui::SetWindowSize(WINDOW_NAME, ImVec2(g.winW, g.winH)); MarkDirty();
        }
        ImGui::SetNextItemWidth(110.f);
        if (ImGui::SliderFloat("Win H", &g.winH, 100.f, 800.f, "%.0f")) {
            g.winW = g.winH / SCROLL_ASPECT;
            ImGui::SetWindowSize(WINDOW_NAME, ImVec2(g.winW, g.winH)); MarkDirty();
        }
        ImGui::TextDisabled("Drag handle");
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("DH Y",  &g.posDragY,   -500.f, 600.f, "%.0f")) MarkDirty();
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("DH H",  &g.posDragH,    10.f,  150.f, "%.0f")) MarkDirty();
        ImGui::TextDisabled("Search bar");
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("SR X",  &g.posSearchX, -500.f, 600.f, "%.0f")) MarkDirty();
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("SR Y",  &g.posSearchY, -500.f, 600.f, "%.0f")) MarkDirty();
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("SR W",  &g.posSearchW,   30.f, 300.f, "%.0f")) MarkDirty();
        ImGui::TextDisabled("Task list");
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("TL X",  &g.posTaskX,   -500.f, 600.f, "%.0f")) MarkDirty();
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("TL Y",  &g.posTaskY,   -500.f, 600.f, "%.0f")) MarkDirty();
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("TL Bot",&g.posTaskBot,    50.f, 800.f, "%.0f")) MarkDirty();
        ImGui::TextDisabled("Add row");
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("AR X",  &g.posAddX,    -500.f, 600.f, "%.0f")) MarkDirty();
        ImGui::SetNextItemWidth(110.f); if (ImGui::SliderFloat("AR Y",  &g.posAddY,    -500.f, 800.f, "%.0f")) MarkDirty();
    }

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
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void (*)(void*, void*))APIDefs->ImguiFree);

    APIDefs->Log(LOGL_INFO, ADDON_NAME, "Loading addon...");

    LoadTodos();
    LoadWindowGeometry();

    if (g.openOnLaunch) {
        g.windowVisible = true;
        if (g.collapseEnabled)
            g.collapsed = true;
    }

    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE, ProcessKeybind, "CTRL+SHIFT+T");
    APIDefs->GUI_Register(RT_Render, RenderTodoWindow);
    APIDefs->GUI_Register(RT_OptionsRender, RenderOptions);

    /* Load icons from embedded data and register Quick Access shortcut */
    APIDefs->Textures_GetOrCreateFromMemory(QA_ICON_ID,    (void*)PTD_ICON,       PTD_ICON_len);
    APIDefs->Textures_GetOrCreateFromMemory(FLOAT_ICON_TEX_ID, (void*)PTD_FLOAT_ICON, PTD_FLOAT_ICON_len);
    if (g.showQuickAccess)
        APIDefs->QuickAccess_Add(QA_ID, QA_ICON_ID, QA_ICON_ID, KB_TOGGLE, "ToDo List");

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
    if (g.winW >= MIN_WINDOW_DIM && g.winH >= MIN_WINDOW_DIM)
        SaveWindowGeometry();
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
