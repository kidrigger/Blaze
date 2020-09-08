
#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Model.hpp"

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

	std::shared_ptr<Model> loadModel(const Context* context, const spirv::Shader* set, const std::string& fileName)
	{
		int i = 0;
		for (auto& name : modelFileNames)
		{
			if (name == fileName)
			{
				return loadModel(context, set, i);
			}
			i++;
		}
	}
	std::shared_ptr<Model> loadModel(const Context* context, const spirv::Shader* set, uint32_t idx);

private:
	void setupMaterialSet(const Context* context, Model::Material& mat);
};
} // namespace blaze