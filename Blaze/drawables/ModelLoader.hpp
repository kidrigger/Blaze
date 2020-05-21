
#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Model2.hpp"

namespace blaze
{
namespace fs = std::filesystem;

class Context;

class ModelLoader
{
private:
	std::vector<std::string> modelFileNames;
	std::vector<fs::path> modelFilePaths;
public:
	ModelLoader() noexcept 
	{
		scan();
	}

	void scan();

	const std::vector<std::string>& getFileNames() const
	{
		return modelFileNames;
	}

	std::shared_ptr<Model2> loadModel(const Context* context, const spirv::Shader* set, uint32_t idx);

private:
	void setupMaterialSet(const Context* context, Model2::Material& mat);
};
} // namespace blaze