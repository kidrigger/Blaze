
#include "files.hpp"

#include <fstream>
#include <stdexcept>
#include <sys/stat.h>

namespace blaze::util
{
std::vector<uint32_t> loadBinaryFile(const std::string_view& filename)
{
	using namespace std;
	ifstream file(filename.data(), ios::ate | ios::binary);
	if (file.is_open())
	{
		size_t filesize = file.tellg();
		vector<uint32_t> filedata(filesize / sizeof(uint32_t));
		file.seekg(0);
		file.read((char*)filedata.data(), filesize);
		file.close();
		return filedata;
	}
	throw std::runtime_error("File ("s + filename.data() + ") could not be opened.");
}

bool fileExists(const std::string_view& filename)
{
	struct stat s;
	return stat(filename.data(), &s) == 0;
}
} // namespace blaze::util
