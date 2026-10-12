#pragma once

#include <Windows.h>
#include <expected>
#include <string>

namespace resolver
{
	enum class Error
	{
		InvalidIdentifier,
		ErrorSnapshot,
		ErrorGetProcessInfo,
		ErrorGetHWND
	};

	class ProcessResolver
	{
	public:

		[[nodiscard]] std::expected<DWORD, Error> get_pid_process_by_name(const std::wstring& process_name) const noexcept;

		[[nodiscard]] std::expected<DWORD, Error> get_pid_process_by_win_thread(const HWND hWnd) const noexcept;
	private:

	};
}