#include <RED4ext/RED4ext.hpp>

#include <Windows.h>

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

// Mirrors RED4ext::UniversalRelocBase::Resolve, but returns nullptr on
// failure instead of calling ShowErrorAndTerminateProcess. A game patch that
// shifts the ForceCursor address must make the mod degrade gracefully
// ("does nothing and says so"), not hard-crash the game.
ForceCursor_t ResolveForceCursor()
{
    using ResolveFunc_t = std::uintptr_t (*)(std::uint32_t);

    const HMODULE red4extModule = GetModuleHandleW(L"RED4ext.dll");
    if (!red4extModule)
    {
        if (s_sdk)
            s_sdk->logger->Error(s_handle, "ForceCursor: could not find RED4ext.dll module");
        return nullptr;
    }

    const auto resolveFunc =
        reinterpret_cast<ResolveFunc_t>(GetProcAddress(red4extModule, "RED4ext_ResolveAddress"));
    if (!resolveFunc)
    {
        if (s_sdk)
            s_sdk->logger->Error(s_handle, "ForceCursor: could not find RED4ext_ResolveAddress export");
        return nullptr;
    }

    const std::uintptr_t address = resolveFunc(kForceCursorHash);
    if (address == 0)
    {
        if (s_sdk)
            s_sdk->logger->Error(s_handle, "ForceCursor: address hash did not resolve (game patch?)");
        return nullptr;
    }

    return reinterpret_cast<ForceCursor_t>(address);
}

// Last value successfully handed to ForceCursor for kReason. The reason
// parameter is a refcount key, so repeat-arming the same value would push the
// count up N times and need N disarms -- and GameUI.Observe re-runs apply() on
// every HUD/menu transition while detached. Starts false because that is the
// game's state before we ever call: a clear on a never-armed reason must be a
// no-op, not an underflow. Only ever touched from the script VM thread.
bool s_cursorForced = false;

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

    forceCursor(engine->unkD0, kReason, aEnabled);
    // Only after the call actually executed -- a failed apply must not poison
    // the cache.
    s_cursorForced = aEnabled;

    if (s_sdk)
        s_sdk->logger->InfoF(s_handle, "ForceCursor(%s) applied", aEnabled ? "true" : "false");

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
    aInfo->version = RED4EXT_V1_SEMVER(0, 4, 0);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_2_31;
    aInfo->sdk     = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
