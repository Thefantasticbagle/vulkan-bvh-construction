/**
 *	@file bvh.hpp
 *	For now, this file simply contains helper functions for BVH construction.
 *	Once this code reaches the point where it needs access to GPU memory, it should the moved into buffer.hpp or a similar place.
 */

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
 //#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // Use alignas(16) instead
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "glsl_cpp_common.h"
#include "tiny_obj_loader.h"

#include <cstdint> // uint32_t
#include <vector>
#include <fstream>
#include <iostream>

/**
 *	A fully compiled BVH.
 */
struct BVH {
	BVHNode*	nodes;
	RTTriangle* triangles;

	uint32_t	nodesCount,
				trianglesCount;
};

/**
 *	Builds and maintains a BVH.
 * 	https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/
 */
class BVHBuilder {
public:
	/**
	 *	Constructor.
	 */
	BVHBuilder() {}

	/**
	 *	Pushes a model from its file path.
	 */
	BVHBuilder push(const std::string path) {
		std::vector<RTTriangle> triangles {};

		// Load model based on extension
		//load manually
		if (path.substr(path.size() - 4) == ".tri") {
			FILE* file = fopen(path.c_str(), "r");
			float a, b, c, d, e, f, g, h, i;
			for (int t = 0; t < 12582; t++) // Hardcoded amount of triangles for now.
			{
				fscanf(file, "%f %f %f %f %f %f %f %f %f\n",
					&a, &b, &c, &d, &e, &f, &g, &h, &i);

				RTTriangle* triangle = new RTTriangle;
				triangle->p0 = glm::vec3(a, b, c);
				triangle->p1 = glm::vec3(d, e, f);
				triangle->p2 = glm::vec3(g, h, i);
				triangle->c = (triangle->p0 + triangle->p1 + triangle->p2) / 3.0f;
				triangles.push_back(*triangle);
			}
			fclose(file);
		}

		//load with tinyobjloader
		else if (path.substr(path.size() - 4) == ".obj") {
			tinyobj::attrib_t                   attributes;
			std::vector<tinyobj::shape_t>       shapes;
			std::vector<tinyobj::material_t>    materials;
			std::string                         warn, err;

			if (!tinyobj::LoadObj(&attributes, &shapes, &materials, &warn, &err, path.c_str()))
				throw std::runtime_error("ERR::VULKAN::BVHBUILDER::PUSHMODELFROMPATH::FAILED_TO_LOAD_MODEL::" + warn + "::" + err);

			for (const auto& shape : shapes) {
				int i = 0;
				glm::vec3 points[3];
				glm::vec2 texcoords[3];
				glm::vec3 normals[3];

				for (const auto& index : shape.mesh.indices) {
					float px, py, pz;
					px = attributes.vertices[3 * index.vertex_index + 0], py = attributes.vertices[3 * index.vertex_index + 1], pz = attributes.vertices[3 * index.vertex_index + 2];
					
					float tx=0, ty=0, nx=0, ny=0, nz=0;
					if (attributes.texcoords.size() > 0)
						tx = attributes.texcoords[2 * index.texcoord_index + 0], ty = 1.0f - attributes.texcoords[2 * index.texcoord_index + 1];
					if (attributes.normals.size() > 0)
						nx = attributes.normals[3 * index.normal_index + 0], ny = attributes.normals[3 * index.normal_index + 1], nz = attributes.normals[3 * index.normal_index + 2];

					points[i]       = {-py, pz, -px};
					normals[i]      = {-ny, nz, -nx};
					texcoords[i]    = {tx, ty};

					if (!(++i%=3)) {
						RTTriangle* triangle = new RTTriangle{};
						triangle->p0 = points[0]; triangle->p1 = points[1]; triangle->p2 = points[2];
						triangle->c = (points[0] + points[1] + points[2]) / 3.f;
						triangle->n0 = normals[0]; triangle->n1 = normals[1]; triangle->n2 = normals[2];
						triangle->t0 = texcoords[0]; triangle->t1 = texcoords[1]; triangle->t2 = texcoords[2];
						triangles.push_back(*triangle);
					}
				}
			}
		}

		// Push triangles
		return push(triangles);
	}

	/** 
	 *	Pushes a list of triangles.
	 */
	BVHBuilder push(std::vector<RTTriangle> triangles) {
		if (triangles.size() <= 0) throw std::runtime_error("ERR::VULKAN::BVHBUILDER::PUSHTRIANGLES::NO_TRIANGLES");
		for (int i = 0; i < triangles.size(); i++) this->triangles.push_back(triangles[i]);
		return *this;
	}

	/**
	 *	Builds the BVH.
	 */
	BVH build() {
		// Init fields
		trianglesCount	= triangles.size();
		triangleIdx		= new uint32_t[trianglesCount];
		nodes			= new BVHNode[trianglesCount * 2];
		rootNodeId		= 0,
		nodesCount		= 1;
		for (int i = 0; i < trianglesCount; i++) triangleIdx[i] = i;

		// Assign all triangles to the root node and update bounds
		BVHNode& rootNode = nodes[rootNodeId];
		rootNode.leftFirst = 0; rootNode.triCount = trianglesCount;
		updateBounds( rootNodeId );

		// Recursively split and adjust bounds
		subdivide( rootNodeId );

		// Finally, set up static BVH data, clean up, and return
		BVH bvh{};
		bvh.nodes = nodes;
		bvh.nodesCount = nodesCount;

		RTTriangle* trianglesSorted = new RTTriangle[trianglesCount];
		
		// TODO: Remove testing code
		uint32_t nodeToShow = 6; // 0 = Show all, anything else = just load that node's triangles
		if (nodeToShow == 0) for (int i = 0; i < trianglesCount; i++) trianglesSorted[i] = triangles[triangleIdx[i]];
		else {
			BVHNode& mNode = nodes[nodeToShow];
			for (int i = 0; i < trianglesCount; i++) {
				if (i >= mNode.leftFirst && i < mNode.leftFirst + mNode.triCount)
					 trianglesSorted[i] = triangles[triangleIdx[i]];
				else trianglesSorted[i] = RTTriangle{};
			}
		}

		bvh.triangles = trianglesSorted;
		bvh.trianglesCount = trianglesCount;

		delete triangleIdx;
		return bvh;
	}

private:
	std::vector<RTTriangle> triangles{};
	uint32_t*				triangleIdx;
	BVHNode*				nodes;
	uint32_t				rootNodeId,
							nodesCount,
							trianglesCount;

	/**
	 *	Updates the bounds of a given node.
	 */
	void updateBounds(uint32_t nodeId) {
		// Error handling
		if (nodeId >= nodesCount) throw std::runtime_error("ERR::VULKAN::BVHBUILDER::UPDATEBOUNDS::NODE_ID_EXCEEDS_MAX");
		BVHNode& node = nodes[nodeId];
		if (node.triCount == 0) throw std::runtime_error("ERR::VULKAN::BVHBUILDER::UPDATEBOUNDS::NODE_NOT_LEAF");

		// Set min and max bounds
		node.aabbMin = glm::vec3(1e30);
		node.aabbMax = glm::vec3(-1e30);
		for (int i = 0; i < node.triCount; i++) {
			RTTriangle& triangle = triangles[triangleIdx[node.leftFirst + i]];
			node.aabbMin = glm::min(node.aabbMin, triangle.p0); node.aabbMax = glm::max(node.aabbMax, triangle.p0);
			node.aabbMin = glm::min(node.aabbMin, triangle.p1); node.aabbMax = glm::max(node.aabbMax, triangle.p1);
			node.aabbMin = glm::min(node.aabbMin, triangle.p2); node.aabbMax = glm::max(node.aabbMax, triangle.p2);
		}
	}

	/**
	 *	Recursively subdivides the BVH.
	 *	Rearranges triangleIdx and nodes.
	 */
	void subdivide(uint32_t nodeId) {
		// Error handling
		if (nodeId >= nodesCount) throw std::runtime_error("ERR::VULKAN::BVHBUILDER::SUBDIVIDE::NODE_ID_EXCEEDS_MAX");
		BVHNode& node = nodes[nodeId];

		// End clause
		if (node.triCount <= 3000) return;

		// Determine axis and position of splitting plane
		glm::vec3 extent = node.aabbMax - node.aabbMin;
		uint32_t axis = 0; // 0 = x, 1 = y, 2 = z
		if (extent.y > extent.x) axis = 1;
		if (extent.z > extent[axis]) axis = 2;
		float splitPos = node.aabbMin[axis] + extent[axis] * 0.5f;

		// Quicksort triangles
		int i = node.leftFirst,
			j = node.leftFirst + node.triCount - 1;
		while (i <= j) {
			if (triangles[triangleIdx[i]].c[axis] < splitPos) i++;
			else std::swap(triangleIdx[i], triangleIdx[j--]);
		}

		// If one side of the split has no triangles, cancel the partitioning
		int leftTrianglesCount = i - node.leftFirst;
		if (leftTrianglesCount == 0 || leftTrianglesCount == node.triCount) return;

		// Otherwise, create child nodes for each partition
		uint32_t	leftChildId = nodesCount++,
					rightChildId = nodesCount++;
		
		nodes[leftChildId].leftFirst = node.leftFirst;
		nodes[leftChildId].triCount = leftTrianglesCount;
		
		nodes[rightChildId].leftFirst = i;
		nodes[rightChildId].triCount = node.triCount - leftTrianglesCount;

		node.leftFirst = leftChildId;
		node.triCount = 0;

		updateBounds( leftChildId );
		updateBounds( rightChildId );

		// Recurse
		subdivide( leftChildId );
		subdivide( rightChildId );
	}
};