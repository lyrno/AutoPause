#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "config_file_manager.hpp"

namespace configRegistry
{
	enum class ErrorRegistry
	{
		IncorrectData,
		ConfigAlreadyInitialized,
		FailedInit,
		FailedOpenConfig,
		FailedToGetPathsFromFile,
		FailedReadFile,
		FailedRemoveFail,
		FailedWriteFile,
		ConfigNotInitialized,
		ResolverAlreadyRegistered
	};

	class ConfigRegistry
	{
	public:
		using resolver_map = std::unordered_map<std::string, std::filesystem::path>;

		ConfigRegistry() = delete;

		explicit ConfigRegistry(const config::ConfigFile& config_file);

		[[nodiscard]] std::expected<void, ErrorRegistry> init(const std::filesystem::path& init_path);
		[[nodiscard]] std::expected<void, ErrorRegistry> add_path(const std::filesystem::path& config_path, const std::string& config_resolver);
		[[nodiscard]] std::expected<void, ErrorRegistry> delete_path(const std::filesystem::path& config_path);
		[[nodiscard]] const resolver_map& get_resolver_map() const noexcept;
		[[nodiscard]] std::optional<std::filesystem::path> find_path_by_resolver(const std::string& config_resolver) const;

	private:
		void update_map(resolver_map updated_map);

		config::ConfigFile _config_file_manager{};
		std::filesystem::path _used_config_path;
		resolver_map _resolver_map{};
	};
}