// Copyright (c) 2012-2020 Wojciech Figat. All rights reserved.

#if GRAPHICS_API_VULKAN

#include "QueueVulkan.h"
#include "GPUDeviceVulkan.h"
#include "CmdBufferVulkan.h"
#include "RenderToolsVulkan.h"

QueueVulkan::QueueVulkan(GPUDeviceVulkan* device, uint32 familyIndex)
    : _queue(VK_NULL_HANDLE)
    , _familyIndex(familyIndex)
    , _queueIndex(0)
    , _device(device)
    , _lastSubmittedCmdBuffer(nullptr)
    , _lastSubmittedCmdBufferFenceCounter(0)
    , _submitCounter(0)
{
    vkGetDeviceQueue(device->Device, familyIndex, 0, &_queue);
}

void QueueVulkan::Submit(CmdBufferVulkan* cmdBuffer, uint32 numSignalSemaphores, VkSemaphore* signalSemaphores)
{
    ASSERT(cmdBuffer->HasEnded());

    auto fence = cmdBuffer->GetFence();
    ASSERT(!fence->IsSignaled());

    const VkCommandBuffer cmdBuffers[] = { cmdBuffer->GetHandle() };

    VkSubmitInfo submitInfo;
    RenderToolsVulkan::ZeroStruct(submitInfo, VK_STRUCTURE_TYPE_SUBMIT_INFO);
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = cmdBuffers;
    submitInfo.signalSemaphoreCount = numSignalSemaphores;
    submitInfo.pSignalSemaphores = signalSemaphores;

    Array<VkSemaphore> waitSemaphores;
    if (cmdBuffer->WaitSemaphores.HasItems())
    {
        waitSemaphores.EnsureCapacity((uint32)cmdBuffer->WaitSemaphores.Count());
        for (auto semaphore : cmdBuffer->WaitSemaphores)
        {
            waitSemaphores.Add(semaphore->GetHandle());
        }
        submitInfo.waitSemaphoreCount = (uint32)cmdBuffer->WaitSemaphores.Count();
        submitInfo.pWaitSemaphores = waitSemaphores.Get();
        submitInfo.pWaitDstStageMask = cmdBuffer->WaitFlags.Get();
    }

    VALIDATE_VULKAN_RESULT(vkQueueSubmit(_queue, 1, &submitInfo, fence->GetHandle()));

    cmdBuffer->_state = CmdBufferVulkan::State::Submitted;
    cmdBuffer->MarkSemaphoresAsSubmitted();
    cmdBuffer->SubmittedFenceCounter = cmdBuffer->FenceSignaledCounter;

#if 0
	// Wait for the GPU to be idle on every submit (useful for tracking GPU hangs)
	const bool WaitForIdleOnSubmit = false;
	if (WaitForIdleOnSubmit)
	{
		// Use 200ms timeout
		bool success = _device->FenceManager.WaitForFence(fence, 200 * 1000 * 1000);
		ASSERT(success);
		ASSERT(_device->FenceManager.IsFenceSignaled(fence));
		cmdBuffer->GetOwner()->RefreshFenceStatus();
	}
#endif

    UpdateLastSubmittedCommandBuffer(cmdBuffer);

    cmdBuffer->GetOwner()->RefreshFenceStatus(cmdBuffer);
}

void QueueVulkan::GetLastSubmittedInfo(CmdBufferVulkan*& cmdBuffer, uint64& fenceCounter) const
{
    _locker.Lock();

    cmdBuffer = _lastSubmittedCmdBuffer;
    fenceCounter = _lastSubmittedCmdBufferFenceCounter;

    _locker.Unlock();
}

void QueueVulkan::UpdateLastSubmittedCommandBuffer(CmdBufferVulkan* cmdBuffer)
{
    _locker.Lock();

    _lastSubmittedCmdBuffer = cmdBuffer;
    _lastSubmittedCmdBufferFenceCounter = cmdBuffer->GetFenceSignaledCounter();
    _submitCounter++;

    _locker.Unlock();
}

#endif