
#include "files.hpp"

#include <stdexcept>
#include <fstream>
#include <sys/stat.h>

namespace blaze::util
{
 	std::vector<char> loadBinaryFile(const std::string& filename)
	{
		using namespace std;
		ifstream file(filename, ios::ate | ios::binary);
		if (file.is_open())
		{
			size_t filesize = file.tellg();
			vector<char> filedata(filesize);
			file.seekg(0);
			file.read(filedata.data(), filesize);
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

