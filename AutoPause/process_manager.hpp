#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <expected>
#include <vector>


namespace process_manager
{
	enum class ProcessError
	{
		InvalidHandle,
		ErrorGetProc,
		ErrorSuspendProcess,
		ErrorSnapshotProceses,
		IterationError
	};

	class ProcessManager
	{
	public:
		[[nodiscard]] std::expected<void, ProcessError> suspend_process(const HANDLE handle) const noexcept;

		[[nodiscard]] std::expected<void, ProcessError> resume_process(const HANDLE handle) const noexcept;

		[[nodiscard]] std::expected<std::vector<PROCESSENTRY32>, ProcessError> get_info_processes() const noexcept;

	private:
	};
}