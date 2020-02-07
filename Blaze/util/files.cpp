
#include "files.hpp"

#include <stdexcept>
#include <fstream>
#include <sys/stat.h>

namespace blaze::util
{
 	std::vector<uint32_t> loadBinaryFile(const std::string& filename)
	{
		using namespace std;
		ifstream file(filename, ios::ate | ios::binary);
		if (file.is_open())
		{
			size_t filesize = file.tellg();
			vector<uint32_t> filedata(filesize / sizeof(uint32_t));
			file.seekg(0);
			file.read((char*)filedata.data(), filesize);
			file.close();
			return filedata;
		}
		throw std::runtime_error("File (" + filename + ") could not be opened.");
	}

    bool fileExists(const std::string& filename)
    {
        struct stat s;
        return stat(filename.c_str(), &s) == 0;
    }
}

