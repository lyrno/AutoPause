#pragma once

#include <Windows.h>
#include <expected>
#include <functional>
#include <vector>

namespace windows_event
{
    enum class EventError 
    {
        ErrorSetHook
    };

    struct SystemWindowEvent
    {
        HWINEVENTHOOK hWinEventHook;
        DWORD event;
        HWND hwnd;
        LONG idObject;
        LONG idChild;
        DWORD idEventThread;
        DWORD dwmsEventTime;
    };

    class WindowEventTracker
    {
    public:
        using event_callback = std::function<void(const SystemWindowEvent&)>;

        WindowEventTracker() = default;
        ~WindowEventTracker();

        WindowEventTracker(const WindowEventTracker&) = delete;
        WindowEventTracker& operator=(const WindowEventTracker&) = delete;

        WindowEventTracker(WindowEventTracker&&) noexcept = default;
        WindowEventTracker& operator=(WindowEventTracker&&) noexcept = default;

        [[nodiscard]] std::expected<void, EventError> start(event_callback callback) noexcept;

        void stop() noexcept;

    private:

        static void CALLBACK Wineventproc
        (
            HWINEVENTHOOK hWinEventHook,
            DWORD event,
            HWND hwnd,
            LONG idObject,
            LONG idChild,
            DWORD idEventThread,
            DWORD dwmsEventTime
        ) noexcept;

        HWINEVENTHOOK hook_{};
        event_callback user_callback_{};
        inline static WindowEventTracker* instance_{};
    };
}