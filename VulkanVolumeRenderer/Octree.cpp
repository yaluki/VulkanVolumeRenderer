#include "Octree.hpp"

using namespace datastructure;

namespace datastructure {

	// public
	std::vector<Node> Octree::create(std::vector<uint32_t> voxelData) {
		int power = int(std::log(voxelData.size()) / std::log(8));
		uint32_t numNodes = 0;
		for (int i = power; i >= 0; i--) {
			numNodes += uint32_t(pow(8, i));
		}

		std::vector<Node> nodes;
		nodes.resize(numNodes);

		// init nodes and linkages, if leaf then children point to root (index 0)
		int powerIter = 0;
		uint32_t skipSection = 0;
		uint32_t skipSectionNext = 1;
		while (powerIter != power) {
			for (int i = 0; i < uint32_t(pow(8, powerIter)); i++) {
				Node node;
				node.firstChild = skipSectionNext + i * 8;
				nodes[skipSection + i] = node;
			}
			skipSection = skipSectionNext;
			skipSectionNext += uint32_t(pow(8, powerIter + 1));
			powerIter++;
		}

		// fill leaves with voxel information
		uint32_t voxelsPerSide = uint32_t(std::pow(2, power));
		uint32_t startIdx = nodes.size() - voxelData.size();
		uint32_t currentIdx = 0;
		for (int i = startIdx; i < nodes.size(); i += 8) {
			std::vector<uint32_t> indices = nextVoxelIdxBlock(currentIdx, voxelsPerSide);

			for (int j = 0; j < 8; j++) {
				nodes[i + j].color = voxelData[indices[j]];
			}
			currentIdx = nextNodeStartIdx(currentIdx, voxelsPerSide);
		}

		// rearrange top layers: creates new node vector and fills it up with nodes from nodes vector in the correct order, then copy them back into the vector
		uint32_t processedNodes = uint32_t(std::pow(8, power));;
		power--;

		while (power != 0) {
			voxelsPerSide = uint32_t(std::pow(2, power));
			uint32_t startIdx = nodes.size() - processedNodes - uint32_t(std::pow(8, power));
			uint32_t endIdx = nodes.size() - processedNodes;
			std::vector<Node> tmp(endIdx - startIdx);
			uint32_t currentIdx = 0;
			for (int i = 0; i < endIdx - startIdx; i += 8) {
				std::vector<uint32_t> indices = nextVoxelIdxBlock(startIdx + currentIdx, voxelsPerSide);

				for (int j = 0; j < 8; j++) {
					tmp[i + j] = nodes[indices[j]];
				}
				currentIdx = nextNodeStartIdx(currentIdx, voxelsPerSide);
			}

			for (int i = startIdx; i < endIdx; i++) {
				nodes[i] = tmp[i - startIdx];
			}

			processedNodes += uint32_t(std::pow(8, power));
			currentIdx = nodes.size() - processedNodes;
			power--;
		}

		// set values of higher nodes

		for (int i = nodes.size() - voxelData.size() - 1; i >= 0; i--) {
			glm::uvec4 meanVal = glm::uvec4(0.0);
			for (int j = 0; j < 8; j++) {
				meanVal += intToVec4(nodes[nodes[i].firstChild + j].color);
			}
			meanVal /= 8;
			nodes[i].color = vec4ToInt(meanVal);
		}

		return nodes;
	}

	std::vector<uint32_t> Octree::nextVoxelIdxBlock(uint32_t startVoxel, uint32_t N) {
		std::vector<uint32_t> voxelIndices(8);
		uint32_t Nsquare = N*N;
		voxelIndices[0] = startVoxel;
		voxelIndices[1] = startVoxel + 1;
		voxelIndices[2] = startVoxel + N;
		voxelIndices[3] = startVoxel + N + 1;
		voxelIndices[4] = startVoxel + Nsquare;
		voxelIndices[5] = startVoxel + Nsquare + 1;
		voxelIndices[6] = startVoxel + Nsquare + N;
		voxelIndices[7] = startVoxel + Nsquare + N + 1;
		return voxelIndices;
	}

	uint32_t Octree::nextNodeStartIdx(uint32_t currentIdx, uint32_t N) {
		uint32_t nextIdx = currentIdx + 2;
		// checks with +2 because zero based index
		if ((currentIdx + 2) % N == 0) {
			nextIdx += N;
			if ((((currentIdx + 2) / N) + 1) % N == 0) {
				nextIdx += N*N;
			}
		}
		return nextIdx;
	}

	// private
	void Octree::removeEmptyNodes() {
		// removes 1/4 of the volume to give a more interesting image
		//nodes[8].color = vec4ToInt(glm::vec4(0, 0, 0, 0));
	}
}