#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Shared.h"
#include "Version.h"
#include "imgui.h"
#include "Nexus.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <atomic>
#include <algorithm>

using json = nlohmann::json;

/* ── Constants ─────────────────────────────────────────────────────────────── */

static const char* ADDON_NAME       = "PieTodo";
static const char* KB_TOGGLE        = "KB_PIE_TODO_TOGGLE";
static const char* WINDOW_NAME      = "Pie's Awesome ToDo List";
static const char* QA_ID            = "PieTodo_qa";
static const char* QA_ICON_ID       = "PieTodo_icon";
static const char* QA_ICON_FILENAME = "icon\\pie-todo-paper-icon.png";

static constexpr float MIN_WINDOW_DIM    = 200.f;
static constexpr float DEFAULT_WINDOW_W  = 400.f;
static constexpr float DEFAULT_WINDOW_H  = 480.f;
static constexpr float ROW_PADDING       = 8.f;
static constexpr float INPUT_WIDTH       = 126.f;
static constexpr float COMBO_WIDTH       = 80.f;
static constexpr float EDIT_FIELD_WIDTH  = 300.f;
static constexpr float WRAP_WIDTH        = 280.f;
static constexpr double RESET_CHECK_INTERVAL = 60.0;
static constexpr double DIRTY_SAVE_DELAY     = 2.0;

/* ── Enums ─────────────────────────────────────────────────────────────────── */

enum RepeatType    { Repeat_Daily = 0, Repeat_Weekly = 1 };
enum CompletedMode { CompletedMode_Colour = 0, CompletedMode_Hide = 1 };

/* ── Data types ────────────────────────────────────────────────────────────── */

struct TodoItem {
    std::string uid;
    std::string text;
    int         repeat    = Repeat_Daily;
    bool        completed = false;
};

/* ── Application state ─────────────────────────────────────────────────────── */

struct AppState {
    std::vector<TodoItem> todos;
    std::string           lastDailyReset;
    std::string           lastWeeklyReset;
    int                   completedMode = CompletedMode_Colour;

    bool                  windowVisible         = false;
    std::atomic<bool>     pendingToggle{false};
    unsigned long long    uidCounter            = 0;

    std::string           newTaskText;
    int                   newTaskRepeat = Repeat_Daily;

    std::string           editingUid;
    std::string           editText;
    int                   editRepeat       = Repeat_Daily;
    bool                  editPopupPending = false;

    std::string           deleteConfirmUid;
    bool                  deletePopupPending = false;

    double                lastResetCheckTime = 0.0;
    int                   dragSourceIdx      = -1;

    std::string           searchFilter;

    std::string           contextMenuUid;
    std::vector<ImVec2>   rowMin, rowMax;
    std::vector<int>      rowVisibleIndices;

    float                 winX = 0.f, winY = 0.f;
    float                 winW = DEFAULT_WINDOW_W, winH = DEFAULT_WINDOW_H;
    bool                  winGeometryLoaded = false;
    bool                  lockPosition      = false;
    bool                  lockSize          = false;

    bool                  dirty          = false;
    double                dirtyTimestamp  = 0.0;
};

static AppState g;

/* ── Forward declarations ──────────────────────────────────────────────────── */

static void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
static void RenderTodoWindow();
static void RenderOptions();
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef();

/* ── Helpers ───────────────────────────────────────────────────────────────── */

static std::string GetConfigPath(const char* filename) {
    if (!APIDefs || !APIDefs->Paths_GetAddonDirectory) return {};
    const char* dir = APIDefs->Paths_GetAddonDirectory(ADDON_NAME);
    if (!dir || !dir[0]) return {};
    std::string path = dir;
    path += "\\";
    path += filename;
    return path;
}

static struct tm GetUtcTime() {
    time_t t = time(nullptr);
    struct tm result = {};
    gmtime_s(&result, &t);
    return result;
}

static std::string FormatDate(const struct tm& u) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", u.tm_year + 1900, u.tm_mon + 1, u.tm_mday);
    return buf;
}

static std::string GetCurrentUtcDate() {
    return FormatDate(GetUtcTime());
}

static std::string GetThisMondayDate() {
    struct tm u = GetUtcTime();
    int daysBack = (u.tm_wday + 6) % 7;
    time_t t = time(nullptr) - (time_t)daysBack * 86400;
    struct tm m = {};
    gmtime_s(&m, &t);
    return FormatDate(m);
}

static std::string TrimWhitespace(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= 32) start++;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= 32) end--;
    return s.substr(start, end - start);
}

/* ── Dirty-flag save ───────────────────────────────────────────────────────── */

static void MarkDirty() {
    g.dirty = true;
    g.dirtyTimestamp = ImGui::GetTime();
}

static void SaveTodos() {
    std::string path = GetConfigPath("todos.json");
    if (path.empty()) return;

    json j;
    json arr = json::array();
    for (const auto& t : g.todos) {
        arr.push_back({
            {"uid",       t.uid},
            {"text",      t.text},
            {"repeat",    t.repeat == Repeat_Weekly ? "weekly" : "daily"},
            {"completed", t.completed}
        });
    }
    j["todos"] = arr;
    j["completed_tasks_mode"] = (g.completedMode == CompletedMode_Hide) ? "hide" : "colour";
    j["lock_position"] = g.lockPosition;
    j["lock_size"] = g.lockSize;
    if (!g.lastDailyReset.empty())  j["last_daily_reset"]  = g.lastDailyReset;
    if (!g.lastWeeklyReset.empty()) j["last_weekly_reset"] = g.lastWeeklyReset;

    std::ofstream f(path);
    if (f) f << j.dump(2) << "\n";

    g.dirty = false;
}

static void FlushIfDirty() {
    if (g.dirty && (ImGui::GetTime() - g.dirtyTimestamp >= DIRTY_SAVE_DELAY))
        SaveTodos();
}

static void LoadTodos() {
    std::string path = GetConfigPath("todos.json");
    if (path.empty()) return;

    std::ifstream f(path);
    if (!f) return;

    json j;
    try { j = json::parse(f); }
    catch (...) { return; }

    /* Migrate: if old format had ISO timestamps, strip to YYYY-MM-DD */
    auto extractDate = [](const std::string& s) -> std::string {
        return (s.size() >= 10) ? s.substr(0, 10) : s;
    };

    if (j.contains("todos") && j["todos"].is_array()) {
        g.todos.clear();
        for (const auto& item : j["todos"]) {
            TodoItem t;
            if (item.contains("uid"))       t.uid       = item["uid"].get<std::string>();
            if (item.contains("text"))      t.text      = item["text"].get<std::string>();
            if (item.contains("repeat"))    t.repeat    = (item["repeat"].get<std::string>() == "weekly") ? Repeat_Weekly : Repeat_Daily;
            if (item.contains("completed")) t.completed = item["completed"].get<bool>();
            if (!t.uid.empty() || !t.text.empty())
                g.todos.push_back(std::move(t));
        }
    }

    if (j.contains("completed_tasks_mode"))
        g.completedMode = (j["completed_tasks_mode"].get<std::string>() == "hide") ? CompletedMode_Hide : CompletedMode_Colour;
    if (j.contains("lock_position"))
        g.lockPosition = j["lock_position"].get<bool>();
    if (j.contains("lock_size"))
        g.lockSize = j["lock_size"].get<bool>();
    if (j.contains("last_daily_reset"))
        g.lastDailyReset = extractDate(j["last_daily_reset"].get<std::string>());
    if (j.contains("last_weekly_reset"))
        g.lastWeeklyReset = extractDate(j["last_weekly_reset"].get<std::string>());
}

/* ── Window geometry ───────────────────────────────────────────────────────── */

static void LoadWindowGeometry() {
    std::string path = GetConfigPath("window.ini");
    if (path.empty()) return;
    std::ifstream f(path);
    if (!f) return;
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    if (f >> x >> y >> w >> h && w >= MIN_WINDOW_DIM && h >= MIN_WINDOW_DIM) {
        g.winX = x; g.winY = y; g.winW = w; g.winH = h;
        g.winGeometryLoaded = true;
    }
}

static void SaveWindowGeometry() {
    std::string path = GetConfigPath("window.ini");
    if (path.empty()) return;
    std::ofstream f(path);
    if (f) f << g.winX << " " << g.winY << " " << g.winW << " " << g.winH << "\n";
}

/* ── UID generation ────────────────────────────────────────────────────────── */

static std::string GenerateUid() {
    g.uidCounter++;
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    char buf[64];
    snprintf(buf, sizeof(buf), "id_%llu_%llu", (unsigned long long)li.QuadPart, g.uidCounter);
    return buf;
}

/* ── Reset logic ───────────────────────────────────────────────────────────── */

static void CheckResetTimes() {
    struct tm u = GetUtcTime();
    std::string today = FormatDate(u);
    bool changed = false;

    if (g.lastDailyReset.empty()) {
        g.lastDailyReset = today;
    } else if (today != g.lastDailyReset) {
        for (auto& t : g.todos) {
            if (t.repeat == Repeat_Daily && t.completed) {
                t.completed = false;
                changed = true;
            }
        }
        g.lastDailyReset = today;
    }

    std::string mondayStr = GetThisMondayDate();
    int daysSinceMonday = (u.tm_wday + 6) % 7;
    bool pastMonday0730 = (daysSinceMonday > 0) || (u.tm_hour > 7 || (u.tm_hour == 7 && u.tm_min >= 30));

    if (g.lastWeeklyReset.empty()) {
        g.lastWeeklyReset = mondayStr;
    } else if (pastMonday0730 && mondayStr != g.lastWeeklyReset) {
        for (auto& t : g.todos) {
            if (t.repeat == Repeat_Weekly && t.completed) {
                t.completed = false;
                changed = true;
            }
        }
        g.lastWeeklyReset = mondayStr;
    }

    if (changed) MarkDirty();
}

/* ── Keybind handler ───────────────────────────────────────────────────────── */

static void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;
    if (strcmp(aIdentifier, KB_TOGGLE) == 0)
        g.pendingToggle.store(true, std::memory_order_release);
}

/* ── Task operations ───────────────────────────────────────────────────────── */

static void AddNewTodo() {
    std::string text = TrimWhitespace(g.newTaskText);
    if (text.empty()) return;
    TodoItem item;
    item.uid       = GenerateUid();
    item.text      = std::move(text);
    item.repeat    = g.newTaskRepeat;
    item.completed = false;
    g.todos.push_back(std::move(item));
    g.newTaskText.clear();
    g.newTaskRepeat = Repeat_Daily;
    MarkDirty();
}

static int IndexForUid(const std::string& uid) {
    for (size_t i = 0; i < g.todos.size(); i++)
        if (g.todos[i].uid == uid) return (int)i;
    return -1;
}

static void MoveTodo(int fromIdx, int toIdx) {
    if (fromIdx < 0 || toIdx < 0 ||
        fromIdx >= (int)g.todos.size() || toIdx >= (int)g.todos.size() ||
        fromIdx == toIdx)
        return;
    TodoItem item = std::move(g.todos[fromIdx]);
    g.todos.erase(g.todos.begin() + fromIdx);
    g.todos.insert(g.todos.begin() + toIdx, std::move(item));
    MarkDirty();
}

/* ── Main window rendering ─────────────────────────────────────────────────── */

static void RenderTodoWindow() {
    if (APIDefs && APIDefs->ImguiContext)
        ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);

    if (g.pendingToggle.exchange(false, std::memory_order_acquire))
        g.windowVisible = !g.windowVisible;
    if (!g.windowVisible) return;

    /* Periodic reset check and dirty-flag flush */
    double now = ImGui::GetTime();
    if (now - g.lastResetCheckTime >= RESET_CHECK_INTERVAL) {
        g.lastResetCheckTime = now;
        CheckResetTimes();
    }
    FlushIfDirty();

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

    /* Build visible indices (respects completed-mode and search filter) */
    std::vector<int> visibleIndices;
    visibleIndices.reserve(g.todos.size());
    for (size_t i = 0; i < g.todos.size(); i++) {
        if (g.completedMode == CompletedMode_Hide && g.todos[i].completed) continue;
        if (!g.searchFilter.empty()) {
            /* Case-insensitive substring match */
            const std::string& txt = g.todos[i].text;
            const std::string& flt = g.searchFilter;
            bool found = std::search(txt.begin(), txt.end(), flt.begin(), flt.end(),
                [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }
            ) != txt.end();
            if (!found) continue;
        }
        visibleIndices.push_back((int)i);
    }

    /* Task list child window (leave room for status bar) */
    float statusBarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("TaskList", ImVec2(-1, -statusBarHeight), true)) {
        const float listWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 winPos = ImGui::GetWindowPos();
        float winWidth = ImGui::GetWindowWidth();
        std::vector<ImVec2> rowMin(visibleIndices.size());
        std::vector<ImVec2> rowMax(visibleIndices.size());
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
            rowMin[vi] = ImVec2(winPos.x, rowStartPos.y);
            rowMax[vi] = ImVec2(winPos.x + listWidth, rEnd.y);

            ImGui::PopID();
        }

        /* Drop target indicator and drop handling */
        if (g.dragSourceIdx >= 0) {
            ImVec2 mouse = ImGui::GetMousePos();
            int dropVisIdx = -1;
            bool dropAfter = false;

            /* Find which row the cursor is over and whether it's in the top or bottom half */
            for (size_t vi = 0; vi < visibleIndices.size(); vi++) {
                if (mouse.y >= rowMin[vi].y && mouse.y < rowMax[vi].y) {
                    float midY = (rowMin[vi].y + rowMax[vi].y) * 0.5f;
                    dropVisIdx = (int)vi;
                    dropAfter = (mouse.y >= midY);
                    break;
                }
            }
            /* If cursor is below all rows, drop after the last row */
            if (dropVisIdx < 0 && !visibleIndices.empty() && mouse.y >= rowMax.back().y) {
                dropVisIdx = (int)visibleIndices.size() - 1;
                dropAfter = true;
            }

            /* Draw a very obvious drop indicator: thick line + arrow markers */
            if (dropVisIdx >= 0) {
                float lineY = dropAfter ? rowMax[dropVisIdx].y : rowMin[dropVisIdx].y;
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

        /* Snapshot row rects for right-click after EndChild */
        g.rowMin = std::move(rowMin);
        g.rowMax = std::move(rowMax);
        g.rowVisibleIndices = std::move(visibleIndices);
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

    /* Status bar */
    {
        int total = (int)g.todos.size();
        int done = 0;
        for (const auto& t : g.todos) if (t.completed) done++;
        char statusBuf[64];
        snprintf(statusBuf, sizeof(statusBuf), "%d/%d completed", done, total);
        float textW = ImGui::CalcTextSize(statusBuf).x;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - textW);
        ImGui::TextUnformatted(statusBuf);
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

    APIDefs->InputBinds_RegisterWithString(KB_TOGGLE, ProcessKeybind, "CTRL+SHIFT+T");
    APIDefs->GUI_Register(RT_Render, RenderTodoWindow);
    APIDefs->GUI_Register(RT_OptionsRender, RenderOptions);

    /* Load Quick Access icon and register shortcut */
    const char* dir = APIDefs->Paths_GetAddonDirectory(ADDON_NAME);
    if (dir && dir[0]) {
        std::string iconPath = std::string(dir) + "\\" + QA_ICON_FILENAME;
        APIDefs->Textures_GetOrCreateFromFile(QA_ICON_ID, iconPath.c_str());
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
    AddonDef.Version.Major    = ADDON_VERSION_MAJOR;
    AddonDef.Version.Minor    = ADDON_VERSION_MINOR;
    AddonDef.Version.Build    = ADDON_VERSION_BUILD;
    AddonDef.Version.Revision = ADDON_VERSION_REVISION;
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
