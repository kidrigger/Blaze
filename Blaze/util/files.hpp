
#pragma once

#include <vector>
#include <string>

namespace blaze::util
{
    bool fileExists(const std::string& filename);
	std::vector<char> loadBinaryFile(const std::string& filename);
}
