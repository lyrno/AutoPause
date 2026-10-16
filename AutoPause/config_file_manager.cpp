#include "config_file_manager.hpp"

#include <fstream>
#include <algorithm>
#include <ranges>


namespace config
{

	[[nodiscard]] std::expected<void, Error> ConfigFile::create_file(const std::filesystem::path& file_path)
	{
		if(file_path.empty()) return std::unexpected(Error::IncorrectData);

		if (std::filesystem::is_regular_file(file_path)) return std::unexpected(Error::FailExits);

		std::ofstream file(file_path);

		if (!file.is_open())  return std::unexpected(Error::FailedCreateFile);

		return {};
	}

	[[nodiscard]] std::expected<void, Error> ConfigFile::move_file(const std::filesystem::path& from, const std::filesystem::path& to)
	{
		if(from.empty() || to.empty()) return std::unexpected(Error::IncorrectData);
		if (std::filesystem::is_directory(to) || std::filesystem::is_regular_file(to)) return std::unexpected(Error::IncorrectData);
		if(!std::filesystem::is_regular_file(from)) return std::unexpected(Error::FileNotFound);

		std::error_code ec{};

		if (std::filesystem::rename(from, to, ec); ec) return std::unexpected(Error::FailedMoveFail);

		return {};
	}

	[[nodiscard]] std::expected<void, Error> ConfigFile::delete_file(const std::filesystem::path& file_path)
	{
		if (file_path.empty()) return std::unexpected(Error::IncorrectData);
		if(std::filesystem::is_directory(file_path) || !std::filesystem::is_regular_file(file_path)) return std::unexpected(Error::IncorrectData);

		if (!std::filesystem::remove(file_path)) return std::unexpected(Error::FailedDeleteFail);

		return {};
	}
}