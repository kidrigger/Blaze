
#pragma once

#include <vector>
#include <string>

/**
 * @namespace blaze::util
 *
 * @brief Contains all the utility classes and functions used in Blaze.
 */
namespace blaze::util
{
    /**
     * @fn fileExists(const std::string& filename)
     * 
     * @brief Checks if a file exists.
     *
     * @param filename The path of the file to check.
     *
     * @returns true If file exists.
     * @returns false Otherwise.
     */
    bool fileExists(const std::string& filename);
    
    /**
     * @fn loadBinaryFile(const std::string& filename)
     *
     * @brief Loads a binary file into memory and returns the data.
     *
     * @param filename The path of the binary file to open.
     *
     * @returns The binary data loaded from the file as \a vector<char>
     */
	std::vector<uint32_t> loadBinaryFile(const std::string& filename);
}
