
#pragma once

#include <chrono>

namespace blaze::util
{
class AutoTimer
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start;
	std::string message;

public:
	AutoTimer(const std::string& msg) : message(msg), start(std::chrono::high_resolution_clock::now())
	{
	}

	~AutoTimer()
	{
		auto end = std::chrono::high_resolution_clock::now();
		auto nano = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << message << nano.count() << std::endl;
	}
};
} // namespace blaze::util
