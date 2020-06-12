
#include "Blaze.hpp"

#include <iostream>

/**
 * @brief Entrypoint for the binary executable.
 */
int main(int argc, char* argv[])
{
	try
	{
		blaze::runDeferred();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}