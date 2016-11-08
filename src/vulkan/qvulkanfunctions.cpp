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
#include <QLibrary>

QT_BEGIN_NAMESPACE

class QVulkanFunctionsPrivate
{
public:
    QVulkanFunctionsPrivate(QVulkanFunctions *q_ptr);
    ~QVulkanFunctionsPrivate();

    QVulkanFunctions *q;

    QLibrary m_lib;
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
    m_lib.setFileName(QStringLiteral("vulkan-1"));
    if (qEnvironmentVariableIsSet("QT_VULKAN_LIB"))
        m_lib.setFileName(QString::fromUtf8(qgetenv("QT_VULKAN_LIB")));

    if (!m_lib.load())
        qFatal("Failed to load %s", qPrintable(m_lib.fileName()));

    q->vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(m_lib.resolve("vkCreateInstance"));
    q->vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(m_lib.resolve("vkDestroyInstance"));
    q->vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(m_lib.resolve("vkEnumeratePhysicalDevices"));
    q->vkGetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(m_lib.resolve("vkGetPhysicalDeviceFeatures"));
    q->vkGetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(m_lib.resolve("vkGetPhysicalDeviceFormatProperties"));
    q->vkGetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(m_lib.resolve("vkGetPhysicalDeviceImageFormatProperties"));
    q->vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(m_lib.resolve("vkGetPhysicalDeviceProperties"));
    q->vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(m_lib.resolve("vkGetPhysicalDeviceQueueFamilyProperties"));
    q->vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(m_lib.resolve("vkGetPhysicalDeviceMemoryProperties"));
    q->vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(m_lib.resolve("vkGetInstanceProcAddr"));
    q->vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_lib.resolve("vkGetDeviceProcAddr"));
    q->vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_lib.resolve("vkCreateDevice"));
    q->vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(m_lib.resolve("vkDestroyDevice"));
    q->vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(m_lib.resolve("vkEnumerateInstanceExtensionProperties"));
    q->vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(m_lib.resolve("vkEnumerateDeviceExtensionProperties"));
    q->vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(m_lib.resolve("vkEnumerateInstanceLayerProperties"));
    q->vkEnumerateDeviceLayerProperties = reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(m_lib.resolve("vkEnumerateDeviceLayerProperties"));
    q->vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(m_lib.resolve("vkGetDeviceQueue"));
    q->vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(m_lib.resolve("vkQueueSubmit"));
    q->vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(m_lib.resolve("vkQueueWaitIdle"));
    q->vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(m_lib.resolve("vkDeviceWaitIdle"));
    q->vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(m_lib.resolve("vkAllocateMemory"));
    q->vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(m_lib.resolve("vkFreeMemory"));
    q->vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(m_lib.resolve("vkMapMemory"));
    q->vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(m_lib.resolve("vkUnmapMemory"));
    q->vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(m_lib.resolve("vkFlushMappedMemoryRanges"));
    q->vkInvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(m_lib.resolve("vkInvalidateMappedMemoryRanges"));
    q->vkGetDeviceMemoryCommitment = reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(m_lib.resolve("vkGetDeviceMemoryCommitment"));
    q->vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(m_lib.resolve("vkBindBufferMemory"));
    q->vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(m_lib.resolve("vkBindImageMemory"));
    q->vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(m_lib.resolve("vkGetBufferMemoryRequirements"));
    q->vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(m_lib.resolve("vkGetImageMemoryRequirements"));
    q->vkGetImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(m_lib.resolve("vkGetImageSparseMemoryRequirements"));
    q->vkGetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(m_lib.resolve("vkGetPhysicalDeviceSparseImageFormatProperties"));
    q->vkQueueBindSparse = reinterpret_cast<PFN_vkQueueBindSparse>(m_lib.resolve("vkQueueBindSparse"));
    q->vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(m_lib.resolve("vkCreateFence"));
    q->vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(m_lib.resolve("vkDestroyFence"));
    q->vkResetFences = reinterpret_cast<PFN_vkResetFences>(m_lib.resolve("vkResetFences"));
    q->vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(m_lib.resolve("vkGetFenceStatus"));
    q->vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(m_lib.resolve("vkWaitForFences"));
    q->vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(m_lib.resolve("vkCreateSemaphore"));
    q->vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(m_lib.resolve("vkDestroySemaphore"));
    q->vkCreateEvent = reinterpret_cast<PFN_vkCreateEvent>(m_lib.resolve("vkCreateEvent"));
    q->vkDestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(m_lib.resolve("vkDestroyEvent"));
    q->vkGetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(m_lib.resolve("vkGetEventStatus"));
    q->vkSetEvent = reinterpret_cast<PFN_vkSetEvent>(m_lib.resolve("vkSetEvent"));
    q->vkResetEvent = reinterpret_cast<PFN_vkResetEvent>(m_lib.resolve("vkResetEvent"));
    q->vkCreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(m_lib.resolve("vkCreateQueryPool"));
    q->vkDestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(m_lib.resolve("vkDestroyQueryPool"));
    q->vkGetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(m_lib.resolve("vkGetQueryPoolResults"));
    q->vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(m_lib.resolve("vkCreateBuffer"));
    q->vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(m_lib.resolve("vkDestroyBuffer"));
    q->vkCreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(m_lib.resolve("vkCreateBufferView"));
    q->vkDestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(m_lib.resolve("vkDestroyBufferView"));
    q->vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(m_lib.resolve("vkCreateImage"));
    q->vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(m_lib.resolve("vkDestroyImage"));
    q->vkGetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(m_lib.resolve("vkGetImageSubresourceLayout"));
    q->vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(m_lib.resolve("vkCreateImageView"));
    q->vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_lib.resolve("vkDestroyImageView"));
    q->vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(m_lib.resolve("vkCreateShaderModule"));
    q->vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(m_lib.resolve("vkDestroyShaderModule"));
    q->vkCreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(m_lib.resolve("vkCreatePipelineCache"));
    q->vkDestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(m_lib.resolve("vkDestroyPipelineCache"));
    q->vkGetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(m_lib.resolve("vkGetPipelineCacheData"));
    q->vkMergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(m_lib.resolve("vkMergePipelineCaches"));
    q->vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(m_lib.resolve("vkCreateGraphicsPipelines"));
    q->vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(m_lib.resolve("vkCreateComputePipelines"));
    q->vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(m_lib.resolve("vkDestroyPipeline"));
    q->vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(m_lib.resolve("vkCreatePipelineLayout"));
    q->vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(m_lib.resolve("vkDestroyPipelineLayout"));
    q->vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(m_lib.resolve("vkCreateSampler"));
    q->vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(m_lib.resolve("vkDestroySampler"));
    q->vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(m_lib.resolve("vkCreateDescriptorSetLayout"));
    q->vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(m_lib.resolve("vkDestroyDescriptorSetLayout"));
    q->vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(m_lib.resolve("vkCreateDescriptorPool"));
    q->vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(m_lib.resolve("vkDestroyDescriptorPool"));
    q->vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(m_lib.resolve("vkResetDescriptorPool"));
    q->vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(m_lib.resolve("vkAllocateDescriptorSets"));
    q->vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(m_lib.resolve("vkFreeDescriptorSets"));
    q->vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(m_lib.resolve("vkUpdateDescriptorSets"));
    q->vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(m_lib.resolve("vkCreateFramebuffer"));
    q->vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(m_lib.resolve("vkDestroyFramebuffer"));
    q->vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(m_lib.resolve("vkCreateRenderPass"));
    q->vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(m_lib.resolve("vkDestroyRenderPass"));
    q->vkGetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(m_lib.resolve("vkGetRenderAreaGranularity"));
    q->vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(m_lib.resolve("vkCreateCommandPool"));
    q->vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(m_lib.resolve("vkDestroyCommandPool"));
    q->vkResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(m_lib.resolve("vkResetCommandPool"));
    q->vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(m_lib.resolve("vkAllocateCommandBuffers"));
    q->vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(m_lib.resolve("vkFreeCommandBuffers"));
    q->vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(m_lib.resolve("vkBeginCommandBuffer"));
    q->vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(m_lib.resolve("vkEndCommandBuffer"));
    q->vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(m_lib.resolve("vkResetCommandBuffer"));
    q->vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(m_lib.resolve("vkCmdBindPipeline"));
    q->vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(m_lib.resolve("vkCmdSetViewport"));
    q->vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(m_lib.resolve("vkCmdSetScissor"));
    q->vkCmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(m_lib.resolve("vkCmdSetLineWidth"));
    q->vkCmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(m_lib.resolve("vkCmdSetDepthBias"));
    q->vkCmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(m_lib.resolve("vkCmdSetBlendConstants"));
    q->vkCmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(m_lib.resolve("vkCmdSetDepthBounds"));
    q->vkCmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(m_lib.resolve("vkCmdSetStencilCompareMask"));
    q->vkCmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(m_lib.resolve("vkCmdSetStencilWriteMask"));
    q->vkCmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(m_lib.resolve("vkCmdSetStencilReference"));
    q->vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(m_lib.resolve("vkCmdBindDescriptorSets"));
    q->vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(m_lib.resolve("vkCmdBindIndexBuffer"));
    q->vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(m_lib.resolve("vkCmdBindVertexBuffers"));
    q->vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(m_lib.resolve("vkCmdDraw"));
    q->vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(m_lib.resolve("vkCmdDrawIndexed"));
    q->vkCmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(m_lib.resolve("vkCmdDrawIndirect"));
    q->vkCmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(m_lib.resolve("vkCmdDrawIndexedIndirect"));
    q->vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(m_lib.resolve("vkCmdDispatch"));
    q->vkCmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(m_lib.resolve("vkCmdDispatchIndirect"));
    q->vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(m_lib.resolve("vkCmdCopyBuffer"));
    q->vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(m_lib.resolve("vkCmdCopyImage"));
    q->vkCmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(m_lib.resolve("vkCmdBlitImage"));
    q->vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(m_lib.resolve("vkCmdCopyBufferToImage"));
    q->vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(m_lib.resolve("vkCmdCopyImageToBuffer"));
    q->vkCmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(m_lib.resolve("vkCmdUpdateBuffer"));
    q->vkCmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(m_lib.resolve("vkCmdFillBuffer"));
    q->vkCmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(m_lib.resolve("vkCmdClearColorImage"));
    q->vkCmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(m_lib.resolve("vkCmdClearDepthStencilImage"));
    q->vkCmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(m_lib.resolve("vkCmdClearAttachments"));
    q->vkCmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(m_lib.resolve("vkCmdResolveImage"));
    q->vkCmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(m_lib.resolve("vkCmdSetEvent"));
    q->vkCmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(m_lib.resolve("vkCmdResetEvent"));
    q->vkCmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(m_lib.resolve("vkCmdWaitEvents"));
    q->vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(m_lib.resolve("vkCmdPipelineBarrier"));
    q->vkCmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(m_lib.resolve("vkCmdBeginQuery"));
    q->vkCmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(m_lib.resolve("vkCmdEndQuery"));
    q->vkCmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(m_lib.resolve("vkCmdResetQueryPool"));
    q->vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(m_lib.resolve("vkCmdWriteTimestamp"));
    q->vkCmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(m_lib.resolve("vkCmdCopyQueryPoolResults"));
    q->vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(m_lib.resolve("vkCmdPushConstants"));
    q->vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(m_lib.resolve("vkCmdBeginRenderPass"));
    q->vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(m_lib.resolve("vkCmdNextSubpass"));
    q->vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(m_lib.resolve("vkCmdEndRenderPass"));
    q->vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(m_lib.resolve("vkCmdExecuteCommands"));
}

QVulkanFunctionsPrivate::~QVulkanFunctionsPrivate()
{
    m_lib.unload();
}

QT_END_NAMESPACE
