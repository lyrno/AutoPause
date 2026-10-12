#include "process_resolver.hpp"

#include <memory>
#include <TlHelp32.h>

namespace
{
	enum class Error
	{
		ErrorSnapshot
	};

	struct HandleDeleter
	{
		void operator()(const HANDLE handle) const
		{
			if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(handle);
			}
		}
	};

	using auto_delete_handle = std::unique_ptr<void, HandleDeleter>;

	[[nodiscard]] std::expected<auto_delete_handle, Error> snapshot_processes()
	{
		HANDLE handle_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (handle_snapshot == INVALID_HANDLE_VALUE) return std::unexpected(Error::ErrorSnapshot);

		return auto_delete_handle(handle_snapshot);
	}
}

namespace resolver
{
	[[nodiscard]] std::expected<DWORD, Error> ProcessResolver::get_pid_process_by_name(const std::wstring& process_name) const noexcept
	{
		if (process_name.empty()) return std::unexpected(Error::InvalidIdentifier);

		auto snap_handle = snapshot_processes();

		if(!snap_handle) return std::unexpected(Error::ErrorSnapshot);

		PROCESSENTRY32W pe{};
		pe.dwSize = sizeof(PROCESSENTRY32W);

		if (Process32First(snap_handle->get(), &pe))
		{
			do
			{
				if (_wcsicmp(process_name.c_str(), pe.szExeFile) == 0)
				{
					return pe.th32ProcessID;
				}

			} while (Process32Next(snap_handle->get(), &pe));

		} else return std::unexpected(Error::ErrorGetProcessInfo);

		return {};
	}

	[[nodiscard]] std::expected<DWORD, Error> ProcessResolver::get_pid_process_by_win_thread(const HWND hWnd) const noexcept
	{
		if (!hWnd) return std::unexpected(Error::InvalidIdentifier);

		DWORD process_id{};

		if (!GetWindowThreadProcessId(hWnd, &process_id)) return std::unexpected(Error::ErrorGetHWND);

		return process_id;
	}

}