
#include "Blaze.hpp"

#include <iostream>
#include <Version.hpp>

/**
 * @brief Entrypoint for the binary executable.
 */
int main(int argc, char* argv[])
{
	try
	{
		std::cout << "Blaze (ver: " << VERSION.MAJOR << "." << VERSION.MINOR << "." << VERSION.BUILD << ")"
				  << std::endl;
		blaze::runRefactored();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}