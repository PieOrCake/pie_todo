#include "TodoManager.h"
#include "Shared.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <algorithm>

using json = nlohmann::json;

/* ── Constants ─────────────────────────────────────────────────────────────── */

const char* ADDON_NAME       = "PieTodo";
const char* KB_TOGGLE        = "KB_PIE_TODO_TOGGLE";
const char* QA_ID            = "PieTodo_qa";
const char* QA_ICON_ID       = "PieTodo_icon";
const char* QA_ICON_HOV_ID   = "PieTodo_icon_hov";
const char* QA_ICON_FILENAME = "icon\\pie-todo-paper-icon.png";

/* ── Global state ──────────────────────────────────────────────────────────── */

AppState g;

/* ── Helpers ───────────────────────────────────────────────────────────────── */

std::string GetConfigPath(const char* filename) {
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

std::string GetCurrentUtcDate() {
    return FormatDate(GetUtcTime());
}

std::string GetThisMondayDate() {
    struct tm u = GetUtcTime();
    int daysBack = (u.tm_wday + 6) % 7;
    time_t t = time(nullptr) - (time_t)daysBack * 86400;
    struct tm m = {};
    gmtime_s(&m, &t);
    return FormatDate(m);
}

std::string TrimWhitespace(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= 32) start++;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= 32) end--;
    return s.substr(start, end - start);
}

/* ── File modification time helper ─────────────────────────────────────────── */

FILETIME GetFileModTime(const std::string& path) {
    FILETIME ft = {};
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attr))
        ft = attr.ftLastWriteTime;
    return ft;
}

/* ── Dirty-flag save ───────────────────────────────────────────────────────── */

void InvalidateCache() {
    g.cacheDirty = true;
}

void MarkDirty() {
    g.dirty = true;
    g.dirtyTimestamp = ImGui::GetTime();
    InvalidateCache();
}

void SaveTodos() {
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
    if (!g.lastDailyReset.empty())  j["last_daily_reset"]  = g.lastDailyReset;
    if (!g.lastWeeklyReset.empty()) j["last_weekly_reset"] = g.lastWeeklyReset;

    std::string tmpPath = path + ".tmp";
    {
        std::ofstream f(tmpPath);
        if (!f) return;
        f << j.dump(2) << "\n";
        if (!f.good()) {
            f.close();
            DeleteFileA(tmpPath.c_str());
            return;
        }
    }
    if (!MoveFileExA(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmpPath.c_str());
        return;
    }

    g.dirty = false;

    /* Update stored mod time so we don't reload our own save */
    g.lastFileWriteTime = GetFileModTime(path);
}

void FlushIfDirty() {
    if (g.dirty && (ImGui::GetTime() - g.dirtyTimestamp >= DIRTY_SAVE_DELAY)) {
        SaveTodos();
        SaveSettings();
    }
}

void LoadTodos() {
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

    if (j.contains("last_daily_reset"))
        g.lastDailyReset = extractDate(j["last_daily_reset"].get<std::string>());
    if (j.contains("last_weekly_reset"))
        g.lastWeeklyReset = extractDate(j["last_weekly_reset"].get<std::string>());

    /* Update stored mod time after loading */
    g.lastFileWriteTime = GetFileModTime(path);
    InvalidateCache();
}

/* ── Settings (settings.json) ──────────────────────────────────────────────── */

static void ClampPosition(float& x, float& y, float w, float h) {
    const float margin = 50.f;
    float sx = (float)GetSystemMetrics(SM_XVIRTUALSCREEN);
    float sy = (float)GetSystemMetrics(SM_YVIRTUALSCREEN);
    float sw = (float)GetSystemMetrics(SM_CXVIRTUALSCREEN);
    float sh = (float)GetSystemMetrics(SM_CYVIRTUALSCREEN);
    x = std::max(sx - w + margin, std::min(x, sx + sw - margin));
    y = std::max(sy,              std::min(y, sy + sh - margin));
}

void SaveSettings() {
    std::string path = GetConfigPath("settings.json");
    if (path.empty()) return;
    json j;
    j["win_x"]    = g.winX;      j["win_y"]    = g.winY;
    j["boring_x"] = g.boringX;   j["boring_y"] = g.boringY;
    j["boring_w"] = g.boringW;   j["boring_h"] = g.boringH;
    j["completed_tasks_mode"] = (g.completedMode == CompletedMode_Hide) ? "hide" : "colour";
    j["lock_position"]    = g.lockPosition;
    j["lock_size"]        = g.lockSize;
    j["show_quick_access"]= g.showQuickAccess;
    j["open_on_launch"]   = g.openOnLaunch;
    j["collapse_enabled"] = g.collapseEnabled;
    j["collapse_delay_sec"]= g.collapseDelaySec;
    j["float_icon_size"]  = g.floatIconSize;
    j["expand_on_click"]  = g.expandOnClick;
    j["display_mode"]     = g.displayMode;
    std::ofstream f(path);
    if (f) f << j.dump(2) << "\n";
}

void LoadSettings() {
    std::string path = GetConfigPath("settings.json");
    if (path.empty()) return;

    std::ifstream f(path);
    if (f) {
        /* Normal load */
        json j;
        try { j = json::parse(f); } catch (...) { return; }
        g.winX    = j.value("win_x",    g.winX);
        g.winY    = j.value("win_y",    g.winY);
        g.boringX = j.value("boring_x", g.boringX);
        g.boringY = j.value("boring_y", g.boringY);
        g.boringW = j.value("boring_w", g.boringW);
        g.boringH = j.value("boring_h", g.boringH);
        if (j.contains("completed_tasks_mode"))
            g.completedMode = (j["completed_tasks_mode"].get<std::string>() == "hide") ? CompletedMode_Hide : CompletedMode_Colour;
        g.lockPosition    = j.value("lock_position",     g.lockPosition);
        g.lockSize        = j.value("lock_size",         g.lockSize);
        g.showQuickAccess = j.value("show_quick_access", g.showQuickAccess);
        g.openOnLaunch    = j.value("open_on_launch",    g.openOnLaunch);
        g.collapseEnabled = j.value("collapse_enabled",  g.collapseEnabled);
        g.collapseDelaySec= j.value("collapse_delay_sec",g.collapseDelaySec);
        g.floatIconSize   = j.value("float_icon_size",   g.floatIconSize);
        g.expandOnClick   = j.value("expand_on_click",   g.expandOnClick);
        g.displayMode     = j.value("display_mode",      g.displayMode);
        ClampPosition(g.winX, g.winY, g.winW, g.winH);
        ClampPosition(g.boringX, g.boringY, g.boringW, g.boringH);
    } else {
        /* First run of new version — migrate from old files */

        /* 1. Read preferences from old todos.json */
        std::string todosPath = GetConfigPath("todos.json");
        std::ifstream tf(todosPath);
        if (tf) {
            json j;
            try {
                j = json::parse(tf);
                if (j.contains("completed_tasks_mode"))
                    g.completedMode = (j["completed_tasks_mode"].get<std::string>() == "hide") ? CompletedMode_Hide : CompletedMode_Colour;
                g.lockPosition    = j.value("lock_position",     g.lockPosition);
                g.lockSize        = j.value("lock_size",         g.lockSize);
                g.showQuickAccess = j.value("show_quick_access", g.showQuickAccess);
                g.openOnLaunch    = j.value("open_on_launch",    g.openOnLaunch);
                g.collapseEnabled = j.value("collapse_enabled",  g.collapseEnabled);
                g.collapseDelaySec= j.value("collapse_delay_sec",g.collapseDelaySec);
                g.floatIconSize   = j.value("float_icon_size",   g.floatIconSize);
                g.expandOnClick   = j.value("expand_on_click",   g.expandOnClick);
            } catch (...) {}
        }

        /* 2. Read window geometry from old window.json → boring geometry */
        std::string winPath = GetConfigPath("window.json");
        std::ifstream wf(winPath);
        if (wf) {
            json j;
            try {
                j = json::parse(wf);
                g.boringX = j.value("x", g.boringX);
                g.boringY = j.value("y", g.boringY);
                g.boringW = j.value("w", g.boringW);
                g.boringH = j.value("h", g.boringH);
                ClampPosition(g.boringX, g.boringY, g.boringW, g.boringH);
            } catch (...) {}
        }

        /* 3. Previous version had no fancy mode — start in boring */
        g.displayMode = DisplayMode_Boring;

        /* 4. Write settings.json so next run is a normal load */
        SaveSettings();
    }
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

void CheckResetTimes() {
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

/* ── Task operations ───────────────────────────────────────────────────────── */

void AddNewTodo() {
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

int IndexForUid(const std::string& uid) {
    for (size_t i = 0; i < g.todos.size(); i++)
        if (g.todos[i].uid == uid) return (int)i;
    return -1;
}

void MoveTodo(int fromIdx, int toIdx) {
    if (fromIdx < 0 || toIdx < 0 ||
        fromIdx >= (int)g.todos.size() || toIdx >= (int)g.todos.size() ||
        fromIdx == toIdx)
        return;
    TodoItem item = std::move(g.todos[fromIdx]);
    g.todos.erase(g.todos.begin() + fromIdx);
    g.todos.insert(g.todos.begin() + toIdx, std::move(item));
    MarkDirty();
}

/* ── Cache rebuild ─────────────────────────────────────────────────────────── */

void RebuildCache() {
    /* Completion counts */
    g.cachedDailyTotal = g.cachedDailyDone = 0;
    g.cachedWeeklyTotal = g.cachedWeeklyDone = 0;
    g.cachedTotal = (int)g.todos.size();
    g.cachedDone = 0;
    for (const auto& t : g.todos) {
        if (t.repeat == Repeat_Daily)  { g.cachedDailyTotal++; if (t.completed) g.cachedDailyDone++; }
        else                           { g.cachedWeeklyTotal++; if (t.completed) g.cachedWeeklyDone++; }
        if (t.completed) g.cachedDone++;
    }

    /* Visible indices (respects completed-mode and search filter) */
    g.cachedVisibleIndices.clear();
    g.cachedVisibleIndices.reserve(g.todos.size());
    for (size_t i = 0; i < g.todos.size(); i++) {
        if (g.completedMode == CompletedMode_Hide && g.todos[i].completed) continue;
        if (!g.searchFilter.empty()) {
            const std::string& txt = g.todos[i].text;
            const std::string& flt = g.searchFilter;
            bool found = std::search(txt.begin(), txt.end(), flt.begin(), flt.end(),
                [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }
            ) != txt.end();
            if (!found) continue;
        }
        g.cachedVisibleIndices.push_back((int)i);
    }

    g.cachedSearchFilter = g.searchFilter;
    g.cachedCompletedMode = g.completedMode;
    g.cacheDirty = false;
}
