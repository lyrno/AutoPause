#pragma once

#include <cctype>
#include <string>

namespace string_utils
{
	[[nodiscard]] inline std::string tolower_string(const std::string& value)
	{
		std::string lowered{};
		lowered.reserve(value.size());

		for (const char character : value)
			lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));

		return lowered;
	}
}
