/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtVulkan module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qvulkanfunctions.h"

QT_BEGIN_NAMESPACE

class QVulkanFunctionsPrivate
{
public:
    QVulkanFunctionsPrivate(QVulkanFunctions *q_ptr);
    ~QVulkanFunctionsPrivate();

    QVulkanFunctions *q;

    HMODULE m_lib;
};

QVulkanFunctions::QVulkanFunctions()
    : d(new QVulkanFunctionsPrivate(this))
{
}

QVulkanFunctions::~QVulkanFunctions()
{
    delete d;
}

Q_GLOBAL_STATIC(QVulkanFunctions, globalVkFunc)

QVulkanFunctions *QVulkanFunctions::instance()
{
    return globalVkFunc();
}

QVulkanFunctionsPrivate::QVulkanFunctionsPrivate(QVulkanFunctions *q_ptr)
    : q(q_ptr)
{
    m_lib = LoadLibraryA("vulkan-1");
    if (!m_lib) {
        OutputDebugStringA("Failed to load vulkan-1");
        abort();
    }

    q->vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(GetProcAddress(m_lib, "vkCreateInstance"));
    q->vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(GetProcAddress(m_lib, "vkDestroyInstance"));
    q->vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(GetProcAddress(m_lib, "vkEnumeratePhysicalDevices"));
    q->vkGetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(GetProcAddress(m_lib, "vkGetPhysicalDeviceFeatures"));
    q->vkGetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(GetProcAddress(m_lib, "vkGetPhysicalDeviceFormatProperties"));
    q->vkGetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(GetProcAddress(m_lib, "vkGetPhysicalDeviceImageFormatProperties"));
    q->vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(GetProcAddress(m_lib, "vkGetPhysicalDeviceProperties"));
    q->vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(GetProcAddress(m_lib, "vkGetPhysicalDeviceQueueFamilyProperties"));
    q->vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(GetProcAddress(m_lib, "vkGetPhysicalDeviceMemoryProperties"));
    q->vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(m_lib, "vkGetInstanceProcAddr"));
    q->vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(m_lib, "vkGetDeviceProcAddr"));
    q->vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(GetProcAddress(m_lib, "vkCreateDevice"));
    q->vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(GetProcAddress(m_lib, "vkDestroyDevice"));
    q->vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(GetProcAddress(m_lib, "vkEnumerateInstanceExtensionProperties"));
    q->vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(GetProcAddress(m_lib, "vkEnumerateDeviceExtensionProperties"));
    q->vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(GetProcAddress(m_lib, "vkEnumerateInstanceLayerProperties"));
    q->vkEnumerateDeviceLayerProperties = reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(GetProcAddress(m_lib, "vkEnumerateDeviceLayerProperties"));
    q->vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(GetProcAddress(m_lib, "vkGetDeviceQueue"));
    q->vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(GetProcAddress(m_lib, "vkQueueSubmit"));
    q->vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(GetProcAddress(m_lib, "vkQueueWaitIdle"));
    q->vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(GetProcAddress(m_lib, "vkDeviceWaitIdle"));
    q->vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(GetProcAddress(m_lib, "vkAllocateMemory"));
    q->vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(GetProcAddress(m_lib, "vkFreeMemory"));
    q->vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(GetProcAddress(m_lib, "vkMapMemory"));
    q->vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(GetProcAddress(m_lib, "vkUnmapMemory"));
    q->vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(GetProcAddress(m_lib, "vkFlushMappedMemoryRanges"));
    q->vkInvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(GetProcAddress(m_lib, "vkInvalidateMappedMemoryRanges"));
    q->vkGetDeviceMemoryCommitment = reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(GetProcAddress(m_lib, "vkGetDeviceMemoryCommitment"));
    q->vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(GetProcAddress(m_lib, "vkBindBufferMemory"));
    q->vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(GetProcAddress(m_lib, "vkBindImageMemory"));
    q->vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(GetProcAddress(m_lib, "vkGetBufferMemoryRequirements"));
    q->vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(GetProcAddress(m_lib, "vkGetImageMemoryRequirements"));
    q->vkGetImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(GetProcAddress(m_lib, "vkGetImageSparseMemoryRequirements"));
    q->vkGetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(GetProcAddress(m_lib, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    q->vkQueueBindSparse = reinterpret_cast<PFN_vkQueueBindSparse>(GetProcAddress(m_lib, "vkQueueBindSparse"));
    q->vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(GetProcAddress(m_lib, "vkCreateFence"));
    q->vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(GetProcAddress(m_lib, "vkDestroyFence"));
    q->vkResetFences = reinterpret_cast<PFN_vkResetFences>(GetProcAddress(m_lib, "vkResetFences"));
    q->vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(GetProcAddress(m_lib, "vkGetFenceStatus"));
    q->vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(GetProcAddress(m_lib, "vkWaitForFences"));
    q->vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(GetProcAddress(m_lib, "vkCreateSemaphore"));
    q->vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(GetProcAddress(m_lib, "vkDestroySemaphore"));
    q->vkCreateEvent = reinterpret_cast<PFN_vkCreateEvent>(GetProcAddress(m_lib, "vkCreateEvent"));
    q->vkDestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(GetProcAddress(m_lib, "vkDestroyEvent"));
    q->vkGetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(GetProcAddress(m_lib, "vkGetEventStatus"));
    q->vkSetEvent = reinterpret_cast<PFN_vkSetEvent>(GetProcAddress(m_lib, "vkSetEvent"));
    q->vkResetEvent = reinterpret_cast<PFN_vkResetEvent>(GetProcAddress(m_lib, "vkResetEvent"));
    q->vkCreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(GetProcAddress(m_lib, "vkCreateQueryPool"));
    q->vkDestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(GetProcAddress(m_lib, "vkDestroyQueryPool"));
    q->vkGetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(GetProcAddress(m_lib, "vkGetQueryPoolResults"));
    q->vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(GetProcAddress(m_lib, "vkCreateBuffer"));
    q->vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(GetProcAddress(m_lib, "vkDestroyBuffer"));
    q->vkCreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(GetProcAddress(m_lib, "vkCreateBufferView"));
    q->vkDestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(GetProcAddress(m_lib, "vkDestroyBufferView"));
    q->vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(GetProcAddress(m_lib, "vkCreateImage"));
    q->vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(GetProcAddress(m_lib, "vkDestroyImage"));
    q->vkGetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(GetProcAddress(m_lib, "vkGetImageSubresourceLayout"));
    q->vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(GetProcAddress(m_lib, "vkCreateImageView"));
    q->vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(GetProcAddress(m_lib, "vkDestroyImageView"));
    q->vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(GetProcAddress(m_lib, "vkCreateShaderModule"));
    q->vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(GetProcAddress(m_lib, "vkDestroyShaderModule"));
    q->vkCreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(GetProcAddress(m_lib, "vkCreatePipelineCache"));
    q->vkDestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(GetProcAddress(m_lib, "vkDestroyPipelineCache"));
    q->vkGetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(GetProcAddress(m_lib, "vkGetPipelineCacheData"));
    q->vkMergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(GetProcAddress(m_lib, "vkMergePipelineCaches"));
    q->vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(GetProcAddress(m_lib, "vkCreateGraphicsPipelines"));
    q->vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(GetProcAddress(m_lib, "vkCreateComputePipelines"));
    q->vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(GetProcAddress(m_lib, "vkDestroyPipeline"));
    q->vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(GetProcAddress(m_lib, "vkCreatePipelineLayout"));
    q->vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(GetProcAddress(m_lib, "vkDestroyPipelineLayout"));
    q->vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(GetProcAddress(m_lib, "vkCreateSampler"));
    q->vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(GetProcAddress(m_lib, "vkDestroySampler"));
    q->vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(GetProcAddress(m_lib, "vkCreateDescriptorSetLayout"));
    q->vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(GetProcAddress(m_lib, "vkDestroyDescriptorSetLayout"));
    q->vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(GetProcAddress(m_lib, "vkCreateDescriptorPool"));
    q->vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(GetProcAddress(m_lib, "vkDestroyDescriptorPool"));
    q->vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(GetProcAddress(m_lib, "vkResetDescriptorPool"));
    q->vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(GetProcAddress(m_lib, "vkAllocateDescriptorSets"));
    q->vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(GetProcAddress(m_lib, "vkFreeDescriptorSets"));
    q->vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(GetProcAddress(m_lib, "vkUpdateDescriptorSets"));
    q->vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(GetProcAddress(m_lib, "vkCreateFramebuffer"));
    q->vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(GetProcAddress(m_lib, "vkDestroyFramebuffer"));
    q->vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(GetProcAddress(m_lib, "vkCreateRenderPass"));
    q->vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(GetProcAddress(m_lib, "vkDestroyRenderPass"));
    q->vkGetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(GetProcAddress(m_lib, "vkGetRenderAreaGranularity"));
    q->vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(GetProcAddress(m_lib, "vkCreateCommandPool"));
    q->vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(GetProcAddress(m_lib, "vkDestroyCommandPool"));
    q->vkResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(GetProcAddress(m_lib, "vkResetCommandPool"));
    q->vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(GetProcAddress(m_lib, "vkAllocateCommandBuffers"));
    q->vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(GetProcAddress(m_lib, "vkFreeCommandBuffers"));
    q->vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(GetProcAddress(m_lib, "vkBeginCommandBuffer"));
    q->vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(GetProcAddress(m_lib, "vkEndCommandBuffer"));
    q->vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(GetProcAddress(m_lib, "vkResetCommandBuffer"));
    q->vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(GetProcAddress(m_lib, "vkCmdBindPipeline"));
    q->vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(GetProcAddress(m_lib, "vkCmdSetViewport"));
    q->vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(GetProcAddress(m_lib, "vkCmdSetScissor"));
    q->vkCmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(GetProcAddress(m_lib, "vkCmdSetLineWidth"));
    q->vkCmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(GetProcAddress(m_lib, "vkCmdSetDepthBias"));
    q->vkCmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(GetProcAddress(m_lib, "vkCmdSetBlendConstants"));
    q->vkCmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(GetProcAddress(m_lib, "vkCmdSetDepthBounds"));
    q->vkCmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(GetProcAddress(m_lib, "vkCmdSetStencilCompareMask"));
    q->vkCmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(GetProcAddress(m_lib, "vkCmdSetStencilWriteMask"));
    q->vkCmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(GetProcAddress(m_lib, "vkCmdSetStencilReference"));
    q->vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(GetProcAddress(m_lib, "vkCmdBindDescriptorSets"));
    q->vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(GetProcAddress(m_lib, "vkCmdBindIndexBuffer"));
    q->vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(GetProcAddress(m_lib, "vkCmdBindVertexBuffers"));
    q->vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(GetProcAddress(m_lib, "vkCmdDraw"));
    q->vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(GetProcAddress(m_lib, "vkCmdDrawIndexed"));
    q->vkCmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(GetProcAddress(m_lib, "vkCmdDrawIndirect"));
    q->vkCmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(GetProcAddress(m_lib, "vkCmdDrawIndexedIndirect"));
    q->vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(GetProcAddress(m_lib, "vkCmdDispatch"));
    q->vkCmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(GetProcAddress(m_lib, "vkCmdDispatchIndirect"));
    q->vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(GetProcAddress(m_lib, "vkCmdCopyBuffer"));
    q->vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(GetProcAddress(m_lib, "vkCmdCopyImage"));
    q->vkCmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(GetProcAddress(m_lib, "vkCmdBlitImage"));
    q->vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(GetProcAddress(m_lib, "vkCmdCopyBufferToImage"));
    q->vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(GetProcAddress(m_lib, "vkCmdCopyImageToBuffer"));
    q->vkCmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(GetProcAddress(m_lib, "vkCmdUpdateBuffer"));
    q->vkCmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(GetProcAddress(m_lib, "vkCmdFillBuffer"));
    q->vkCmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(GetProcAddress(m_lib, "vkCmdClearColorImage"));
    q->vkCmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(GetProcAddress(m_lib, "vkCmdClearDepthStencilImage"));
    q->vkCmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(GetProcAddress(m_lib, "vkCmdClearAttachments"));
    q->vkCmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(GetProcAddress(m_lib, "vkCmdResolveImage"));
    q->vkCmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(GetProcAddress(m_lib, "vkCmdSetEvent"));
    q->vkCmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(GetProcAddress(m_lib, "vkCmdResetEvent"));
    q->vkCmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(GetProcAddress(m_lib, "vkCmdWaitEvents"));
    q->vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(GetProcAddress(m_lib, "vkCmdPipelineBarrier"));
    q->vkCmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(GetProcAddress(m_lib, "vkCmdBeginQuery"));
    q->vkCmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(GetProcAddress(m_lib, "vkCmdEndQuery"));
    q->vkCmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(GetProcAddress(m_lib, "vkCmdResetQueryPool"));
    q->vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(GetProcAddress(m_lib, "vkCmdWriteTimestamp"));
    q->vkCmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(GetProcAddress(m_lib, "vkCmdCopyQueryPoolResults"));
    q->vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(GetProcAddress(m_lib, "vkCmdPushConstants"));
    q->vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(GetProcAddress(m_lib, "vkCmdBeginRenderPass"));
    q->vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(GetProcAddress(m_lib, "vkCmdNextSubpass"));
    q->vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(GetProcAddress(m_lib, "vkCmdEndRenderPass"));
    q->vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(GetProcAddress(m_lib, "vkCmdExecuteCommands"));
}

QVulkanFunctionsPrivate::~QVulkanFunctionsPrivate()
{
    FreeLibrary(m_lib);
}

QT_END_NAMESPACE
