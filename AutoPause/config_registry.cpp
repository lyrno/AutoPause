#include "config_registry.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

	struct RegistryEntry
	{
		std::filesystem::path config_path;
		std::string config_resolver;
	};

	[[nodiscard]] bool is_valid_entry(const std::filesystem::path& config_path, const std::string& config_resolver)
	{
		if (config_path.empty() || config_resolver.empty() || config_resolver.find_first_of("\r\n") != std::string::npos)
			return false;

		return config_path.string().find_first_of("\r\n") == std::string::npos;
	}

	[[nodiscard]] std::expected<RegistryEntry, configRegistry::ErrorRegistry> get_entry_from_line(const std::string& line)
	{
		std::string path_string{};
		std::string config_resolver{};

		if (!line.empty() && line.front() == '"')
		{
			std::istringstream entry_stream(line);
			char delimiter{};

			if (!(entry_stream >> std::quoted(path_string)) || !(entry_stream >> delimiter) || delimiter != '=')
				return std::unexpected(configRegistry::ErrorRegistry::FailedToGetPathsFromFile);

			if (!std::getline(entry_stream, config_resolver))
				return std::unexpected(configRegistry::ErrorRegistry::FailedToGetPathsFromFile);
		}
		else
		{
			const auto delimiter = line.find('=');

			if (delimiter == std::string::npos)
				return std::unexpected(configRegistry::ErrorRegistry::FailedToGetPathsFromFile);

			path_string = line.substr(0, delimiter);
			config_resolver = line.substr(delimiter + 1);
		}

		if (path_string.empty() || config_resolver.empty())
			return std::unexpected(configRegistry::ErrorRegistry::FailedToGetPathsFromFile);

		return RegistryEntry{ std::filesystem::path(path_string), std::move(config_resolver) };
	}

	[[nodiscard]] std::expected<std::vector<RegistryEntry>, configRegistry::ErrorRegistry> read_entries(const std::filesystem::path& file_path)
	{
		std::ifstream file(file_path);

		if (!file.is_open()) return std::unexpected(configRegistry::ErrorRegistry::FailedOpenConfig);

		std::vector<RegistryEntry> entries{};
		std::string line{};

		while (std::getline(file, line))
		{
			auto entry = get_entry_from_line(line);

			if (!entry) return std::unexpected(entry.error());

			entries.push_back(std::move(*entry));
		}

		if (file.bad()) return std::unexpected(configRegistry::ErrorRegistry::FailedReadFile);

		return entries;
	}

	[[nodiscard]] std::expected<configRegistry::ConfigRegistry::resolver_map, configRegistry::ErrorRegistry> build_resolver_map(const std::vector<RegistryEntry>& entries)
	{
		configRegistry::ConfigRegistry::resolver_map resolver_map{};

		for (const auto& entry : entries)
		{
			if (!resolver_map.emplace(entry.config_resolver, entry.config_path).second)
				return std::unexpected(configRegistry::ErrorRegistry::ResolverAlreadyRegistered);
		}

		return resolver_map;
	}

	[[nodiscard]] std::expected<void, configRegistry::ErrorRegistry> append_entry(const std::filesystem::path& file_path, const RegistryEntry& entry)
	{
		std::ofstream file(file_path, std::ios::app);

		if (!file.is_open()) return std::unexpected(configRegistry::ErrorRegistry::FailedOpenConfig);

		file << std::quoted(entry.config_path.string()) << '=' << entry.config_resolver << std::endl;

		if (!file) return std::unexpected(configRegistry::ErrorRegistry::FailedWriteFile);

		file.close();

		if (file.fail()) return std::unexpected(configRegistry::ErrorRegistry::FailedWriteFile);

		return {};
	}

	[[nodiscard]] std::expected<void, configRegistry::ErrorRegistry> write_entries(const std::filesystem::path& file_path, const std::vector<RegistryEntry>& entries)
	{
		std::ofstream file(file_path, std::ios::trunc);

		if (!file.is_open()) return std::unexpected(configRegistry::ErrorRegistry::FailedOpenConfig);

		for (const auto& entry : entries)
		{
			file << std::quoted(entry.config_path.string()) << '=' << entry.config_resolver << std::endl;

			if (!file) return std::unexpected(configRegistry::ErrorRegistry::FailedWriteFile);
		}

		file.close();

		if (file.fail()) return std::unexpected(configRegistry::ErrorRegistry::FailedWriteFile);

		return {};
	}

}

namespace configRegistry
{

	ConfigRegistry::ConfigRegistry(const config::ConfigFile& config_file_manager) : _config_file_manager(config_file_manager) {}

	[[nodiscard]] std::expected<void, ErrorRegistry> ConfigRegistry::init(const std::filesystem::path& init_path)
	{
		if (!_used_config_path.empty()) return std::unexpected(ErrorRegistry::ConfigAlreadyInitialized);
		if (init_path.empty()) return std::unexpected(ErrorRegistry::IncorrectData);

		std::error_code error_code{};
		const bool file_exists = std::filesystem::exists(init_path, error_code);

		if (error_code) return std::unexpected(ErrorRegistry::FailedInit);

		if (file_exists)
		{
			const bool is_regular = std::filesystem::is_regular_file(init_path, error_code);

			if (error_code) return std::unexpected(ErrorRegistry::FailedInit);
			if (!is_regular) return std::unexpected(ErrorRegistry::IncorrectData);
		}
		else if (auto result = _config_file_manager.create_file(init_path); !result)
		{
			return std::unexpected(ErrorRegistry::FailedInit);
		}

		auto entries = read_entries(init_path);

		if (!entries) return std::unexpected(entries.error());

		auto loaded_map = build_resolver_map(*entries);

		if (!loaded_map) return std::unexpected(loaded_map.error());

		_used_config_path = init_path;
		update_map(std::move(*loaded_map));

		return {};
	}

	[[nodiscard]] std::expected<void, ErrorRegistry> ConfigRegistry::add_path(const std::filesystem::path& config_path, const std::string& config_resolver)
	{
		if (_used_config_path.empty()) return std::unexpected(ErrorRegistry::ConfigNotInitialized);
		if (!is_valid_entry(config_path, config_resolver)) return std::unexpected(ErrorRegistry::IncorrectData);

		if (_resolver_map.contains(config_resolver))
			return std::unexpected(ErrorRegistry::ResolverAlreadyRegistered);

		auto updated_map = _resolver_map;
		updated_map.emplace(config_resolver, config_path);

		if (auto result = append_entry(_used_config_path, { config_path, config_resolver }); !result)
			return std::unexpected(result.error());

		update_map(std::move(updated_map));
		return {};
	}

	[[nodiscard]] std::expected<void, ErrorRegistry> ConfigRegistry::update_path(const std::filesystem::path& config_path, const std::string& config_resolver)
	{
		if (_used_config_path.empty()) return std::unexpected(ErrorRegistry::ConfigNotInitialized);
		if (!is_valid_entry(config_path, config_resolver)) return std::unexpected(ErrorRegistry::IncorrectData);

		auto entries = read_entries(_used_config_path);

		if (!entries) return std::unexpected(entries.error());

		auto entry = std::find_if(entries->begin(), entries->end(), [&config_resolver](const RegistryEntry& current_entry)
		{
			return current_entry.config_resolver == config_resolver;
		});

		if (entry == entries->end()) return std::unexpected(ErrorRegistry::ResolverNotFound);

		entry->config_path = config_path;

		auto updated_map = build_resolver_map(*entries);

		if (!updated_map) return std::unexpected(updated_map.error());

		if (auto result = write_entries(_used_config_path, *entries); !result)
			return std::unexpected(result.error());

		update_map(std::move(*updated_map));
		return {};
	}

	[[nodiscard]] std::expected<void, ErrorRegistry> ConfigRegistry::delete_path(const std::filesystem::path& config_path)
	{
		if (config_path.empty()) return std::unexpected(ErrorRegistry::IncorrectData);
		if (_used_config_path.empty()) return std::unexpected(ErrorRegistry::ConfigNotInitialized);

		auto entries = read_entries(_used_config_path);

		if (!entries) return std::unexpected(entries.error());

		std::vector<RegistryEntry> remaining_entries{};
		remaining_entries.reserve(entries->size());
		const auto normalized_path_to_delete = config_path.lexically_normal();

		for (auto& entry : *entries)
		{
			if (entry.config_path.lexically_normal() != normalized_path_to_delete)
				remaining_entries.push_back(std::move(entry));
		}

		auto updated_map = build_resolver_map(remaining_entries);

		if (!updated_map) return std::unexpected(updated_map.error());

		if (auto result = write_entries(_used_config_path, remaining_entries); !result)
			return std::unexpected(result.error());

		update_map(std::move(*updated_map));
		return {};
	}

	[[nodiscard]] const ConfigRegistry::resolver_map& ConfigRegistry::get_resolver_map() const noexcept
	{
		return _resolver_map;
	}

	[[nodiscard]] std::optional<std::filesystem::path> ConfigRegistry::find_path_by_resolver(const std::string& config_resolver) const
	{
		const auto it = _resolver_map.find(config_resolver);

		if (it == _resolver_map.end()) return std::nullopt;

		return it->second;
	}

	void ConfigRegistry::update_map(resolver_map updated_map)
	{
		_resolver_map.swap(updated_map);
	}

}