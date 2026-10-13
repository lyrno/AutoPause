#include "process_manager.hpp"

#include <string_view>
#include <string>
#include <memory>
#include <winternl.h>
#include <ntstatus.h>

using pfnNtSuspendProcess = NTSTATUS(NTAPI*)(HANDLE ProcessHandle);

using pfnNtResumeProcess = NTSTATUS(NTAPI*)(HANDLE ProcessHandle);

using pfnNtQuerySystemInformation = NTSTATUS(NTAPI*)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);

typedef struct _AUTOPAUSE_SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	ULONGLONG WorkingSetPrivateSize;
	ULONG HardFaultCount;
	ULONG NumberOfThreadsHighWatermark;
	ULONGLONG CycleTime;
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR UniqueProcessKey;
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION Threads[1];
} AUTOPAUSE_SYSTEM_PROCESS_INFORMATION, * PAUTOPAUSE_SYSTEM_PROCESS_INFORMATION;

namespace
{
	constexpr ULONG system_thread_state_waiting = 5;
	constexpr ULONG system_thread_wait_reason_suspended = 5;

	enum class ErrorManagerHelper
	{
		InvalidIdentifier,
		ErrorGetModuleHandle,
		ErrorGetProcAddr,
		ErrorSnapshot,
		ErrorGetLength,
		ErrorGetInfoProcess,
		GetProcessId
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

	[[nodiscard]] std::expected<std::unique_ptr<std::byte[]>, ErrorManagerHelper> query_system_information() noexcept
	{
		auto proc_addr = get_ntdll_proc("NtQuerySystemInformation");
		if (!proc_addr) return std::unexpected(ErrorManagerHelper::ErrorGetInfoProcess);

		const auto nt_query_system_information = reinterpret_cast<pfnNtQuerySystemInformation>(*proc_addr);
		ULONG return_length{};

		NTSTATUS status = nt_query_system_information
		(
			SystemProcessInformation,
			nullptr,
			0,
			&return_length
		);

		if (status != STATUS_INFO_LENGTH_MISMATCH) return std::unexpected(ErrorManagerHelper::ErrorGetLength);

		auto unique_buffer = std::make_unique<std::byte[]>(return_length);

		status = nt_query_system_information
		(
			SystemProcessInformation,
			unique_buffer.get(),
			return_length,
			nullptr
		);

		if (!NT_SUCCESS(status)) return std::unexpected(ErrorManagerHelper::ErrorGetInfoProcess);

		return unique_buffer;
	}

	[[nodiscard]] std::expected<ULONG_PTR, ErrorManagerHelper> get_process_id(const HANDLE handle) noexcept
	{
		if (!handle || handle == INVALID_HANDLE_VALUE) return std::unexpected(ErrorManagerHelper::GetProcessId);

		DWORD process_id = GetProcessId(handle);

		if (process_id == 0) return std::unexpected(ErrorManagerHelper::GetProcessId);

		return static_cast<ULONG_PTR>(process_id);
	}

	[[nodiscard]] ULONG_PTR get_process_id(const AUTOPAUSE_SYSTEM_PROCESS_INFORMATION& process_info) noexcept
	{
		return reinterpret_cast<ULONG_PTR>(process_info.UniqueProcessId);
	}

	[[nodiscard]] bool is_process_suspended(const PAUTOPAUSE_SYSTEM_PROCESS_INFORMATION& process_info) noexcept
	{
		for (ULONG i = 0; i < process_info->NumberOfThreads; ++i)
		{
			const auto& thread = process_info->Threads[i];
			if  (
					thread.ThreadState != system_thread_state_waiting ||
					thread.WaitReason != system_thread_wait_reason_suspended
				) return false;
		}

		return true;
	}

	[[nodiscard]] PAUTOPAUSE_SYSTEM_PROCESS_INFORMATION next_process_info(const PAUTOPAUSE_SYSTEM_PROCESS_INFORMATION& process_info) noexcept
	{
		return reinterpret_cast<PAUTOPAUSE_SYSTEM_PROCESS_INFORMATION>(reinterpret_cast<std::byte*>(process_info) + process_info->NextEntryOffset);
	}
}

namespace process_manager
{
	std::expected<void, ProcessError> ProcessManager::suspend_process(const HANDLE handle) const noexcept
	{
		if (!handle || handle == INVALID_HANDLE_VALUE) return std::unexpected(ProcessError::InvalidHandle);

		auto addr_proc = get_ntdll_proc("NtSuspendProcess");

		if (!addr_proc) return std::unexpected(ProcessError::ErrorGetProc);

		auto NtSuspendProcess = reinterpret_cast<pfnNtSuspendProcess>(*addr_proc);

		NTSTATUS result = NtSuspendProcess(handle);

		if (result != 0) return std::unexpected(ProcessError::ErrorSuspendProcess);

		return {};
	}

	std::expected<void, ProcessError> ProcessManager::resume_process(const HANDLE handle) const noexcept
	{
		if (!handle || handle == INVALID_HANDLE_VALUE) return std::unexpected(ProcessError::InvalidHandle);

		auto addr_proc = get_ntdll_proc("NtResumeProcess");

		if (!addr_proc) return std::unexpected(ProcessError::ErrorGetProc);

		auto NtResumeProcess = reinterpret_cast<pfnNtResumeProcess>(*addr_proc);

		NTSTATUS result = NtResumeProcess(handle);

		if (result != 0) return std::unexpected(ProcessError::ErrorSuspendProcess);

		return {};
	}

	std::expected<std::vector<PROCESSENTRY32>, ProcessError> ProcessManager::get_info_processes() const noexcept
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



	[[nodiscard]] std::expected<bool, ProcessError> ProcessManager::is_suspend(const HANDLE handle) const noexcept
	{
		if (!handle || handle == INVALID_HANDLE_VALUE) return std::unexpected(ProcessError::InvalidHandle);

		auto result = query_system_information();

		if (!result) return std::unexpected(ProcessError::ErrorGetInfoProcess);

		std::unique_ptr<std::byte[]> buffer = std::move(*result);

		auto* process_info = reinterpret_cast<PAUTOPAUSE_SYSTEM_PROCESS_INFORMATION>(buffer.get());

		while (process_info)
		{
			auto process_id = get_process_id(handle);
			if (!process_id) return std::unexpected(ProcessError::ErrorGetInfoProcess);

			auto unique_process_id = get_process_id(*process_info);

			if (unique_process_id == *process_id)
			{
				if (is_process_suspended(process_info)) return true;
				else return false;
			}

			if (process_info->NextEntryOffset == 0) break;

			process_info = next_process_info(process_info);
		}

		return std::unexpected(ProcessError::ErrorGetInfoProcess);
	}

}