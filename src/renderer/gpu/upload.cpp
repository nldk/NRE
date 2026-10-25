#include "gpu/upload.h"

#include "core/context.h"
#include "core/errorHandling.h"

#include <string>

namespace NRE {
    Result<void> immediateSubmit(const std::function<void(VkCommandBuffer)>& record) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c);
        UploadContext& u = c->vulkan.upload;
        vkResetCommandBuffer(u.cmd, 0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(u.cmd, &bi);
        record(u.cmd);
        vkEndCommandBuffer(u.cmd);

        VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        cbi.commandBuffer = u.cmd;
        VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        si.commandBufferInfoCount = 1;
        si.pCommandBufferInfos = &cbi;
        if (VkResult r = vkQueueSubmit2(c->vulkan.graphicsQueue, 1, &si, u.fence); r != VK_SUCCESS) {
            auto error = Error{ErrorCode::UploadFailed, "Failed to submit a GPU upload (texture or mesh data): "
                               + std::string(ErrorHandeling::vkResultName(r))};
            if (ErrorHandeling::handleError(error)) return std::unexpected(error);
            return {};
        }
        vkWaitForFences(c->vulkan.device, 1, &u.fence, VK_TRUE, UINT64_MAX);
        vkResetFences(c->vulkan.device, 1, &u.fence);
        return {};
    }
}
