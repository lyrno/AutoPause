#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "config_file_manager.hpp"
#include "config_registry.hpp"

namespace process_registry
{
	enum class ErrorProcessRegistry
	{
		IncorrectData,
		ConfigAlreadyInitialized,
		ConfigNotInitialized,
		FailedInit,
		FailedOpenConfig,
		FailedReadFile,
		FailedWriteFile,
		ProcessAlreadyRegistered,
		ProcessNotFound,
		FailedMoveConfig,
		FailedUpdateGlobalRegistry,
		FailedRollback
	};

	class ProcessRegistry
	{
	public:
		using process_list = std::vector<std::string>;

		explicit ProcessRegistry(configRegistry::ConfigRegistry& config_registry, config::ConfigFile& config_file_manager);

		[[nodiscard]] std::expected<void, ErrorProcessRegistry> init(const std::filesystem::path& init_path);
		[[nodiscard]] std::expected<void, ErrorProcessRegistry> add_process(const std::string& process_name);
		[[nodiscard]] std::expected<void, ErrorProcessRegistry> delete_process(const std::string& process_name);
		[[nodiscard]] const process_list& get_process_names() const noexcept;
		[[nodiscard]] std::optional<std::string> find_process(const std::string& process_name) const;
		[[nodiscard]] std::expected<void, ErrorProcessRegistry> relocate(const std::filesystem::path& new_path);

	private:
		[[nodiscard]] std::expected<process_list, ErrorProcessRegistry> read_process_names(const std::filesystem::path& config_path) const;
		[[nodiscard]] std::expected<void, ErrorProcessRegistry> write_process_names(const std::filesystem::path& config_path, const process_list& process_names) const;
		void update_process_names(process_list updated_names);

		configRegistry::ConfigRegistry& _config_registry;
		config::ConfigFile& _config_file_manager;
		std::filesystem::path _used_config_path;
		process_list _process_names{};
	};
}