#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include "imgui.h"

/* ── Enums ─────────────────────────────────────────────────────────────────── */

enum RepeatType    { Repeat_Daily = 0, Repeat_Weekly = 1 };
enum CompletedMode { CompletedMode_Colour = 0, CompletedMode_Hide = 1 };

/* ── Constants ─────────────────────────────────────────────────────────────── */

extern const char* ADDON_NAME;
extern const char* KB_TOGGLE;
extern const char* QA_ID;
extern const char* QA_ICON_ID;
extern const char* QA_ICON_FILENAME;

static constexpr float MIN_WINDOW_DIM    = 200.f;
static constexpr float DEFAULT_WINDOW_W  = 400.f;
static constexpr float DEFAULT_WINDOW_H  = 480.f;
static constexpr double RESET_CHECK_INTERVAL = 60.0;
static constexpr double DIRTY_SAVE_DELAY     = 2.0;
static constexpr double FILE_POLL_INTERVAL   = 1.0;

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

    /* Cached computation — rebuilt only on data/filter change */
    bool                  cacheDirty        = true;
    std::vector<int>      cachedVisibleIndices;
    std::string           cachedSearchFilter;
    int                   cachedCompletedMode = -1;
    int                   cachedDailyTotal = 0, cachedDailyDone = 0;
    int                   cachedWeeklyTotal = 0, cachedWeeklyDone = 0;
    int                   cachedTotal = 0, cachedDone = 0;
    std::string           cachedConfigPath;

    float                 winX = 0.f, winY = 0.f;
    float                 winW = DEFAULT_WINDOW_W, winH = DEFAULT_WINDOW_H;
    bool                  winGeometryLoaded = false;
    bool                  lockPosition      = false;
    bool                  lockSize          = false;

    bool                  showQuickAccess   = true;
    bool                  openOnLaunch      = false;
    bool                  collapseEnabled   = false;
    float                 collapseDelaySec  = 2.0f;
    bool                  collapsed         = false;
    double                lastHoverTime     = 0.0;

    FILETIME              lastFileWriteTime = {};
    double                lastFilePollTime  = 0.0;

    bool                  dirty          = false;
    double                dirtyTimestamp  = 0.0;
};

extern AppState g;

/* ── Public API ────────────────────────────────────────────────────────────── */

std::string GetConfigPath(const char* filename);
FILETIME    GetFileModTime(const std::string& path);
std::string TrimWhitespace(const std::string& s);
std::string GetCurrentUtcDate();
std::string GetThisMondayDate();

void InvalidateCache();
void MarkDirty();
void SaveTodos();
void FlushIfDirty();
void LoadTodos();

void LoadWindowGeometry();
void SaveWindowGeometry();

void CheckResetTimes();
void RebuildCache();

void AddNewTodo();
int  IndexForUid(const std::string& uid);
void MoveTodo(int fromIdx, int toIdx);
