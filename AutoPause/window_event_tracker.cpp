#include "window_event_tracker.hpp"

namespace windows_event
{
    void CALLBACK WindowEventTracker::Wineventproc
    (
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD idEventThread,
        DWORD dwmsEventTime
    )
    {
        if (instance_ && instance_->user_callback_) 
        {
            SystemWindowEvent params
            {
                hWinEventHook,
                event,
                hwnd,
                idObject,
                idChild,
                idEventThread,
                dwmsEventTime
            };
            instance_->user_callback_(params);
        }
    }

    std::expected<void, EventError> WindowEventTracker::start(event_callback callback) noexcept
    {
        stop();

        user_callback_ = std::move(callback);

        instance_ = this;

        hook_ = SetWinEventHook
        (
            EVENT_MIN,
            EVENT_MAX,
            NULL,
            Wineventproc,
            0, 0,
            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
        );

        if (!hook_)
        {
            instance_ = nullptr;
            return std::unexpected(EventError::ErrorSetHook);
        }

        return {};
    }

    void WindowEventTracker::stop() noexcept
    {

        if (hook_) 
        {
            UnhookWinEvent(hook_);
            hook_ = nullptr;
        }

        if (instance_ == this) instance_ = nullptr;
    }

    WindowEventTracker::~WindowEventTracker()
    {
        stop();
    }
}