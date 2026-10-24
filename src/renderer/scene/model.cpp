#include "scene/model.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <numeric>
#include <optional>
#include <variant>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_obj_loader.h>

#include "core/context.h"
#include "core/errorHandling.h"
#include "gpu/upload.h"

namespace NRE {
    namespace {
        Error loadError(const std::string& path, std::string_view reason) {
            return Error{ErrorCode::ModelLoadFailed, "Failed to load model " + path + ": " + std::string(reason)};
        }

        // Loads glTF image imageIndex into the resource manager. Returns an empty handle (= use the default texture)
        // if it can't be loaded; loadTexture / loadTextureFromMemory have already reported why.
        Handel<Texture> loadGltfImage(const fastgltf::Asset& asset, std::size_t imageIndex, const std::filesystem::path& dir) {
            Texture texture;
            auto loaded = std::visit(fastgltf::visitor{
                // A separate file next to the .gltf.
                [&](const fastgltf::sources::URI& file) -> Result<void> {
                    // A non-zero offset means the image is packed inside a bigger file, which loadTexture can't read.
                    if (file.fileByteOffset != 0 || !file.uri.isLocalPath()) return {};
                    return texture.loadTexture((dir / file.uri.fspath()).string());
                },
                // Embedded in a buffer: the binary chunk of a .glb.
                [&](const fastgltf::sources::BufferView& view) -> Result<void> {
                    auto bytes = fastgltf::DefaultBufferDataAdapter{}(asset, view.bufferViewIndex);
                    return texture.loadTextureFromMemory(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
                },
                // A base64 data URI in a .gltf, already decoded by fastgltf.
                [&](const fastgltf::sources::Array& array) -> Result<void> {
                    return texture.loadTextureFromMemory(reinterpret_cast<const unsigned char*>(array.bytes.data()),
                                                         array.bytes.size_bytes());
                },
                [](const auto&) -> Result<void> { return {}; },
            }, asset.images[imageIndex].data);

            // With RenderErrorPolicy::Ignore a failed load still returns success, so check that an image exists.
            if (!loaded || texture.getView() == VK_NULL_HANDLE) return {};
            return context()->manager.create(std::move(texture));
        }

        // Reads one primitive's vertices and indices. Returns nothing for primitives that can't be drawn as triangles.
        std::optional<Mesh> loadGltfPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive) {
            // Only triangle lists for now; points/lines/strips are skipped.
            if (primitive.type != fastgltf::PrimitiveType::Triangles) return std::nullopt;
            auto position = primitive.findAttribute("POSITION");
            if (position == primitive.attributes.end()) return std::nullopt;

            Mesh mesh;
            if (primitive.materialIndex.has_value()) mesh.material = static_cast<uint32_t>(*primitive.materialIndex);
            auto& positionAccessor = asset.accessors[position->accessorIndex];
            mesh.vertices.resize(positionAccessor.count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
                [&](glm::vec3 v, std::size_t i) { mesh.vertices[i].position = v; });

            if (auto normal = primitive.findAttribute("NORMAL"); normal != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normal->accessorIndex],
                    [&](glm::vec3 n, std::size_t i) { mesh.vertices[i].normal = n; });
            }
            if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->accessorIndex],
                    [&](glm::vec2 t, std::size_t i) { mesh.vertices[i].uv = t; });
            }

            if (primitive.indicesAccessor.has_value()) {
                auto& indexAccessor = asset.accessors[*primitive.indicesAccessor];
                mesh.indices.resize(indexAccessor.count);
                fastgltf::copyFromAccessor<uint32_t>(asset, indexAccessor, mesh.indices.data());
            } else {
                mesh.indices.resize(mesh.vertices.size());
                std::iota(mesh.indices.begin(), mesh.indices.end(), 0u);
            }
            return mesh;
        }

        // Moves the vertices from node space into model space.
        void bakeTransform(Mesh& mesh, const glm::mat4& transform) {
            if (transform == glm::mat4(1.0f)) return;
            // Normals need the inverse-transpose, or non-uniform scale would tilt them.
            glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(transform));
            for (Vertex& v : mesh.vertices) {
                v.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
                glm::vec3 n = normalMatrix * v.normal;
                if (glm::length(n) > 0.0f) v.normal = glm::normalize(n);   // a missing normal stays (0, 0, 0)
            }
            // A mirroring transform (negative scale) turns the triangles inside out, so put the winding back.
            if (glm::determinant(glm::mat3(transform)) < 0.0f) {
                for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
            }
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

            auto c = context();
            NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");

            Model model;
            // Same order as asset->materials, so a primitive's materialIndex indexes model.materials directly.
            // Several materials can use the same image, so each image is loaded at most once.
            std::vector<std::optional<Handel<Texture>>> imageTextures(asset->images.size());
            std::filesystem::path dir = std::filesystem::path(path).parent_path();
            for (auto& gltfMaterial : asset->materials) {
                Handel<Texture> base{};   // empty: createMaterial uses the default texture
                if (const auto& info = gltfMaterial.pbrData.baseColorTexture; info.has_value()) {
                    if (const auto& imageIndex = asset->textures[info->textureIndex].imageIndex; imageIndex.has_value()) {
                        auto& cached = imageTextures[*imageIndex];
                        if (!cached) cached = loadGltfImage(asset.get(), *imageIndex, dir);
                        base = *cached;
                    }
                }
                auto material = createMaterial(base);
                if (!material) return std::unexpected(material.error());
                // Not used by the shader yet: stored so it's there once base color factors are.
                if (Material* m = c->manager.get(*material)) {
                    const auto& f = gltfMaterial.pbrData.baseColorFactor;
                    m->baseColor = {f[0], f[1], f[2], f[3]};
                }
                model.materials.push_back(*material);
            }

            // Adds every primitive of glTF mesh meshIndex to the model, with transform baked into its vertices.
            auto addMesh = [&](std::size_t meshIndex, const glm::mat4& transform) {
                for (auto& primitive : asset->meshes[meshIndex].primitives) {
                    if (auto mesh = loadGltfPrimitive(asset.get(), primitive)) {
                        bakeTransform(*mesh, transform);
                        model.meshes.push_back(std::move(*mesh));
                    }
                }
            };

            if (asset->scenes.empty()) {
                // No scene means no node transforms: every mesh sits at its own origin.
                for (std::size_t i = 0; i < asset->meshes.size(); ++i) addMesh(i, glm::mat4(1.0f));
            } else {
                // Each node's matrix already includes its parents', so a child (the car's grill) follows its parent (the body).
                // A mesh used by several nodes gets its vertices copied once per node.
                fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value_or(0), fastgltf::math::fmat4x4(),
                    [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
                        if (node.meshIndex.has_value()) addMesh(*node.meshIndex, glm::make_mat4(matrix.data()));
                    });
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
    Result<void> Model::draw(VkCommandBuffer commandBuffer,Pipeline &pipeline, const glm::mat4& modelMatrix, const glm::mat4& viewProj) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized. Initialize with init.");
        PushConstants pc{modelMatrix, viewProj};
        vkCmdPushConstants(commandBuffer, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(pc), &pc);
        uint32_t materialCount = materials.size();
        for (auto& mesh : meshes) {
            if (mesh.indexBuffer.buffer == VK_NULL_HANDLE) continue;

            Material* material = mesh.material < materialCount ? c->manager.get(materials[mesh.material]) : nullptr;
            if (!material) material =  c->manager.get(c->vulkan.defaultMaterial);
            vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline.pipelineLayout,0,1,&material->descriptorSet,0,nullptr);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer.buffer, &offset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);
        }
        return {};
    }
}
