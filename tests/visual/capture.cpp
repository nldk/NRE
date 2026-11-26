// librenderer hides its own stb symbols, so the test carries its own copy.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "capture.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>

#include <vulkan/vulkan.h>

#include "renderer.h"
#include "renderer_unsafe.h"

namespace NreTest {
    glm::vec3 Image::average(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) const {
        glm::dvec3 sum{0.0};
        for (uint32_t y = y0; y < y1; ++y)
            for (uint32_t x = x0; x < x1; ++x) sum += glm::dvec3(at(x, y));
        return glm::vec3(sum / double((x1 - x0) * (y1 - y0)));
    }

    namespace {
        bool fail(const char* what, VkResult r = VK_SUCCESS) {
            std::fprintf(stderr, "capture: %s (VkResult %d)\n", what, int(r));
            return false;
        }

        template <class T>
        bool ok(const NRE::Result<T>& r, const char* what) {
            if (r) return true;
            std::cerr << "capture: " << what << " failed: " << r.error() << "\n";
            return false;
        }

        void barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                     VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                     VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.srcAccessMask = srcAccess;
            b.dstAccessMask = dstAccess;
            b.oldLayout = from;
            b.newLayout = to;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = image;
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
        }

        // Copies the render target (in SHADER_READ_ONLY_OPTIMAL) to RGB display values and puts it back.
        bool readRenderTarget(Image& out) {
            VkDevice device = NRE::Unsafe::getDevice();
            VkQueue queue = NRE::Unsafe::getGraphicsQueue();
            VkImage image = NRE::Unsafe::getRenderTargetImage();
            VkExtent2D extent = NRE::Unsafe::getRenderTargetExtent();
            VkFormat format = NRE::Unsafe::getRenderTargetFormat();
            if (image == VK_NULL_HANDLE) return fail("no render target");

            bool bgra = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_B8G8R8A8_UNORM;
            bool rgba = format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_R8G8B8A8_UNORM;
            if (!bgra && !rgba) return fail("unsupported render target format");

            VkDeviceSize size = VkDeviceSize(extent.width) * extent.height * 4;
            bool success = false;
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkCommandPool pool = VK_NULL_HANDLE;
            void* mapped = nullptr;
            do {
                VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
                bi.size = size;
                bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                if (VkResult r = vkCreateBuffer(device, &bi, nullptr, &buffer); r != VK_SUCCESS) { fail("vkCreateBuffer", r); break; }

                VkMemoryRequirements req;
                vkGetBufferMemoryRequirements(device, buffer, &req);
                VkPhysicalDeviceMemoryProperties props;
                vkGetPhysicalDeviceMemoryProperties(NRE::Unsafe::getPhysicalDevice(), &props);
                constexpr VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                uint32_t type = UINT32_MAX;
                for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
                    if ((req.memoryTypeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & want) == want) { type = i; break; }
                if (type == UINT32_MAX) { fail("no host-visible memory type"); break; }

                VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                ai.allocationSize = req.size;
                ai.memoryTypeIndex = type;
                if (VkResult r = vkAllocateMemory(device, &ai, nullptr, &memory); r != VK_SUCCESS) { fail("vkAllocateMemory", r); break; }
                vkBindBufferMemory(device, buffer, memory, 0);

                VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
                pi.queueFamilyIndex = NRE::Unsafe::getGraphicsQueueFamily();
                if (VkResult r = vkCreateCommandPool(device, &pi, nullptr, &pool); r != VK_SUCCESS) { fail("vkCreateCommandPool", r); break; }
                VkCommandBufferAllocateInfo ci{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                ci.commandPool = pool;
                ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                ci.commandBufferCount = 1;
                VkCommandBuffer cmd;
                if (VkResult r = vkAllocateCommandBuffers(device, &ci, &cmd); r != VK_SUCCESS) { fail("vkAllocateCommandBuffers", r); break; }

                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &begin);
                barrier(cmd, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                VkBufferImageCopy region{};
                region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                region.imageExtent = {extent.width, extent.height, 1};
                vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
                barrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                vkEndCommandBuffer(cmd);

                VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &cmd;
                if (VkResult r = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE); r != VK_SUCCESS) { fail("vkQueueSubmit", r); break; }
                vkQueueWaitIdle(queue);

                if (VkResult r = vkMapMemory(device, memory, 0, size, 0, &mapped); r != VK_SUCCESS) { fail("vkMapMemory", r); break; }
                out.width = extent.width;
                out.height = extent.height;
                out.pixels.resize(size_t(extent.width) * extent.height);
                const auto* bytes = static_cast<const uint8_t*>(mapped);
                for (size_t i = 0; i < out.pixels.size(); ++i) {
                    const uint8_t* p = bytes + i * 4;
                    out.pixels[i] = bgra ? glm::vec3(p[2], p[1], p[0]) / 255.0f : glm::vec3(p[0], p[1], p[2]) / 255.0f;
                }
                success = true;
            } while (false);

            if (mapped) vkUnmapMemory(device, memory);
            if (pool) vkDestroyCommandPool(device, pool, nullptr);
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
            return success;
        }
    }

    Image renderAndCapture(const std::function<void()>& draws) {
        using namespace NRE;
        if (!ok(renderFrame(), "renderFrame")) return {};
        if (draws) {
            draws();
            if (!ok(endFrame(), "endFrame")) return {};
        }
        if (!ok(submitFrame(), "submitFrame")) return {};
        if (!ok(presentFrame(), "presentFrame")) return {};
        vkDeviceWaitIdle(Unsafe::getDevice());
        Image image;
        if (!readRenderTarget(image)) return {};
        return image;
    }

    void savePng(const Image& image, const std::string& path) {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::vector<uint8_t> bytes(image.pixels.size() * 3);
        for (size_t i = 0; i < image.pixels.size(); ++i)
            for (int c = 0; c < 3; ++c)
                bytes[i * 3 + c] = uint8_t(std::clamp(image.pixels[i][c], 0.0f, 1.0f) * 255.0f + 0.5f);
        if (!stbi_write_png(path.c_str(), int(image.width), int(image.height), 3, bytes.data(), int(image.width) * 3))
            std::fprintf(stderr, "capture: could not write %s\n", path.c_str());
    }

    Image loadPng(const std::string& path) {
        int w, h, n;
        uint8_t* data = stbi_load(path.c_str(), &w, &h, &n, 3);
        if (!data) return {};
        Image image;
        image.width = uint32_t(w);
        image.height = uint32_t(h);
        image.pixels.resize(size_t(w) * h);
        for (size_t i = 0; i < image.pixels.size(); ++i)
            image.pixels[i] = glm::vec3(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]) / 255.0f;
        stbi_image_free(data);
        return image;
    }

    float fractionDifferent(const Image& a, const Image& b, float tolerance) {
        if (a.width != b.width || a.height != b.height || a.pixels.empty()) return 1.0f;
        size_t different = 0;
        for (size_t i = 0; i < a.pixels.size(); ++i) {
            glm::vec3 d = glm::abs(a.pixels[i] - b.pixels[i]);
            if (std::max({d.x, d.y, d.z}) > tolerance) ++different;
        }
        return float(different) / float(a.pixels.size());
    }

    namespace {
        struct Scenario { std::string name; ScenarioFn fn; };
        std::vector<Scenario>& registry() {
            static std::vector<Scenario> scenarios;
            return scenarios;
        }
        bool g_updateGolden = false;
    }

    ScenarioRegistrar::ScenarioRegistrar(const char* name, ScenarioFn fn) {
        std::string s = name;
        std::replace(s.begin(), s.end(), '_', '-');
        registry().push_back({s, fn});
    }

    bool updatingGolden() { return g_updateGolden; }

    int runScenarios(int argc, char** argv) {
        std::string only;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--update-golden") == 0) g_updateGolden = true;
            else only = argv[i];
        }
        int failures = 0, ran = 0;
        for (const auto& s : registry()) {
            if (!only.empty() && s.name != only) continue;
            ++ran;
            bool passed = s.fn();
            std::printf("%s: %s\n", s.name.c_str(), passed ? "PASS" : "FAIL");
            if (!passed) ++failures;
        }
        if (ran == 0) {
            std::fprintf(stderr, "no scenario named '%s'\n", only.c_str());
            return 2;
        }
        return failures == 0 ? 0 : 1;
    }
}
