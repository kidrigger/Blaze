
#include "Blaze.hpp"

#include <iostream>
#include <thirdparty/renderdoc/renderdoc.h>
/**
 * @brief Entrypoint for the binary executable.
 */
int main(int argc, char* argv[])
{
	try
	{
		renderdoc::init();
		blaze::run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}