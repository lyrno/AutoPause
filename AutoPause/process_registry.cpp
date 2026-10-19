#include "process_registry.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "string_utils.hpp"

namespace
{

	constexpr char process_registry_resolver[] = "ProcessRegistry";

	[[nodiscard]] bool is_valid_process_name(const std::string& process_name)
	{
		return !process_name.empty()
			&& process_name.find_first_of("\r\n") == std::string::npos
			&& process_name.find('\0') == std::string::npos;
	}

	[[nodiscard]] bool contains_process_name(const process_registry::ProcessRegistry::process_list& process_names, const std::string& process_name)
	{
		const auto normalized_name = string_utils::tolower_string(process_name);

		return std::any_of(process_names.begin(), process_names.end(), [&normalized_name](const std::string& current_name)
		{
			return string_utils::tolower_string(current_name) == normalized_name;
		});
	}

	[[nodiscard]] std::expected<void, process_registry::ErrorProcessRegistry> create_config_file(config::ConfigFile& config_file_manager, const std::filesystem::path& config_path)
	{
		try
		{
			if (auto result = config_file_manager.create_file(config_path); !result)
				return std::unexpected(process_registry::ErrorProcessRegistry::FailedInit);
		}
		catch (const std::filesystem::filesystem_error&)
		{
			return std::unexpected(process_registry::ErrorProcessRegistry::FailedInit);
		}

		return {};
	}

	[[nodiscard]] std::expected<void, process_registry::ErrorProcessRegistry> move_config_file(config::ConfigFile& config_file_manager, const std::filesystem::path& from, const std::filesystem::path& to)
	{
		try
		{
			if (auto result = config_file_manager.move_file(from, to); !result)
				return std::unexpected(process_registry::ErrorProcessRegistry::FailedMoveConfig);
		}
		catch (const std::filesystem::filesystem_error&)
		{
			return std::unexpected(process_registry::ErrorProcessRegistry::FailedMoveConfig);
		}

		return {};
	}

}

namespace process_registry
{

	ProcessRegistry::ProcessRegistry(configRegistry::ConfigRegistry& config_registry, config::ConfigFile& config_file_manager)
		: _config_registry(config_registry), _config_file_manager(config_file_manager)
	{
	}

	[[nodiscard]] std::expected<void, ErrorProcessRegistry> ProcessRegistry::init(const std::filesystem::path& init_path)
	{
		if (!_used_config_path.empty()) return std::unexpected(ErrorProcessRegistry::ConfigAlreadyInitialized);
		if (init_path.empty()) return std::unexpected(ErrorProcessRegistry::IncorrectData);

		std::error_code error_code{};
		const bool config_exists = std::filesystem::exists(init_path, error_code);

		if (error_code) return std::unexpected(ErrorProcessRegistry::FailedInit);

		if (config_exists)
		{
			const bool is_regular = std::filesystem::is_regular_file(init_path, error_code);

			if (error_code) return std::unexpected(ErrorProcessRegistry::FailedInit);
			if (!is_regular) return std::unexpected(ErrorProcessRegistry::IncorrectData);
		}
		else if (auto result = create_config_file(_config_file_manager, init_path); !result)
		{
			return std::unexpected(result.error());
		}

		auto process_names = read_process_names(init_path);

		if (!process_names) return std::unexpected(process_names.error());

		_used_config_path = init_path;
		update_process_names(std::move(*process_names));
		return {};
	}

	[[nodiscard]] std::expected<void, ErrorProcessRegistry> ProcessRegistry::add_process(const std::string& process_name)
	{
		if (_used_config_path.empty()) return std::unexpected(ErrorProcessRegistry::ConfigNotInitialized);
		if (!is_valid_process_name(process_name)) return std::unexpected(ErrorProcessRegistry::IncorrectData);

		auto process_names = read_process_names(_used_config_path);

		if (!process_names) return std::unexpected(process_names.error());
		if (contains_process_name(*process_names, process_name))
			return std::unexpected(ErrorProcessRegistry::ProcessAlreadyRegistered);

		const auto previous_names = *process_names;
		process_names->push_back(process_name);

		if (auto result = write_process_names(_used_config_path, *process_names); !result)
		{
			if (result.error() == ErrorProcessRegistry::FailedWriteFile)
			{
				if (auto rollback = write_process_names(_used_config_path, previous_names); !rollback)
					return std::unexpected(ErrorProcessRegistry::FailedRollback);
			}

			return std::unexpected(result.error());
		}

		update_process_names(std::move(*process_names));
		return {};
	}

	[[nodiscard]] std::expected<void, ErrorProcessRegistry> ProcessRegistry::delete_process(const std::string& process_name)
	{
		if (_used_config_path.empty()) return std::unexpected(ErrorProcessRegistry::ConfigNotInitialized);
		if (!is_valid_process_name(process_name)) return std::unexpected(ErrorProcessRegistry::IncorrectData);

		auto process_names = read_process_names(_used_config_path);

		if (!process_names) return std::unexpected(process_names.error());

		const auto normalized_name = string_utils::tolower_string(process_name);
		auto process = std::find_if(process_names->begin(), process_names->end(), [&normalized_name](const std::string& current_name)
		{
			return string_utils::tolower_string(current_name) == normalized_name;
		});

		if (process == process_names->end()) return std::unexpected(ErrorProcessRegistry::ProcessNotFound);

		const auto previous_names = *process_names;
		process_names->erase(process);

		if (auto result = write_process_names(_used_config_path, *process_names); !result)
		{
			if (result.error() == ErrorProcessRegistry::FailedWriteFile)
			{
				if (auto rollback = write_process_names(_used_config_path, previous_names); !rollback)
					return std::unexpected(ErrorProcessRegistry::FailedRollback);
			}

			return std::unexpected(result.error());
		}

		update_process_names(std::move(*process_names));
		return {};
	}

	[[nodiscard]] const ProcessRegistry::process_list& ProcessRegistry::get_process_names() const noexcept
	{
		return _process_names;
	}

	[[nodiscard]] std::optional<std::string> ProcessRegistry::find_process(const std::string& process_name) const
	{
		if (!is_valid_process_name(process_name)) return std::nullopt;

		const auto normalized_name = string_utils::tolower_string(process_name);
		const auto process = std::find_if(_process_names.begin(), _process_names.end(), [&normalized_name](const std::string& current_name)
		{
			return string_utils::tolower_string(current_name) == normalized_name;
		});

		if (process == _process_names.end()) return std::nullopt;

		return *process;
	}

	[[nodiscard]] std::expected<void, ErrorProcessRegistry> ProcessRegistry::relocate(const std::filesystem::path& new_path)
	{
		if (_used_config_path.empty()) return std::unexpected(ErrorProcessRegistry::ConfigNotInitialized);
		if (new_path.empty()) return std::unexpected(ErrorProcessRegistry::IncorrectData);
		if (_used_config_path.lexically_normal() == new_path.lexically_normal()) return {};

		const auto old_path = _used_config_path;

		if (auto result = move_config_file(_config_file_manager, old_path, new_path); !result)
			return std::unexpected(result.error());

		bool registry_updated{};

		try
		{
			registry_updated = static_cast<bool>(_config_registry.update_path(new_path, process_registry_resolver));
		}
		catch (const std::filesystem::filesystem_error&)
		{
		}

		if (!registry_updated)
		{
			if (auto rollback = move_config_file(_config_file_manager, new_path, old_path); !rollback)
			{
				_used_config_path = new_path;
				return std::unexpected(ErrorProcessRegistry::FailedRollback);
			}

			return std::unexpected(ErrorProcessRegistry::FailedUpdateGlobalRegistry);
		}

		_used_config_path = new_path;
		return {};
	}

	[[nodiscard]] std::expected<ProcessRegistry::process_list, ErrorProcessRegistry> ProcessRegistry::read_process_names(const std::filesystem::path& config_path) const
	{
		if (config_path.empty()) return std::unexpected(ErrorProcessRegistry::IncorrectData);

		std::ifstream file(config_path);

		if (!file.is_open()) return std::unexpected(ErrorProcessRegistry::FailedOpenConfig);

		process_list process_names{};
		std::unordered_set<std::string> normalized_names{};
		std::string process_name{};

		while (std::getline(file, process_name))
		{
			if (process_name.empty()) continue;
			if (!is_valid_process_name(process_name)) return std::unexpected(ErrorProcessRegistry::IncorrectData);

			if (!normalized_names.insert(string_utils::tolower_string(process_name)).second)
				return std::unexpected(ErrorProcessRegistry::ProcessAlreadyRegistered);

			process_names.push_back(std::move(process_name));
		}

		if (file.bad()) return std::unexpected(ErrorProcessRegistry::FailedReadFile);

		return process_names;
	}

	[[nodiscard]] std::expected<void, ErrorProcessRegistry> ProcessRegistry::write_process_names(const std::filesystem::path& config_path, const process_list& process_names) const
	{
		if (config_path.empty()) return std::unexpected(ErrorProcessRegistry::IncorrectData);

		std::unordered_set<std::string> normalized_names{};

		for (const auto& process_name : process_names)
		{
			if (!is_valid_process_name(process_name)) return std::unexpected(ErrorProcessRegistry::IncorrectData);
			if (!normalized_names.insert(string_utils::tolower_string(process_name)).second)
				return std::unexpected(ErrorProcessRegistry::ProcessAlreadyRegistered);
		}

		std::ofstream file(config_path, std::ios::trunc);

		if (!file.is_open()) return std::unexpected(ErrorProcessRegistry::FailedOpenConfig);

		for (const auto& process_name : process_names)
		{
			file << process_name << std::endl;

			if (!file) return std::unexpected(ErrorProcessRegistry::FailedWriteFile);
		}

		file.close();

		if (file.fail()) return std::unexpected(ErrorProcessRegistry::FailedWriteFile);

		return {};
	}

	void ProcessRegistry::update_process_names(process_list updated_names)
	{
		_process_names.swap(updated_names);
	}

}