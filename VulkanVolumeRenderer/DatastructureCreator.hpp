#pragma once

#include <iostream>
#include <fstream>
#include <string>

#include "Octree.hpp"

namespace datastructure {
	void loadVoxelDataFromTxt(std::string filePath, std::vector<uint32_t>* voxelData);
	
	glm::uvec4 intToVec4(uint32_t rawValue);

	uint32_t vec4ToInt(glm::uvec4 encodedValue);
}