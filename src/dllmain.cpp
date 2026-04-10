#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cstdio>
#include <cstring>

#include "nexus/Nexus.h"
#include "imgui.h"
#include "TodoManager.h"
#include "Shared.h"

// Version constants
#define V_MAJOR 0
#define V_MINOR 9
#define V_BUILD 0
#define V_REVISION 1

/* ── UI Constants ──────────────────────────────────────────────────────────── */

static const char* WINDOW_NAME      = "Pie's Awesome ToDo List";
static const char* ICON_WINDOW_NAME = "##PieTodoIcon";

static constexpr float ROW_PADDING       = 8.f;
static constexpr float INPUT_WIDTH       = 126.f;
static constexpr float COMBO_WIDTH       = 80.f;
static constexpr float EDIT_FIELD_WIDTH  = 300.f;
static constexpr float WRAP_WIDTH        = 280.f;

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
        ImGui::SetNextWindowPos(ImVec2(g.winX, g.winY), ImGuiCond_Always);
        ImGuiWindowFlags iconFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin(ICON_WINDOW_NAME, nullptr, iconFlags)) {
            Texture_t* tex = APIDefs->Textures_Get(QA_ICON_ID);
            if (tex && tex->Resource) {
                ImGui::Image(tex->Resource, ImVec2(32.f, 32.f));
            }
            char dBuf[32], wBuf[32];
            snprintf(dBuf, sizeof(dBuf), "D: %d/%d", g.cachedDailyDone, g.cachedDailyTotal);
            snprintf(wBuf, sizeof(wBuf), "W: %d/%d", g.cachedWeeklyDone, g.cachedWeeklyTotal);
            ImGui::TextUnformatted(dBuf);
            ImGui::TextUnformatted(wBuf);

            if (ImGui::IsWindowHovered()) {
                g.collapsed = false;
                g.lastHoverTime = now;
            }
        }
        ImGui::End();
        return;
    }

    /* Window setup */
    if (g.winGeometryLoaded) {
        ImGui::SetNextWindowPos(ImVec2(g.winX, g.winY), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(g.winW, g.winH), ImGuiCond_FirstUseEver);
    } else {
        ImGui::SetNextWindowSize(ImVec2(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H), ImGuiCond_FirstUseEver);
    }
    ImGuiWindowFlags wflags = ImGuiWindowFlags_None;
    if (g.lockPosition) wflags |= ImGuiWindowFlags_NoMove;
    if (g.lockSize) wflags |= ImGuiWindowFlags_NoResize;
    if (!ImGui::Begin(WINDOW_NAME, &g.windowVisible, wflags)) {
        ImGui::End();
        return;
    }

    /* New task input row */
    ImGui::SetNextItemWidth(INPUT_WIDTH);
    char newBuf[512];
    strncpy(newBuf, g.newTaskText.c_str(), sizeof(newBuf) - 1);
    newBuf[sizeof(newBuf) - 1] = '\0';
    if (ImGui::InputTextWithHint("##newtask", "New task...", newBuf, sizeof(newBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        g.newTaskText = newBuf;
        AddNewTodo();
    } else {
        g.newTaskText = newBuf;
    }
    ImGui::SameLine();
    const char* repeatLabels[] = { "Daily", "Weekly" };
    ImGui::SetNextItemWidth(COMBO_WIDTH);
    ImGui::Combo("##repeat", &g.newTaskRepeat, repeatLabels, 2);
    ImGui::SameLine();
    if (ImGui::Button("Add")) AddNewTodo();

    ImGui::Separator();

    /* Search / filter bar (replaces the old "Tasks" label) */
    {
        char searchBuf[256];
        strncpy(searchBuf, g.searchFilter.c_str(), sizeof(searchBuf) - 1);
        searchBuf[sizeof(searchBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize("X").x - ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().FramePadding.x * 2.f);
        if (ImGui::InputTextWithHint("##search", "Search tasks...", searchBuf, sizeof(searchBuf)))
            g.searchFilter = searchBuf;
        ImGui::SameLine();
        if (ImGui::Button("X")) g.searchFilter.clear();
    }

    /* Use cached visible indices (rebuilt only on data/filter change) */
    const std::vector<int>& visibleIndices = g.cachedVisibleIndices;

    /* Task list child window (leave room for status bar) */
    float statusBarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("TaskList", ImVec2(-1, -statusBarHeight), true)) {
        const float listWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 winPos = ImGui::GetWindowPos();
        float winWidth = ImGui::GetWindowWidth();
        /* Reuse persistent row rect vectors instead of allocating every frame */
        g.rowMin.resize(visibleIndices.size());
        g.rowMax.resize(visibleIndices.size());
        const float repeatColWidth = ImGui::CalcTextSize("Weekly").x + ImGui::GetStyle().ItemSpacing.x * 2.f;

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
                ImGui::GetWindowDrawList()->AddRectFilled(zMin, zMax, IM_COL32(255, 255, 255, 10));
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
            ImGui::TextUnformatted(item.repeat == Repeat_Weekly ? "Weekly" : "Daily");

            /* Draw row background highlight for completed tasks (full row height) */
            if (completed) {
                ImVec2 rMin = ImVec2(winPos.x, rowStartPos.y);
                ImVec2 rMax = ImVec2(winPos.x + winWidth, rowStartPos.y + ImGui::GetFrameHeightWithSpacing());
                ImGui::GetWindowDrawList()->AddRectFilled(rMin, rMax, IM_COL32(100, 230, 100, 90));
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

        /* rowMin/rowMax are already written to g.rowMin/g.rowMax in-place */
        g.rowVisibleIndices = visibleIndices;
    }
    ImGui::EndChild();

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
        ImGui::Combo("Repeat", &g.editRepeat, editRepeatLabels, 2);
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

    /* Status bar (uses cached counts) */
    {
        char statusBuf[64];
        snprintf(statusBuf, sizeof(statusBuf), "%d/%d completed", g.cachedDone, g.cachedTotal);
        float textW = ImGui::CalcTextSize(statusBuf).x;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - textW);
        ImGui::TextUnformatted(statusBuf);
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
    ImGui::Spacing();
    ImGui::TextWrapped("Show completed tasks with a green row (Colour) or hide them (Hide).");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Lock window position", &g.lockPosition))
        MarkDirty();
    if (ImGui::Checkbox("Lock window size", &g.lockSize))
        MarkDirty();
    ImGui::TextWrapped("Prevent the window from being moved or resized.");

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
    ImGui::TextWrapped("Show or hide the shortcut icon in the Quick Access bar.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Open on launch", &g.openOnLaunch))
        MarkDirty();
    ImGui::TextWrapped("Automatically show the window when the game starts.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Checkbox("Collapse to icon", &g.collapseEnabled)) {
        if (!g.collapseEnabled) g.collapsed = false;
        MarkDirty();
    }
    if (g.collapseEnabled) {
        ImGui::SetNextItemWidth(100.f);
        if (ImGui::InputFloat("Delay (seconds)", &g.collapseDelaySec, 0.5f, 1.0f, "%.1f")) {
            if (g.collapseDelaySec < 0.5f) g.collapseDelaySec = 0.5f;
            if (g.collapseDelaySec > 30.0f) g.collapseDelaySec = 30.0f;
            MarkDirty();
        }
    }
    ImGui::TextWrapped("When enabled, the window collapses to an icon after the mouse leaves. Hover the icon to expand.");
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

    /* Load Quick Access icon and register shortcut */
    const char* dir = APIDefs->Paths_GetAddonDirectory(ADDON_NAME);
    if (dir && dir[0]) {
        std::string iconPath = std::string(dir) + "\\" + QA_ICON_FILENAME;
        APIDefs->Textures_GetOrCreateFromFile(QA_ICON_ID, iconPath.c_str());
        if (g.showQuickAccess)
            APIDefs->QuickAccess_Add(QA_ID, QA_ICON_ID, QA_ICON_ID, KB_TOGGLE, "ToDo List");
    }

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
