#pragma once

#include <expected>
#include <unordered_map>
#include <filesystem>
#include <string>

namespace config
{

	enum class Error
	{
		IncorrectData,
		FailedCreateFile,
		FileNotFound,
		FailedDeleteFail,
		FailExits,
		FailedMoveFail
	};

	class ConfigFile
	{
	public:

		[[nodiscard]] std::expected<void, Error> create_file(const std::filesystem::path& file_path);
		[[nodiscard]] std::expected<void, Error> move_file(const std::filesystem::path& from, const std::filesystem::path& to);
		[[nodiscard]] std::expected<void, Error> delete_file(const std::filesystem::path& file_path);

	};
}