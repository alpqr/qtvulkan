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

#ifndef QVULKANRENDERLOOP_H
#define QVULKANRENDERLOOP_H

#include <QtVulkan/qtvulkanglobal.h>
#include <QtVulkan/qvulkan.h>
#include <QWindow>

QT_BEGIN_NAMESPACE

class QVulkanRenderLoopPrivate;
class QVulkanFunctions;

class Q_VULKAN_EXPORT QVulkanFrameWorker
{
public:
    virtual ~QVulkanFrameWorker() { }
    virtual void init() = 0;
    virtual void resize(uint32_t w, uint32_t h) = 0;
    virtual void cleanup() = 0;
    virtual void queueFrame(uint32_t frame, VkQueue queue, VkSemaphore waitSem, VkSemaphore signalSem) = 0;
};

class Q_VULKAN_EXPORT QVulkanRenderLoop
{
public:
    enum Flag {
        EnableValidation = 0x01,
        Unthrottled = 0x02,
        UpdateContinuously = 0x04,
        DontReleaseOnObscure = 0x08,
        TrippleBuffer = 0x10
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QVulkanRenderLoop(QWindow *window);
    ~QVulkanRenderLoop();

    void setFlags(Flags flags);
    void setFramesInFlight(int frameCount);
    void setWorker(QVulkanFrameWorker *worker);

    void update();

    // for QVulkanFrameWorker
    void frameQueued();
    QVulkanFunctions *functions();

    VkInstance instance() const;
    VkPhysicalDevice physicalDevice() const;
    const VkPhysicalDeviceLimits *physicalDeviceLimits() const;
    uint32_t hostVisibleMemoryIndex() const;
    VkDevice device() const;
    VkCommandPool commandPool() const;

    int swapChainImageCount() const;
    int currentSwapChainImageIndex() const;
    VkImage swapChainImage(int idx) const;
    VkImageView swapChainImageView(int idx) const;
    VkFormat swapChainFormat() const;
    VkImage depthStencilImage() const;
    VkImageView depthStencilImageView() const;
    VkFormat depthStencilFormat() const;

private:
    QVulkanRenderLoopPrivate *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVulkanRenderLoop::Flags)

QT_END_NAMESPACE

#endif // QVULKANRENDERLOOP_H
