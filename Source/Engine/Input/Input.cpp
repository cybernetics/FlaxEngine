// Copyright (c) 2012-2020 Wojciech Figat. All rights reserved.

#include "Input.h"
#include "InputSettings.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Gamepad.h"
#include "FlaxEngine.Gen.h"
#include "Engine/Platform/Window.h"
#include "Engine/Engine/Engine.h"
#include "Engine/Engine/EngineService.h"
#include "Engine/Engine/Screen.h"
#include "Engine/Engine/Time.h"
#include "Engine/Platform/WindowsManager.h"
#include "Engine/Scripting/ScriptingType.h"
#include "Engine/Scripting/BinaryModule.h"
#include "Engine/Profiler/ProfilerCPU.h"

struct AxisEvaluation
{
    float RawValue;
    float Value;
    float PrevKeyValue;
    bool Used;
};

struct ActionData
{
    bool Active;
    uint64 FrameIndex;

    ActionData()
    {
        Active = false;
        FrameIndex = 0;
    }
};

struct AxisData
{
    float Value;
    float ValueRaw;
    float PrevKeyValue;
    uint64 FrameIndex;

    AxisData()
    {
        Value = 0.0f;
        ValueRaw = 0.0f;
        PrevKeyValue = 0.0f;
        FrameIndex = 0;
    }
};

namespace InputImpl
{
    Dictionary<StringView, ActionData> Actions;
    Dictionary<StringView, AxisData> Axes;
    bool GamepadsChanged = true;
    Array<AxisEvaluation> AxesValues;
    InputDevice::EventQueue InputEvents;
}

using namespace InputImpl;

class InputService : public EngineService
{
public:

    InputService()
        : EngineService(TEXT("Input"), -60)
    {
    }

    void Update() override;
};

InputService InputServiceInstance;

Mouse* Input::Mouse = nullptr;
Keyboard* Input::Keyboard = nullptr;
Array<Gamepad*, FixedAllocation<MAX_GAMEPADS>> Input::Gamepads;
Action Input::GamepadsChanged;
Array<InputDevice*, InlinedAllocation<16>> Input::CustomDevices;
Input::CharDelegate Input::CharInput;
Input::KeyboardDelegate Input::KeyDown;
Input::KeyboardDelegate Input::KeyUp;
Input::MouseButtonDelegate Input::MouseDown;
Input::MouseButtonDelegate Input::MouseUp;
Input::MouseButtonDelegate Input::MouseDoubleClick;
Input::MouseWheelDelegate Input::MouseWheel;
Input::MouseDelegate Input::MouseMove;
Action Input::MouseLeave;
Input::TouchDelegate Input::TouchDown;
Input::TouchDelegate Input::TouchMove;
Input::TouchDelegate Input::TouchUp;
Delegate<StringView> Input::ActionTriggered;
Array<ActionConfig> Input::ActionMappings;
Array<AxisConfig> Input::AxisMappings;

int32 Input::GetGamepadsCount()
{
    return Gamepads.Count();
}

Gamepad* Input::GetGamepad(int32 index)
{
    if (index >= 0 && index < Gamepads.Count())
        return Gamepads[index];
    return nullptr;
}

void Input::OnGamepadsChanged()
{
    ::GamepadsChanged = true;
}

StringView Input::GetInputText()
{
    return Keyboard ? Keyboard->GetInputText() : StringView::Empty;
}

bool Input::GetKey(const KeyboardKeys key)
{
    return Keyboard ? Keyboard->GetKey(key) : false;
}

bool Input::GetKeyDown(const KeyboardKeys key)
{
    return Keyboard ? Keyboard->GetKeyDown(key) : false;
}

bool Input::GetKeyUp(const KeyboardKeys key)
{
    return Keyboard ? Keyboard->GetKeyUp(key) : false;
}

Vector2 Input::GetMousePosition()
{
    return Mouse ? Engine::ScreenToGameViewport(Mouse->GetPosition()) : Vector2::Minimum;
}

void Input::SetMousePosition(const Vector2& position)
{
    if (Mouse && Engine::HasGameViewportFocus())
    {
        const auto pos = Engine::GameViewportToScreen(position);
        if (pos > Vector2::Minimum)
            Mouse->SetMousePosition(pos);
    }
}

Vector2 Input::GetMouseScreenPosition()
{
    return Mouse ? Mouse->GetPosition() : Vector2::Minimum;
}

void Input::SetMouseScreenPosition(const Vector2& position)
{
    if (Mouse && Engine::HasFocus)
    {
        Mouse->SetMousePosition(position);
    }
}

Vector2 Input::GetMousePositionDelta()
{
    return Mouse ? Mouse->GetPositionDelta() : Vector2::Zero;
}

float Input::GetMouseScrollDelta()
{
    return Mouse ? Mouse->GetScrollDelta() : 0.0f;
}

bool Input::GetMouseButton(const MouseButton button)
{
    return Mouse ? Mouse->GetButton(button) : false;
}

bool Input::GetMouseButtonDown(const MouseButton button)
{
    return Mouse ? Mouse->GetButtonDown(button) : false;
}

bool Input::GetMouseButtonUp(const MouseButton button)
{
    return Mouse ? Mouse->GetButtonUp(button) : false;
}

float Input::GetGamepadAxis(int32 gamepadIndex, GamepadAxis axis)
{
    if (gamepadIndex >= 0 && gamepadIndex < Gamepads.Count())
        return Gamepads[gamepadIndex]->GetAxis(axis);
    return 0.0f;
}

bool Input::GetGamepadButton(int32 gamepadIndex, GamepadButton button)
{
    if (gamepadIndex >= 0 && gamepadIndex < Gamepads.Count())
        return Gamepads[gamepadIndex]->GetButton(button);
    return false;
}

bool Input::GetGamepadButtonDown(int32 gamepadIndex, GamepadButton button)
{
    if (gamepadIndex >= 0 && gamepadIndex < Gamepads.Count())
        return Gamepads[gamepadIndex]->GetButtonDown(button);
    return false;
}

bool Input::GetGamepadButtonUp(int32 gamepadIndex, GamepadButton button)
{
    if (gamepadIndex >= 0 && gamepadIndex < Gamepads.Count())
        return Gamepads[gamepadIndex]->GetButtonUp(button);
    return false;
}

float Input::GetGamepadAxis(InputGamepadIndex gamepad, GamepadAxis axis)
{
    if (gamepad == InputGamepadIndex::All)
    {
        for (auto g : Gamepads)
        {
            if (g->GetAxis(axis))
                return true;
        }
    }
    else
    {
        const auto index = static_cast<int32>(gamepad);
        if (index < Gamepads.Count())
            return Gamepads[index]->GetAxis(axis);
    }
    return false;
}

bool Input::GetGamepadButton(InputGamepadIndex gamepad, GamepadButton button)
{
    if (gamepad == InputGamepadIndex::All)
    {
        for (auto g : Gamepads)
        {
            if (g->GetButton(button))
                return true;
        }
    }
    else
    {
        const auto index = static_cast<int32>(gamepad);
        if (index < Gamepads.Count())
            return Gamepads[index]->GetButton(button);
    }
    return false;
}

bool Input::GetGamepadButtonDown(InputGamepadIndex gamepad, GamepadButton button)
{
    if (gamepad == InputGamepadIndex::All)
    {
        for (auto g : Gamepads)
        {
            if (g->GetButtonDown(button))
                return true;
        }
    }
    else
    {
        const auto index = static_cast<int32>(gamepad);
        if (index < Gamepads.Count())
            return Gamepads[index]->GetButtonDown(button);
    }
    return false;
}

bool Input::GetGamepadButtonUp(InputGamepadIndex gamepad, GamepadButton button)
{
    if (gamepad == InputGamepadIndex::All)
    {
        for (auto g : Gamepads)
        {
            if (g->GetButtonUp(button))
                return true;
        }
    }
    else
    {
        const auto index = static_cast<int32>(gamepad);
        if (index < Gamepads.Count())
            return Gamepads[index]->GetButtonUp(button);
    }
    return false;
}

bool Input::GetAction(const StringView& name)
{
    const auto e = Actions.TryGet(name);
    return e ? e->Active : false;
}

float Input::GetAxis(const StringView& name)
{
    const auto e = Axes.TryGet(name);
    return e ? e->Value : false;
}

float Input::GetAxisRaw(const StringView& name)
{
    const auto e = Axes.TryGet(name);
    return e ? e->ValueRaw : false;
}

void InputService::Update()
{
    PROFILE_CPU();
    const auto frame = Time::Update.TicksCount;
    const auto dt = Time::Update.UnscaledDeltaTime.GetTotalSeconds();
    InputEvents.Clear();

    // If application has no user focus then simply clear the state
    if (!Engine::HasFocus)
    {
        if (Input::Mouse)
            Input::Mouse->ResetState();
        if (Input::Keyboard)
            Input::Keyboard->ResetState();
        for (int32 i = 0; i < Input::Gamepads.Count(); i++)
            Input::Gamepads[i]->ResetState();
        Axes.Clear();
        Actions.Clear();
        return;
    }

    // Update input devices state
    if (Input::Mouse)
    {
        if (Input::Mouse->Update(InputEvents))
        {
            Input::Mouse->DeleteObject();
            Input::Mouse = nullptr;
        }
    }
    if (Input::Keyboard)
    {
        if (Input::Keyboard->Update(InputEvents))
        {
            Input::Keyboard->DeleteObject();
            Input::Keyboard = nullptr;
        }
    }
    for (int32 i = 0; i < Input::Gamepads.Count(); i++)
    {
        if (Input::Gamepads[i]->Update(InputEvents))
        {
            Input::Gamepads[i]->DeleteObject();
            Input::Gamepads.RemoveAtKeepOrder(i);
            Input::OnGamepadsChanged();
            i--;
            if (Input::Gamepads.IsEmpty())
                break;
        }
    }
    for (int32 i = 0; i < Input::CustomDevices.Count(); i++)
    {
        if (Input::CustomDevices[i]->Update(InputEvents))
        {
            Input::CustomDevices[i]->DeleteObject();
            Input::CustomDevices.RemoveAtKeepOrder(i);
            i--;
            if (Input::CustomDevices.IsEmpty())
                break;
        }
    }

    // Send gamepads change events
    if (GamepadsChanged)
    {
        GamepadsChanged = false;
        Input::GamepadsChanged();
    }

    // Pick the first focused window for input events
    WindowsManager::WindowsLocker.Lock();
    Window* defaultWindow = nullptr;
    for (auto window : WindowsManager::Windows)
    {
        if (window->IsFocused() && window->GetSettings().AllowInput)
        {
            defaultWindow = window;
            break;
        }
    }
    WindowsManager::WindowsLocker.Unlock();

    // Send input events for the focused window
    WindowsManager::WindowsLocker.Lock();
    for (const auto& e : InputEvents)
    {
        auto window = e.Target ? e.Target : defaultWindow;
        if (!window)
            continue;
        switch (e.Type)
        {
            // Keyboard events
        case InputDevice::EventType::Char:
            window->OnCharInput(e.CharData.Char);
            break;
        case InputDevice::EventType::KeyDown:
            window->OnKeyDown(e.KeyData.Key);
            break;
        case InputDevice::EventType::KeyUp:
            window->OnKeyUp(e.KeyData.Key);
            break;
            // Mouse events
        case InputDevice::EventType::MouseDown:
            window->OnMouseDown(window->ScreenToClient(e.MouseData.Position), e.MouseData.Button);
            break;
        case InputDevice::EventType::MouseUp:
            window->OnMouseUp(window->ScreenToClient(e.MouseData.Position), e.MouseData.Button);
            break;
        case InputDevice::EventType::MouseDoubleClick:
            window->OnMouseDoubleClick(window->ScreenToClient(e.MouseData.Position), e.MouseData.Button);
            break;
        case InputDevice::EventType::MouseWheel:
            window->OnMouseWheel(window->ScreenToClient(e.MouseWheelData.Position), e.MouseWheelData.WheelDelta);
            break;
        case InputDevice::EventType::MouseMove:
            window->OnMouseMove(window->ScreenToClient(e.MouseData.Position));
            break;
        case InputDevice::EventType::MouseLeave:
            window->OnMouseLeave();
            break;
            // Touch events
        case InputDevice::EventType::TouchDown:
            window->OnTouchDown(window->ScreenToClient(e.TouchData.Position), e.TouchData.PointerId);
            break;
        case InputDevice::EventType::TouchMove:
            window->OnTouchMove(window->ScreenToClient(e.TouchData.Position), e.TouchData.PointerId);
            break;
        case InputDevice::EventType::TouchUp:
            window->OnTouchUp(window->ScreenToClient(e.TouchData.Position), e.TouchData.PointerId);
            break;
        }
    }
    WindowsManager::WindowsLocker.Unlock();

    // Skip if game has no focus to handle the input
    if (!Engine::HasGameViewportFocus())
    {
        Axes.Clear();
        Actions.Clear();
        return;
    }

    // Send input events
    for (const auto& e : InputEvents)
    {
        switch (e.Type)
        {
            // Keyboard events
        case InputDevice::EventType::Char:
            Input::CharInput(e.CharData.Char);
            break;
        case InputDevice::EventType::KeyDown:
            Input::KeyDown(e.KeyData.Key);
            break;
        case InputDevice::EventType::KeyUp:
            Input::KeyUp(e.KeyData.Key);
            break;
            // Mouse events
        case InputDevice::EventType::MouseDown:
            Input::MouseDown(e.MouseData.Position, e.MouseData.Button);
            break;
        case InputDevice::EventType::MouseUp:
            Input::MouseUp(e.MouseData.Position, e.MouseData.Button);
            break;
        case InputDevice::EventType::MouseDoubleClick:
            Input::MouseDoubleClick(e.MouseData.Position, e.MouseData.Button);
            break;
        case InputDevice::EventType::MouseWheel:
            Input::MouseWheel(e.MouseWheelData.Position, e.MouseWheelData.WheelDelta);
            break;
        case InputDevice::EventType::MouseMove:
            Input::MouseMove(e.MouseData.Position);
            break;
        case InputDevice::EventType::MouseLeave:
            Input::MouseLeave();
            break;
            // Touch events
        case InputDevice::EventType::TouchDown:
            Input::TouchDown(e.TouchData.Position, e.TouchData.PointerId);
            break;
        case InputDevice::EventType::TouchMove:
            Input::TouchMove(e.TouchData.Position, e.TouchData.PointerId);
            break;
        case InputDevice::EventType::TouchUp:
            Input::TouchUp(e.TouchData.Position, e.TouchData.PointerId);
            break;
        }
    }

    // Update all actions
    for (int32 i = 0; i < Input::ActionMappings.Count(); i++)
    {
        const auto& config = Input::ActionMappings[i];
        const StringView name = config.Name;
        ActionData& data = Actions[name];

        data.Active = false;

        // Mark as updated in this frame
        data.FrameIndex = frame;
    }
    for (int32 i = 0; i < Input::ActionMappings.Count(); i++)
    {
        const auto& config = Input::ActionMappings[i];
        const StringView name = config.Name;
        ActionData& data = Actions[name];

        bool isActive;
        if (config.Mode == InputActionMode::Pressing)
        {
            isActive = Input::GetKey(config.Key) || Input::GetMouseButton(config.MouseButton) || Input::GetGamepadButton(config.Gamepad, config.GamepadButton);
        }
        else if (config.Mode == InputActionMode::Press)
        {
            isActive = Input::GetKeyDown(config.Key) || Input::GetMouseButtonDown(config.MouseButton) || Input::GetGamepadButtonDown(config.Gamepad, config.GamepadButton);
        }
        else
        {
            isActive = Input::GetKeyUp(config.Key) || Input::GetMouseButtonUp(config.MouseButton) || Input::GetGamepadButtonUp(config.Gamepad, config.GamepadButton);
        }

        data.Active |= isActive;
    }

    // Update all axes
    AxesValues.Resize(Input::AxisMappings.Count(), false);
    for (int32 i = 0; i < Input::AxisMappings.Count(); i++)
    {
        const auto& config = Input::AxisMappings[i];
        const StringView name = config.Name;
        const AxisData& data = Axes[name];

        // Get key raw value
        const bool isPositiveKey = Input::GetKey(config.PositiveButton);
        const bool isNegativeKey = Input::GetKey(config.NegativeButton);
        float keyRawValue = 0;
        if (isPositiveKey && !isNegativeKey)
        {
            keyRawValue = 1;
        }
        else if (!isPositiveKey && isNegativeKey)
        {
            keyRawValue = -1;
        }

        // Apply keyboard curve smoothing and snapping
        float prevKeyValue = data.PrevKeyValue;
        if (config.Snap && Math::NotSameSign(data.PrevKeyValue, keyRawValue))
        {
            prevKeyValue = 0;
        }
        float keyValue;
        if (Math::Abs(prevKeyValue) <= Math::Abs(keyRawValue))
        {
            keyValue = Math::LerpStable(prevKeyValue, keyRawValue, Math::Saturate(dt * config.Sensitivity));
        }
        else
        {
            keyValue = Math::LerpStable(prevKeyValue, keyRawValue, Math::Saturate(dt * config.Gravity));
        }

        // Get axis raw value
        float axisRawValue;
        switch (config.Axis)
        {
        case InputAxisType::MouseX:
            axisRawValue = Input::GetMousePositionDelta().X * config.Sensitivity;
            break;
        case InputAxisType::MouseY:
            axisRawValue = Input::GetMousePositionDelta().Y * config.Sensitivity;
            break;
        case InputAxisType::MouseWheel:
            axisRawValue = Input::GetMouseScrollDelta() * config.Sensitivity;
            break;
        case InputAxisType::GamepadLeftStickX:
            axisRawValue = Input::GetGamepadAxis(config.Gamepad, GamepadAxis::LeftStickX);
            break;
        case InputAxisType::GamepadLeftStickY:
            axisRawValue = Input::GetGamepadAxis(config.Gamepad, GamepadAxis::LeftStickY);
            break;
        case InputAxisType::GamepadRightStickX:
            axisRawValue = Input::GetGamepadAxis(config.Gamepad, GamepadAxis::RightStickX);
            break;
        case InputAxisType::GamepadRightStickY:
            axisRawValue = Input::GetGamepadAxis(config.Gamepad, GamepadAxis::RightStickY);
            break;
        case InputAxisType::GamepadLeftTrigger:
            axisRawValue = Input::GetGamepadAxis(config.Gamepad, GamepadAxis::LeftTrigger);
            break;
        case InputAxisType::GamepadRightTrigger:
            axisRawValue = Input::GetGamepadAxis(config.Gamepad, GamepadAxis::RightTrigger);
            break;
        default:
            axisRawValue = 0.0f;
            break;
        }

        // Apply dead zone
        const float deadZone = config.DeadZone;
        float axisValue = axisRawValue >= deadZone || axisRawValue <= -deadZone ? axisRawValue : 0.0f;
        keyValue = keyValue >= deadZone || keyValue <= -deadZone ? keyValue : 0.0f;

        auto& e = AxesValues[i];
        e.Used = false;
        e.PrevKeyValue = keyRawValue;

        // Select keyboard input or axis input (choose the higher absolute values)
        e.Value = Math::Abs(keyValue) > Math::Abs(axisValue) ? keyValue : axisValue;
        e.RawValue = Math::Abs(keyRawValue) > Math::Abs(axisRawValue) ? keyRawValue : axisRawValue;

        // Scale
        e.Value *= config.Scale;
    }
    for (int32 i = 0; i < Input::AxisMappings.Count(); i++)
    {
        auto& e = AxesValues[i];
        if (e.Used)
            continue;
        const auto& config = Input::AxisMappings[i];
        const StringView name = config.Name;
        AxisData& data = Axes[name];

        // Blend final axis raw value between all entries
        // Virtual axis with the same name may be used more than once, select the highest absolute value
        for (int32 j = i + 1; j < Input::AxisMappings.Count(); j++)
        {
            auto& other = AxesValues[j];
            if (!other.Used && Input::AxisMappings[j].Name == config.Name)
            {
                if (Math::Abs(other.Value) > Math::Abs(e.Value))
                {
                    e = other;
                }
                other.Used = true;
            }
        }

        // Setup axis data
        data.PrevKeyValue = e.PrevKeyValue;
        data.ValueRaw = e.RawValue;
        data.Value = e.Value;

        // Mark as updated in this frame
        data.FrameIndex = frame;
    }

    // Remove not used entries
    for (auto i = Actions.Begin(); i.IsNotEnd(); ++i)
    {
        if (i->Value.FrameIndex != frame)
        {
            Actions.Remove(i);
        }
    }
    for (auto i = Axes.Begin(); i.IsNotEnd(); ++i)
    {
        if (i->Value.FrameIndex != frame)
        {
            Axes.Remove(i);
        }
    }

    // Lock mouse if need to
    const auto lockMode = Screen::GetCursorLock();
    if (lockMode == CursorLockMode::Locked)
    {
        Input::SetMousePosition(Screen::GetSize() * 0.5f);
    }

    // Send events for the active actions (send events only in play mode)
    if (!Time::GetGamePaused())
    {
        for (auto i = Actions.Begin(); i.IsNotEnd(); ++i)
        {
            if (i->Value.Active)
            {
                Input::ActionTriggered(i->Key);
            }
        }
    }
}
