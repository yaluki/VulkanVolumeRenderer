#pragma once

#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "DatastructureCreator.hpp"

namespace datastructure {
	struct Node {
		uint32_t color;
		uint32_t firstChild;
	};

	class Octree {
	private:
		std::vector<Node> nodes;

		std::vector<Node> create(std::vector<uint32_t> voxelData);

		std::vector<uint32_t> nextVoxelIdxBlock(uint32_t startVoxel, uint32_t N);

		uint32_t nextNodeStartIdx(uint32_t currentIdx, uint32_t N);

	public:
		glm::vec3 pos;
		float voxelFreq;
		int32_t numVoxelsSide;

		Octree(std::vector<uint32_t>* voxelData, glm::vec3 pos, float voxelFreq) {
			this->pos = pos;
			this->voxelFreq = voxelFreq;
			numVoxelsSide = int32_t(std::cbrt(voxelData->size()));
			nodes = create(*voxelData);
		}

		~Octree() {
		}

		void removeEmptyNodes();

		void* data() {
			return nodes.data();
		}

		uint32_t numNodes() {
			return nodes.size();
		}
	};
}