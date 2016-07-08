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

#ifndef QVULKANRENDERLOOP_P_H
#define QVULKANRENDERLOOP_P_H

#include "qvulkanrenderloop.h"
#include <QObject>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

class QVulkanRenderLoopPrivate : public QObject
{
    Q_OBJECT

public:
    QVulkanRenderLoopPrivate(QWindow *window);

    bool eventFilter(QObject *watched, QEvent *event) override;
    void update();
    QWindow *m_window;

    void init();
    void cleanup();
    void recreateSwapChain();
    void ensureFrameCmdBuf(int frame, int subIndex);
    void submitFrameCmdBuf(VkSemaphore waitSem, VkSemaphore signalSem, int subIndex, bool fence);
    bool beginFrame();
    void endFrame();
    void renderFrame();

    void createDeviceAndSurface();
    void releaseDeviceAndSurface();
    void createSurface();
    void releaseSurface();
    bool physicalDeviceSupportsPresent(int queueFamilyIdx);

    void transitionImage(VkCommandBuffer cmdBuf, VkImage image,
                         VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, bool ds = false);

    WId m_winId;
#ifdef Q_OS_LINUX
    xcb_connection_t *m_xcbConnection;
    xcb_visualid_t m_xcbVisualId;
#endif
    QSize m_windowSize;
    QVulkanRenderLoop::Flags m_flags;
    int m_framesInFlight;
    bool m_inited = false;
    QVulkanFunctions *f;

    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
    PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;

    VkFormat m_colorFormat;
    VkInstance m_vkInst;
    VkPhysicalDevice m_vkPhysDev;
    VkPhysicalDeviceMemoryProperties m_vkPhysDevMemProps;
    VkDevice m_vkDev;
    VkQueue m_vkQueue;
    VkCommandPool m_vkCmdPool;
    uint32_t m_hostVisibleMemIndex;
    bool m_hasDebug;
    VkDebugReportCallbackEXT m_debugCallback;

    VkSurfaceKHR m_surface;
    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    uint32_t m_swapChainBufferCount = 0;

    static const int MAX_SWAPCHAIN_BUFFERS = 3;
    static const int MAX_FRAMES_IN_FLIGHT = 3;

    VkImage m_swapChainImages[MAX_SWAPCHAIN_BUFFERS];
    VkImageView m_swapChainImageViews[MAX_SWAPCHAIN_BUFFERS];
    VkDeviceMemory m_dsMem = VK_NULL_HANDLE;
    VkImage m_ds;
    VkImageView m_dsView;

    bool m_frameActive;
    VkSemaphore m_acquireSem[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore m_renderSem[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore m_workerWaitSem[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore m_workerSignalSem[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer m_frameCmdBuf[MAX_FRAMES_IN_FLIGHT][2];
    bool m_frameCmdBufRecording[MAX_FRAMES_IN_FLIGHT];
    VkFence m_frameFence[MAX_FRAMES_IN_FLIGHT];
    bool m_frameFenceActive[MAX_FRAMES_IN_FLIGHT];

    uint32_t m_currentSwapChainBuffer;
    uint32_t m_currentFrame;

#if defined(Q_OS_WIN)
    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#elif defined(Q_OS_LINUX)
    PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
    PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;

    QVulkanFrameWorker *m_worker = nullptr;

public slots:
    void onDestroyed(QObject *obj);
    void onQueued();
};

QT_END_NAMESPACE

#endif // QVULKANRENDERLOOP_P_H
