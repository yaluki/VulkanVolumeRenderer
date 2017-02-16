#ifdef _WIN32
#pragma comment(linker, "/subsystem:console")
#endif

#include "DatastructureCreator.hpp"


namespace datastructure {

	void loadVoxelDataFromTxt(std::string filePath, std::vector<uint32_t>* voxelData) {
		std::ifstream fin(filePath, std::ifstream::in);

		if (fin.is_open()) {
			char c;
			char buffer[3];
			int pos = 0;
			int intColor;
			while (fin >> c) {
				if (c == ';') {
					std::sscanf(buffer, "%d", &intColor);
					int alpha = 255;
					if (intColor == 0) {
						alpha = 0;
					}
					uint32_t color = vec4ToInt(glm::vec4(intColor, intColor, intColor, intColor));
					pos = 0;
					std::fill_n(buffer, 3, NULL);
					voxelData->push_back(color);
				} else {
					buffer[pos++] = c;
				}
			}
			fin.close();
		} else {
			std::cout << "Unable to open file!" << std::endl;
		}
	}

	glm::uvec4 intToVec4(uint32_t number) {
		glm::uvec4 uVector;
		int mask = ((1 << 8) - 1);
		for (int i = 0; i < 4; i++) {
			uVector[3 - i] = (number >> 8 * i) & mask;
		}
		return uVector;
	}

	uint32_t vec4ToInt(glm::uvec4 uVector) {
		return (uVector.r << 24) | (uVector.g << 16) | (uVector.b << 8) | (uVector.a);
	}
}
