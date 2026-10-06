#include "process_manager.hpp"

#include <string_view>
#include <string>
#include <memory>

using pfnNtSuspendProcess = NTSTATUS(NTAPI*)(HANDLE ProcessHandle);

using pfnNtResumeProcess = NTSTATUS(NTAPI*)(HANDLE ProcessHandle);

namespace
{
	enum class ErrorManagerHelper
	{
		InvalidIdentifier,
		ErrorGetModuleHandle,
		ErrorGetProcAddr,
		ErrorSnapshot
	};

	[[nodiscard]] std::expected<FARPROC, ErrorManagerHelper> get_ntdll_proc(std::string_view name_proc)
	{
		if (name_proc.empty()) return std::unexpected(ErrorManagerHelper::InvalidIdentifier);

		const std::string proc_name(name_proc);

		static const HMODULE h_ntdll = GetModuleHandleA("ntdll.dll");

		if (!h_ntdll) return  std::unexpected(ErrorManagerHelper::ErrorGetModuleHandle);

		FARPROC proc_addr = GetProcAddress(h_ntdll, proc_name.c_str());

		if (!proc_addr) return std::unexpected(ErrorManagerHelper::ErrorGetProcAddr);

		return proc_addr;
	}

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

	[[nodiscard]] std::expected<auto_delete_handle, ErrorManagerHelper> snapshot_process(const DWORD flags, const DWORD process_id)
	{
		HANDLE handle_snapshot = CreateToolhelp32Snapshot(flags, process_id);

		if (handle_snapshot == INVALID_HANDLE_VALUE) return std::unexpected(ErrorManagerHelper::ErrorSnapshot);

		return auto_delete_handle(handle_snapshot);
	}
}

namespace process_manager
{
	std::expected<void, ProcessError> ProcessManager::suspend_process(const HANDLE handle) const
	{
		if (handle == INVALID_HANDLE_VALUE) return std::unexpected(ProcessError::InvalidHandle);

		auto addr_proc = get_ntdll_proc("NtSuspendProcess");

		if (!addr_proc) return std::unexpected(ProcessError::ErrorGetProc);

		auto NtSuspendProcess = reinterpret_cast<pfnNtSuspendProcess>(*addr_proc);

		NTSTATUS result = NtSuspendProcess(handle);

		if (result != 0) return std::unexpected(ProcessError::ErrorSuspendProcess);

		return {};
	}

	std::expected<void, ProcessError> ProcessManager::resume_process(const HANDLE handle) const
	{
		if (handle == INVALID_HANDLE_VALUE) return std::unexpected(ProcessError::InvalidHandle);

		auto addr_proc = get_ntdll_proc("NtResumeProcess");

		if (!addr_proc) return std::unexpected(ProcessError::ErrorGetProc);

		auto NtResumeProcess = reinterpret_cast<pfnNtResumeProcess>(*addr_proc);

		NTSTATUS result = NtResumeProcess(handle);

		if (result != 0) return std::unexpected(ProcessError::ErrorSuspendProcess);

		return {};
	}

	std::expected<std::vector<PROCESSENTRY32>, ProcessError> ProcessManager::get_info_processes() const
	{
		auto handle_snapshot = snapshot_process(TH32CS_SNAPPROCESS, 0);

		if (!handle_snapshot) return std::unexpected(ProcessError::ErrorSnapshotProceses);

		PROCESSENTRY32 struct_info_processes{};
		struct_info_processes.dwSize = sizeof(PROCESSENTRY32);

		std::vector<PROCESSENTRY32> info_processes;

		if (Process32First(handle_snapshot->get(), &struct_info_processes))
		{
			do
			{
				info_processes.push_back(struct_info_processes);

			} while (Process32Next(handle_snapshot->get(), &struct_info_processes));
		}
		else return std::unexpected(ProcessError::IterationError);

		return info_processes;
	}
}