#include "model.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <numeric>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <tiny_obj_loader.h>

#include "context.h"
#include "errorHandeling.h"
#include "upload.h"

namespace NRE {
    namespace {
        Error loadError(const std::string& path, std::string_view reason) {
            return Error{ErrorCode::ModelLoadFailed, "Failed to load model " + path + ": " + std::string(reason)};
        }

        Result<Model> loadGltf(const std::string& path) {
            auto data = fastgltf::GltfDataBuffer::FromPath(path);
            if (data.error() != fastgltf::Error::None) {
                return std::unexpected(loadError(path, fastgltf::getErrorMessage(data.error())));
            }
            fastgltf::Parser parser;
            auto asset = parser.loadGltf(data.get(), std::filesystem::path(path).parent_path(),
                                         fastgltf::Options::LoadExternalBuffers);
            if (asset.error() != fastgltf::Error::None) {
                return std::unexpected(loadError(path, fastgltf::getErrorMessage(asset.error())));
            }

            Model model;
            for (auto& gltfMesh : asset->meshes) {
                for (auto& primitive : gltfMesh.primitives) {
                    // Only triangle lists for now; points/lines/strips are skipped.
                    if (primitive.type != fastgltf::PrimitiveType::Triangles) continue;
                    auto position = primitive.findAttribute("POSITION");
                    if (position == primitive.attributes.end()) continue;

                    Mesh mesh;
                    auto& positionAccessor = asset->accessors[position->accessorIndex];
                    mesh.vertices.resize(positionAccessor.count);
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), positionAccessor,
                        [&](glm::vec3 v, std::size_t i) { mesh.vertices[i].position = v; });

                    if (auto normal = primitive.findAttribute("NORMAL"); normal != primitive.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), asset->accessors[normal->accessorIndex],
                            [&](glm::vec3 n, std::size_t i) { mesh.vertices[i].normal = n; });
                    }
                    if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<glm::vec2>(asset.get(), asset->accessors[uv->accessorIndex],
                            [&](glm::vec2 t, std::size_t i) { mesh.vertices[i].uv = t; });
                    }

                    if (primitive.indicesAccessor.has_value()) {
                        auto& indexAccessor = asset->accessors[*primitive.indicesAccessor];
                        mesh.indices.resize(indexAccessor.count);
                        fastgltf::copyFromAccessor<uint32_t>(asset.get(), indexAccessor, mesh.indices.data());
                    } else {
                        mesh.indices.resize(mesh.vertices.size());
                        std::iota(mesh.indices.begin(), mesh.indices.end(), 0u);
                    }
                    model.meshes.push_back(std::move(mesh));
                }
            }
            return model;
        }

        Result<Model> loadObj(const std::string& path) {
            tinyobj::ObjReaderConfig config;
            config.triangulate = true;
            config.mtl_search_path = std::filesystem::path(path).parent_path().string();
            tinyobj::ObjReader reader;
            if (!reader.ParseFromFile(path, config)) {
                return std::unexpected(loadError(path, reader.Error()));
            }

            auto& attrib = reader.GetAttrib();
            Model model;
            for (auto& shape : reader.GetShapes()) {
                Mesh mesh;
                // One vertex per index (no de-duplication), so indices are just 0..n-1.
                for (auto& index : shape.mesh.indices) {
                    Vertex v;
                    v.position = {attrib.vertices[3 * index.vertex_index + 0],
                                  attrib.vertices[3 * index.vertex_index + 1],
                                  attrib.vertices[3 * index.vertex_index + 2]};
                    if (index.normal_index >= 0) {
                        v.normal = {attrib.normals[3 * index.normal_index + 0],
                                    attrib.normals[3 * index.normal_index + 1],
                                    attrib.normals[3 * index.normal_index + 2]};
                    }
                    if (index.texcoord_index >= 0) {
                        // OBJ has v=0 at the bottom; flip to match glTF/Vulkan (v=0 at the top).
                        v.uv = {attrib.texcoords[2 * index.texcoord_index + 0],
                                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
                    }
                    mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size()));
                    mesh.vertices.push_back(v);
                }
                model.meshes.push_back(std::move(mesh));
            }
            return model;
        }
    }

    Result<Model> loadModel(const std::string& path) {
        if (!std::filesystem::exists(path)) {
            return std::unexpected(Error{ErrorCode::FileNotFound, "File " + path + " not found"});
        }
        std::string extension = std::filesystem::path(path).extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char ch) { return std::tolower(ch); });

        if (extension == ".gltf" || extension == ".glb") return loadGltf(path);
        if (extension == ".obj") return loadObj(path);

        return std::unexpected(Error{ErrorCode::UnsupportedFormat, "Unsupported model format '" + extension + "' for " + path});
    }

    Result<void> Model::uploadMeshes() {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized");

        // Where each mesh's vertices and indices sit inside the shared staging buffer,
        // plus the new GPU buffers. These only move into the mesh once the copy succeeded.
        struct Region {
            Mesh* mesh;
            VkDeviceSize vertexOffset, vertexBytes;
            VkDeviceSize indexOffset, indexBytes;
            Buffer vertexBuffer, indexBuffer;
        };
        std::vector<Region> regions;
        VkDeviceSize stagingSize = 0;
        for (Mesh& mesh : meshes) {
            if (mesh.vertices.empty() || mesh.indices.empty()) continue;
            Region r{&mesh};
            r.vertexBytes = mesh.vertices.size() * sizeof(Vertex);
            r.indexBytes = mesh.indices.size() * sizeof(uint32_t);
            r.vertexOffset = stagingSize;
            r.indexOffset = r.vertexOffset + r.vertexBytes;
            stagingSize = r.indexOffset + r.indexBytes;
            regions.push_back(std::move(r));
        }
        if (regions.empty()) return {};

        Buffer staging(stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
        if (staging.buffer == VK_NULL_HANDLE) {
            return std::unexpected(Error{ErrorCode::BufferCreationFailed,"Failed to create staging buffer for model upload."});
        }

        // On any early return, the Buffers in regions free themselves and the meshes keep their old buffers.
        for (Region& r : regions) {
            if (auto res = staging.copyToBuffer(r.mesh->vertices.data(), r.vertexOffset, r.vertexBytes); !res) return std::unexpected(res.error());
            if (auto res = staging.copyToBuffer(r.mesh->indices.data(), r.indexOffset, r.indexBytes); !res) return std::unexpected(res.error());

            r.vertexBuffer = Buffer(r.vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);
            r.indexBuffer = Buffer(r.indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);
            if (r.vertexBuffer.buffer == VK_NULL_HANDLE || r.indexBuffer.buffer == VK_NULL_HANDLE) {
                return std::unexpected(Error{ErrorCode::BufferCreationFailed,"Failed to create mesh buffers for model upload."});
            }
        }

        auto submitted = immediateSubmit([&](VkCommandBuffer cmd) {
            for (const Region& r : regions) {
                VkBufferCopy vertexCopy{r.vertexOffset, 0, r.vertexBytes};
                vkCmdCopyBuffer(cmd, staging.buffer, r.vertexBuffer.buffer, 1, &vertexCopy);
                VkBufferCopy indexCopy{r.indexOffset, 0, r.indexBytes};
                vkCmdCopyBuffer(cmd, staging.buffer, r.indexBuffer.buffer, 1, &indexCopy);
            }

            VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT;
            VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dep.memoryBarrierCount = 1;
            dep.pMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd, &dep);
        });
        if (!submitted) return std::unexpected(submitted.error());

        // Replacing a mesh's buffers frees the old ones, which frames in flight may still be reading.
        bool replacing = std::ranges::any_of(meshes, [](const Mesh& m) { return m.vertexBuffer.buffer != VK_NULL_HANDLE; });
        if (replacing) vkDeviceWaitIdle(c->vulkan.device);
        for (Region& r : regions) {
            r.mesh->vertexBuffer = std::move(r.vertexBuffer);
            r.mesh->indexBuffer = std::move(r.indexBuffer);
        }
        return {};
    }

    void Model::releaseGpuMeshes() {
        for (Mesh& mesh : meshes) {
            (void)mesh.vertexBuffer.destroyBuffer();
            (void)mesh.indexBuffer.destroyBuffer();
        }
    }
    void Model::draw(VkCommandBuffer commandBuffer,Pipeline &pipeline, glm::mat4 viewProj) {
        PushConstants pc{transform.matrix(), viewProj};
        vkCmdPushConstants(commandBuffer, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(pc), &pc);

        for (auto& mesh : meshes) {
            if (mesh.indexBuffer.buffer == VK_NULL_HANDLE) continue;   // empty mesh, nothing was uploaded
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &offset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
        }
    }
}
