/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtVulkan module
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "worker.h"
#include <QVulkanFunctions>
#include <QMatrix4x4>

static float vertexData[] = {
    0.0f, 0.5f,      1.0f, 0.0f, 0.0f,
    -0.5f, -0.5f,    0.0f, 1.0f, 0.0f,
    0.5f, -0.5f,     0.0f, 0.0f, 1.0f
};

static const int UNIFORM_DATA_SIZE = 16 * sizeof(float);

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
    return (v + byteAlign - 1) & ~(byteAlign - 1);
}

void Worker::init()
{
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
        m_cb[i] = VK_NULL_HANDLE;

    QVulkanFunctions *f = m_renderLoop->functions();
    VkDevice dev = m_renderLoop->device();

    // Prepare the vertex and uniform buffers. The vertex data will never
    // change so one buffer is sufficient regardless of the value of
    // FRAMES_IN_FLIGHT. Uniform data is changing per frame however so active
    // frames have to have a dedicated copy.

    // This is not OpenGL 2.0 anymore, so have just one memory allocation and
    // one buffer. We will then specify the appropriate offsets for uniform
    // buffers in the VkDescriptorBufferInfo. Have to watch out for
    // VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment, though.

    const VkDeviceSize uniAlign = m_renderLoop->physicalDeviceLimits()->minUniformBufferOffsetAlignment; // 256 bytes usually
    VkBufferCreateInfo bufInfo;
    memset(&bufInfo, 0, sizeof(bufInfo));
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    // Our internal layout is vertex, uniform, uniform, ... with each uniform buffer start offset aligned to uniAlign.
    const VkDeviceSize vertexAllocSize = aligned(sizeof(vertexData), uniAlign);
    const VkDeviceSize uniformAllocSize = aligned(UNIFORM_DATA_SIZE, uniAlign);
    bufInfo.size = vertexAllocSize + FRAMES_IN_FLIGHT * uniformAllocSize;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VkResult err = f->vkCreateBuffer(dev, &bufInfo, nullptr, &m_buf);
    if (err != VK_SUCCESS)
        qFatal("Failed to create buffer: %d", err);

    VkMemoryRequirements memReq;
    f->vkGetBufferMemoryRequirements(dev, m_buf, &memReq);

    VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        memReq.size,
        m_renderLoop->hostVisibleMemoryIndex()
    };

    err = f->vkAllocateMemory(dev, &memAllocInfo, nullptr, &m_bufMem);
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate memory: %d", err);

    err = f->vkBindBufferMemory(dev, m_buf, m_bufMem, 0);
    if (err != VK_SUCCESS)
        qFatal("Failed to bind buffer memory: %d", err);

    quint8 *p;
    err = f->vkMapMemory(dev, m_bufMem, 0, memReq.size, 0, reinterpret_cast<void **>(&p));
    if (err != VK_SUCCESS)
        qFatal("Failed to map memory: %d", err);
    memcpy(p, vertexData, sizeof(vertexData));
    p += vertexAllocSize;
    QMatrix4x4 ident;
    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i)
        memcpy(p + i * uniformAllocSize, ident.constData(), UNIFORM_DATA_SIZE);
    f->vkUnmapMemory(dev, m_bufMem);

    VkVertexInputBindingDescription vertexBindingDesc = {
        0, // binding
        5 * sizeof(float),
        VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        {
            0, // location
            0, // binding
            VK_FORMAT_R32G32_SFLOAT,
            0
        },
        {
            0,
            1,
            VK_FORMAT_R32G32B32_SFLOAT,
            2 * sizeof(float)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = vertexAttrDesc;

    // Create render pass.
    VkAttachmentDescription attDesc[2];
    memset(attDesc, 0, sizeof(attDesc));
    attDesc[0].format = m_renderLoop->swapChainFormat();
    attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attDesc[1].format = m_renderLoop->depthStencilFormat();
    attDesc[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // do not write out depth
    attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subPassDesc;
    memset(&subPassDesc, 0, sizeof(subPassDesc));
    subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDesc.colorAttachmentCount = 1;
    subPassDesc.pColorAttachments = &colorRef;
    subPassDesc.pDepthStencilAttachment = &dsRef;

    VkRenderPassCreateInfo rpInfo;
    memset(&rpInfo, 0, sizeof(rpInfo));
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attDesc;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subPassDesc;
    err = f->vkCreateRenderPass(dev, &rpInfo, nullptr, &m_renderPass);
    if (err != VK_SUCCESS)
        qFatal("Failed to create renderpass: %d", err);

    // Leave framebuffer creation to resize().
    for (size_t i = 0; i < sizeof(m_fb) / sizeof(VkFramebuffer); ++i)
        m_fb[i] = VK_NULL_HANDLE;

    // ###
}

void Worker::resize(const QSize &size)
{
    // Window size dependent resources are (re)created here. This function is
    // called once after init() and then whenever the window gets resized.

    QVulkanFunctions *f = m_renderLoop->functions();
    VkDevice dev = m_renderLoop->device();

    for (size_t i = 0; i < sizeof(m_fb) / sizeof(VkFramebuffer); ++i) {
        if (m_fb[i] != VK_NULL_HANDLE)
            f->vkDestroyFramebuffer(dev, m_fb[i], nullptr);
    }

    const int count = m_renderLoop->swapChainImageCount();
    Q_ASSERT(count <= sizeof(m_fb) / sizeof(VkFramebuffer));
    for (int i = 0; i < count; ++i) {
        VkImageView views[2] = {
            m_renderLoop->swapChainImageView(i),
            m_renderLoop->depthStencilImageView()
        };
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = views;
        fbInfo.width = size.width();
        fbInfo.height = size.height();
        fbInfo.layers = 1;
        VkResult err = f->vkCreateFramebuffer(dev, &fbInfo, nullptr, &m_fb[i]);
        if (err != VK_SUCCESS)
            qFatal("Failed to create framebuffer: %d", err);
    }
}

void Worker::cleanup()
{
    QVulkanFunctions *f = m_renderLoop->functions();
    VkDevice dev = m_renderLoop->device();

    for (int i = 0; i < m_renderLoop->swapChainImageCount(); ++i)
        f->vkDestroyFramebuffer(dev, m_fb[i], nullptr);

    f->vkDestroyRenderPass(dev, m_renderPass, nullptr);

    f->vkDestroyBuffer(dev, m_buf, nullptr);
    f->vkFreeMemory(dev, m_bufMem, nullptr);

    for (int i = 0; i < FRAMES_IN_FLIGHT; ++i) {
        if (m_cb[i] != VK_NULL_HANDLE) {
            f->vkFreeCommandBuffers(dev, m_renderLoop->commandPool(), 1, &m_cb[i]);
            m_cb[i] = VK_NULL_HANDLE;
        }
    }
}

void Worker::queueFrame(int frame, VkQueue queue, VkSemaphore waitSem, VkSemaphore signalSem)
{
    qDebug("worker queueFrame %d on thread %p", frame, QThread::currentThread()); // frame = 0 .. frames_in_flight - 1

    QVulkanFunctions *f = m_renderLoop->functions();
    VkDevice dev = m_renderLoop->device();
    VkImage img = m_renderLoop->swapChainImage(m_renderLoop->currentSwapChainImageIndex());

    // Free the command buffer used in frame no. current - frames_in_flight,
    // we know for sure that that frame has already finished.
    if (m_cb[frame] != VK_NULL_HANDLE)
        f->vkFreeCommandBuffers(dev, m_renderLoop->commandPool(), 1, &m_cb[frame]);

    VkCommandBufferAllocateInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_renderLoop->commandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkResult err = f->vkAllocateCommandBuffers(dev, &cmdBufInfo, &m_cb[frame]);
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate command buffer: %d", err);

    VkCommandBufferBeginInfo cmdBufBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
    err = f->vkBeginCommandBuffer(m_cb[frame], &cmdBufBeginInfo);
    if (err != VK_SUCCESS)
        qFatal("Failed to begin command buffer: %d", err);

    VkClearColorValue clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    VkImageSubresourceRange subResRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    f->vkCmdClearColorImage(m_cb[frame], img, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subResRange);

    // ###

    err = f->vkEndCommandBuffer(m_cb[frame]);
    if (err != VK_SUCCESS)
        qFatal("Failed to end command buffer: %d", err);

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_cb[frame];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSem;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSem;
    VkPipelineStageFlags psf = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    submitInfo.pWaitDstStageMask = &psf;
    err = f->vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (err != VK_SUCCESS)
        qFatal("Failed to submit to command queue: %d", err);

#ifndef TEST_ASYNC
    // All command buffer have been submitted with correct wait/signal
    // settings. Notify the renderloop that we are done. We could also have
    // gone async, spawn some threads, and so on. All that matters is to call
    // frameQueued (on any thread) at some point.
    m_renderLoop->frameQueued();
#else
    // Note that one thing we cannot do on this thread is to use timers or
    // other stuff relying on the Qt event loop. This is because while the the
    // thread spins the Qt loop between frames, it will now go to sleep until
    // frameQueued() is called. However, nothing prevents us from spawning as
    // many threads as we want.
    QThreadPool::globalInstance()->start(new AsyncFrameTest(this));
#endif

    // Could schedule updates manually if we did not have UpdateContinuously set:
    // m_renderLoop->update();
}
