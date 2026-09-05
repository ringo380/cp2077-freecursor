#include <RED4ext/RED4ext.hpp>

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "window_hook.h"

namespace
{
const RED4ext::v1::Sdk*   s_sdk    = nullptr;
RED4ext::v1::PluginHandle s_handle = nullptr;

// input::InputSystemWin32Base::ForceCursor(engineUnkD0, reason, enabled)
using ForceCursor_t = void (*)(RED4ext::CBaseEngine::UnkD0*, RED4ext::CName, bool);
constexpr std::uint32_t kForceCursorHash = 2130646213UL;

// Distinct from CET's "ImGui" reason: the parameter is a refcount key, so a
// shared name would make the two mods stomp on each other's state.
const RED4ext::CName kReason = "FreeCursorMod";

// Two name arrays inside the input system decide the OS pointer. The game's
// WM_SETCURSOR handler hides the pointer only when the second array is
// non-empty and the first is empty:
//   +0x158  reasons forcing the pointer visible (what ForceCursor edits)
//   +0x168  reasons keeping it hidden (gameplay holds one, popups drop it)
// Layout is the engine's dynamic array: entries, capacity, size.
struct NameArray
{
    RED4ext::CName* entries;
    std::uint32_t   capacity;
    std::uint32_t   size;
};
constexpr std::size_t kForcedReasonsOffset = 0x158;
constexpr std::size_t kHideReasonsOffset   = 0x168;

// input::InputSystemWin32Base: insert a hide reason (no-op if present) and
// erase one. Same calling shape as ForceCursor minus the bool.
using HideCursor_t = void (*)(RED4ext::CBaseEngine::UnkD0*, RED4ext::CName);
constexpr std::uint32_t kHideCursorHash = 1013978019UL;

constexpr std::size_t kMaxReasons = 8;

// Copy up to kMaxReasons names out of an array. The samples run on a worker
// thread while the game may be resizing the array, so the copy is guarded
// and a fault just reports nothing rather than taking the game down.
std::uint32_t CopyReasons(const NameArray* aArray, RED4ext::CName* aOut, std::uint32_t* aTotal) noexcept
{
    __try
    {
        const std::uint32_t total = aArray->size;
        *aTotal                   = total;
        const std::uint32_t n     = total < kMaxReasons ? total : kMaxReasons;
        for (std::uint32_t i = 0; i < n; ++i)
            aOut[i] = aArray->entries[i];
        return n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        *aTotal = 0;
        return 0;
    }
}

const NameArray* ReasonArray(RED4ext::CBaseEngine::UnkD0* aInput, std::size_t aOffset)
{
    return reinterpret_cast<const NameArray*>(reinterpret_cast<std::uint8_t*>(aInput) + aOffset);
}

// "[a,b]" for the log; "[?]" when the array could not be read.
void DescribeReasons(RED4ext::CBaseEngine::UnkD0* aInput, std::size_t aOffset, char* aBuf, std::size_t aBufSize)
{
    RED4ext::CName names[kMaxReasons];
    std::uint32_t  total = 0;
    const auto     n     = CopyReasons(ReasonArray(aInput, aOffset), names, &total);
    std::size_t    pos   = 0;
    auto put = [&](const char* aText)
    {
        const std::size_t len = std::strlen(aText);
        if (pos + len + 1 >= aBufSize)
            return;
        std::memcpy(aBuf + pos, aText, len);
        pos += len;
        aBuf[pos] = 0;
    };
    aBuf[0] = 0;
    put("[");
    for (std::uint32_t i = 0; i < n; ++i)
    {
        if (i)
            put(",");
        const char* text = names[i].ToString();
        put(text ? text : "?");
    }
    if (total > n)
        put(",...");
    put("]");
}

// The hide reasons seen the last time the pointer was legitimately hidden
// (hide array non-empty, forced array empty). Recorded on the main thread
// only, before our own ForceCursor calls; read back by RestoreCursorLock.
RED4ext::CName s_lastHideReasons[kMaxReasons];
std::uint32_t  s_lastHideCount = 0;

void RememberHideReasons(RED4ext::CBaseEngine::UnkD0* aInput)
{
    RED4ext::CName names[kMaxReasons];
    std::uint32_t  hideTotal = 0;
    const auto     n         = CopyReasons(ReasonArray(aInput, kHideReasonsOffset), names, &hideTotal);
    std::uint32_t  forcedTotal = 0;
    RED4ext::CName scratch[kMaxReasons];
    CopyReasons(ReasonArray(aInput, kForcedReasonsOffset), scratch, &forcedTotal);
    if (n == 0 || forcedTotal != 0)
        return;
    for (std::uint32_t i = 0; i < n; ++i)
        s_lastHideReasons[i] = names[i];
    s_lastHideCount = n;
}

// Mirrors RED4ext::UniversalRelocBase::Resolve, but returns nullptr on
// failure instead of calling ShowErrorAndTerminateProcess. A game patch that
// shifts the ForceCursor address must make the mod degrade gracefully
// ("does nothing and says so"), not hard-crash the game.
std::uintptr_t ResolveGameAddress(std::uint32_t aHash)
{
    using ResolveFunc_t = std::uintptr_t (*)(std::uint32_t);

    const HMODULE red4extModule = GetModuleHandleW(L"RED4ext.dll");
    if (!red4extModule)
    {
        if (s_sdk)
            s_sdk->logger->Error(s_handle, "ForceCursor: could not find RED4ext.dll module");
        return 0;
    }

    const auto resolveFunc =
        reinterpret_cast<ResolveFunc_t>(GetProcAddress(red4extModule, "RED4ext_ResolveAddress"));
    if (!resolveFunc)
    {
        if (s_sdk)
            s_sdk->logger->Error(s_handle, "ForceCursor: could not find RED4ext_ResolveAddress export");
        return 0;
    }

    const std::uintptr_t address = resolveFunc(aHash);
    if (address == 0 && s_sdk)
        s_sdk->logger->ErrorF(s_handle, "address hash %u did not resolve (game patch?)", aHash);
    return address;
}

ForceCursor_t ResolveForceCursor()
{
    return reinterpret_cast<ForceCursor_t>(ResolveGameAddress(kForceCursorHash));
}

HideCursor_t ResolveHideCursor()
{
    return reinterpret_cast<HideCursor_t>(ResolveGameAddress(kHideCursorHash));
}

// Last value successfully handed to ForceCursor for kReason. The reason
// parameter is a refcount key, so repeat-arming the same value would push the
// count up N times and need N disarms -- and GameUI.Observe re-runs apply() on
// every HUD/menu transition while detached. Starts false because that is the
// game's state before we ever call: a clear on a never-armed reason must be a
// no-op, not an underflow. Only ever touched from the script VM thread.
bool s_cursorForced = false;

// Diagnostic: what the OS pointer is actually doing around a ForceCursor call.
// The log otherwise records only what the mod asked for, never whether the
// pointer hid. CURSORINFO is process-wide, so it can be read from any thread;
// the foreground check says whether the game even owns the pointer right now.
void LogCursorSample(const char* aWhen)
{
    if (!s_sdk)
        return;
    CURSORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetCursorInfo(&info))
    {
        s_sdk->logger->InfoF(s_handle, "cursor %s: GetCursorInfo failed (%lu)", aWhen, GetLastError());
        return;
    }
    const HWND fg = GetForegroundWindow();
    DWORD      fgPid = 0;
    if (fg)
        GetWindowThreadProcessId(fg, &fgPid);
    char forced[160] = "[?]";
    char hide[160]   = "[?]";
    if (auto* engine = RED4ext::CGameEngine::Get(); engine && engine->unkD0)
    {
        DescribeReasons(engine->unkD0, kForcedReasonsOffset, forced, sizeof(forced));
        DescribeReasons(engine->unkD0, kHideReasonsOffset, hide, sizeof(hide));
    }
    s_sdk->logger->InfoF(s_handle, "cursor %s: showing=%d hCursor=%p pos=(%ld,%ld) gameForeground=%d forced=%s hide=%s",
                         aWhen, (info.flags & CURSOR_SHOWING) ? 1 : 0, static_cast<void*>(info.hCursor),
                         info.ptScreenPos.x, info.ptScreenPos.y, fgPid == GetCurrentProcessId() ? 1 : 0, forced,
                         hide);
}

// Two delayed samples after a state change. The game may hide or show the
// pointer some frames after ForceCursor returns, so a sample taken on the
// calling thread alone would only ever see the old state.
void ScheduleCursorSamples(bool aEnabled)
{
    std::thread(
        [aEnabled]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            LogCursorSample(aEnabled ? "100ms after force(true)" : "100ms after force(false)");
            std::this_thread::sleep_for(std::chrono::milliseconds(900));
            LogCursorSample(aEnabled ? "1s after force(true)" : "1s after force(false)");
        })
        .detach();
}

bool ApplyCursorForced(bool aEnabled)
{
    static ForceCursor_t forceCursor = ResolveForceCursor();
    if (!forceCursor)
        return false;

    auto* engine = RED4ext::CGameEngine::Get();
    if (!engine || !engine->unkD0)
    {
        if (s_sdk)
            s_sdk->logger->Error(s_handle, "ForceCursor: game engine not ready");
        return false;
    }

    // Dedup AFTER the resolve/engine-ready checks, so a genuine failure still
    // reports false rather than being masked by a cache hit.
    if (aEnabled == s_cursorForced)
        return true;

    RememberHideReasons(engine->unkD0);
    LogCursorSample(aEnabled ? "before force(true)" : "before force(false)");
    forceCursor(engine->unkD0, kReason, aEnabled);
    // Only after the call actually executed -- a failed apply must not poison
    // the cache.
    s_cursorForced = aEnabled;

    if (s_sdk)
        s_sdk->logger->InfoF(s_handle, "ForceCursor(%s) applied", aEnabled ? "true" : "false");
    LogCursorSample(aEnabled ? "right after force(true)" : "right after force(false)");
    ScheduleCursorSamples(aEnabled);

    return true;
}

// After a gameplay reattach: if nobody forces the pointer and nobody hides it
// either, the game dropped its own lock while we were detached (seen when a
// popup closed under a detach). Put back the reasons last seen holding it.
// Returns false when there is nothing safe to do; every branch is logged.
bool RestoreCursorLock()
{
    static HideCursor_t hideCursor = ResolveHideCursor();
    if (!hideCursor)
        return false;

    auto* engine = RED4ext::CGameEngine::Get();
    if (!engine || !engine->unkD0)
        return false;

    char forced[160] = "[?]";
    char hide[160]   = "[?]";
    DescribeReasons(engine->unkD0, kForcedReasonsOffset, forced, sizeof(forced));
    DescribeReasons(engine->unkD0, kHideReasonsOffset, hide, sizeof(hide));

    RED4ext::CName scratch[kMaxReasons];
    std::uint32_t  forcedTotal = 0;
    std::uint32_t  hideTotal   = 0;
    CopyReasons(ReasonArray(engine->unkD0, kForcedReasonsOffset), scratch, &forcedTotal);
    CopyReasons(ReasonArray(engine->unkD0, kHideReasonsOffset), scratch, &hideTotal);

    if (hideTotal != 0)
        return true; // the game still holds its lock; nothing to restore

    if (forcedTotal != 0)
    {
        if (s_sdk)
            s_sdk->logger->WarnF(s_handle, "RestoreCursorLock: pointer still forced by %s; not touching it", forced);
        return false;
    }

    if (s_lastHideCount == 0)
    {
        if (s_sdk)
            s_sdk->logger->Warn(s_handle, "RestoreCursorLock: hide array is empty and no lock reason has been seen yet");
        return false;
    }

    for (std::uint32_t i = 0; i < s_lastHideCount; ++i)
        hideCursor(engine->unkD0, s_lastHideReasons[i]);

    DescribeReasons(engine->unkD0, kHideReasonsOffset, hide, sizeof(hide));
    if (s_sdk)
        s_sdk->logger->InfoF(s_handle, "RestoreCursorLock: hide array was empty after reattach; restored %s", hide);
    return true;
}
} // namespace

void FreeCursor_SetCursorForced(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, bool* aOut,
                               int64_t a4)
{
    RED4EXT_UNUSED_PARAMETER(aContext);
    RED4EXT_UNUSED_PARAMETER(a4);

    bool enabled = false;
    RED4ext::GetParameter(aFrame, &enabled);
    aFrame->code++; // skip ParamEnd - omitting this corrupts the script VM

    const bool ok = ApplyCursorForced(enabled);

    // Success is logged inside ApplyCursorForced, and only on an actual state
    // transition -- logging here would spam the RED4ext log on every one of the
    // many no-op calls GameUI.Observe drives while detached.
    if (!ok && s_sdk)
        s_sdk->logger->ErrorF(s_handle, "SetCursorForced(%s) FAILED", enabled ? "true" : "false");

    if (aOut)
        *aOut = ok;
}

void FreeCursor_SetInputSwallow(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, bool* aOut,
                                int64_t a4)
{
    RED4EXT_UNUSED_PARAMETER(aContext);
    RED4EXT_UNUSED_PARAMETER(a4);

    bool enabled = false;
    RED4ext::GetParameter(aFrame, &enabled);
    aFrame->code++; // skip ParamEnd - omitting this corrupts the script VM

    const bool ok = freecursor::WindowHook::SetSwallow(enabled);

    if (!ok && s_sdk)
        s_sdk->logger->Error(s_handle, "SetInputSwallow: window hook not installed");

    if (aOut)
        *aOut = ok;
}

void FreeCursor_SetWheelBlock(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, bool* aOut,
                              int64_t a4)
{
    RED4EXT_UNUSED_PARAMETER(aContext);
    RED4EXT_UNUSED_PARAMETER(a4);

    bool enabled = false;
    RED4ext::GetParameter(aFrame, &enabled);
    aFrame->code++; // skip ParamEnd - omitting this corrupts the script VM

    const bool ok = freecursor::WindowHook::SetWheelBlock(enabled);

    if (!ok && s_sdk)
        s_sdk->logger->Error(s_handle, "SetWheelBlock: window hook not installed");

    if (aOut)
        *aOut = ok;
}

void FreeCursor_RestoreCursorLock(RED4ext::IScriptable* aContext, RED4ext::CStackFrame* aFrame, bool* aOut,
                                  int64_t a4)
{
    RED4EXT_UNUSED_PARAMETER(aContext);
    RED4EXT_UNUSED_PARAMETER(a4);

    aFrame->code++; // skip ParamEnd - omitting this corrupts the script VM

    const bool ok = RestoreCursorLock();
    if (aOut)
        *aOut = ok;
}

void PostRegisterTypes()
{
    auto* rtti = RED4ext::CRTTISystem::Get();

    auto* func = RED4ext::CGlobalFunction::Create("FreeCursor_SetCursorForced", "FreeCursor_SetCursorForced",
                                                 &FreeCursor_SetCursorForced);
    func->flags = {.isNative = true, .isStatic = true};
    func->AddParam("Bool", "enabled");
    func->SetReturnType("Bool");
    rtti->RegisterFunction(func);

    auto* swallowFunc = RED4ext::CGlobalFunction::Create(
        "FreeCursor_SetInputSwallow", "FreeCursor_SetInputSwallow", &FreeCursor_SetInputSwallow);
    swallowFunc->flags = {.isNative = true, .isStatic = true};
    swallowFunc->AddParam("Bool", "enabled");
    swallowFunc->SetReturnType("Bool");
    rtti->RegisterFunction(swallowFunc);

    auto* wheelBlockFunc = RED4ext::CGlobalFunction::Create(
        "FreeCursor_SetWheelBlock", "FreeCursor_SetWheelBlock", &FreeCursor_SetWheelBlock);
    wheelBlockFunc->flags = {.isNative = true, .isStatic = true};
    wheelBlockFunc->AddParam("Bool", "enabled");
    wheelBlockFunc->SetReturnType("Bool");
    rtti->RegisterFunction(wheelBlockFunc);

    auto* restoreFunc = RED4ext::CGlobalFunction::Create(
        "FreeCursor_RestoreCursorLock", "FreeCursor_RestoreCursorLock", &FreeCursor_RestoreCursorLock);
    restoreFunc->flags = {.isNative = true, .isStatic = true};
    restoreFunc->SetReturnType("Bool");
    rtti->RegisterFunction(restoreFunc);
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
        s_sdk    = aSdk;
        s_handle = aHandle;
        aSdk->logger->Info(aHandle, "FreeCursor loaded");
        RED4ext::CRTTISystem::Get()->AddPostRegisterCallback(PostRegisterTypes);
        freecursor::WindowHook::Install(
            [](const char* aMessage)
            {
                if (s_sdk)
                    s_sdk->logger->Info(s_handle, aMessage);
            });
        break;

    case RED4ext::v1::EMainReason::Unload:
        freecursor::WindowHook::Uninstall();
        break;
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name    = L"FreeCursor";
    aInfo->author  = L"ringo";
    aInfo->version = RED4EXT_V1_SEMVER(0, 5, 3);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_2_31;
    aInfo->sdk     = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
