#include "gpu/upload.h"

#include "core/context.h"
#include "core/errorHandling.h"

namespace NRE {
    Result<void> immediateSubmit(const std::function<void(VkCommandBuffer)>& record) {
        auto c = context();
        NRE_REQUIRE_CONTEXT(c, "Renderer not initialized");
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
        if (vkQueueSubmit2(c->vulkan.graphicsQueue, 1, &si, u.fence) != VK_SUCCESS) {
            if (ErrorHandeling::handleError(Error{ErrorCode::UploadFailed,"Failed to submit upload commands."})) {
                return std::unexpected(Error{ErrorCode::UploadFailed,"Failed to submit upload commands."});
            };
            return {};
        }
        vkWaitForFences(c->vulkan.device, 1, &u.fence, VK_TRUE, UINT64_MAX);
        vkResetFences(c->vulkan.device, 1, &u.fence);
        return {};
    }
}
