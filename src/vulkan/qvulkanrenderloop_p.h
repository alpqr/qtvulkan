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
#include <queue>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

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

class QVulkanRenderThread;
class QVulkanRenderThreadEvent;

class QVulkanRenderLoopPrivate : public QObject
{
public:
    QVulkanRenderLoopPrivate(QVulkanRenderLoop *q_ptr, QWindow *window);
    ~QVulkanRenderLoopPrivate();

    bool eventFilter(QObject *watched, QEvent *event) override;

    void postThreadEvent(QVulkanRenderThreadEvent *e, bool needsLock = true);

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

    QVulkanRenderLoop *q;
    QWindow *m_window;
    QVulkanRenderLoop::Flags m_flags = 0;
    int m_framesInFlight = 1;
    QVulkanRenderThread *m_thread = nullptr;
    QVulkanFrameWorker *m_worker = nullptr;
    QVulkanFunctions *f;

    WId m_winId;
    std::pair<uint32_t, uint32_t> m_windowSize;
    bool m_inited = false;

    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
    PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;

    VkFormat m_colorFormat;
    VkInstance m_vkInst;
    VkPhysicalDevice m_vkPhysDev;
    VkPhysicalDeviceProperties m_physDevProps;
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

    std::chrono::time_point<std::chrono::steady_clock> m_beginFrameTimePoint;

#if defined(Q_OS_WIN)
    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
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
};

class QVulkanRenderThreadEvent
{
public:
    QVulkanRenderThreadEvent(int type) : m_type(type) { }
    int type() const { return m_type; }
    static const int Base = 1;

private:
    int m_type;
};

class QVulkanRenderThreadExposeEvent : public QVulkanRenderThreadEvent
{
public:
    static const int Type = QVulkanRenderThreadEvent::Base + 1;
    QVulkanRenderThreadExposeEvent() : QVulkanRenderThreadEvent(Type) { }
};

class QVulkanRenderThreadObscureEvent : public QVulkanRenderThreadEvent
{
public:
    static const int Type = QVulkanRenderThreadEvent::Base + 2;
    QVulkanRenderThreadObscureEvent() : QVulkanRenderThreadEvent(Type) { }
};

class QVulkanRenderThreadResizeEvent : public QVulkanRenderThreadEvent
{
public:
    static const int Type = QVulkanRenderThreadEvent::Base + 3;
    QVulkanRenderThreadResizeEvent() : QVulkanRenderThreadEvent(Type) { }
};

class QVulkanRenderThreadUpdateEvent : public QVulkanRenderThreadEvent
{
public:
    static const int Type = QVulkanRenderThreadEvent::Base + 4;
    QVulkanRenderThreadUpdateEvent() : QVulkanRenderThreadEvent(Type) { }
};

class QVulkanRenderThreadFrameQueuedEvent : public QVulkanRenderThreadEvent
{
public:
    static const int Type = QVulkanRenderThreadEvent::Base + 5;
    QVulkanRenderThreadFrameQueuedEvent() : QVulkanRenderThreadEvent(Type) { }
};

class QVulkanRenderThreadDestroyEvent : public QVulkanRenderThreadEvent
{
public:
    static const int Type = QVulkanRenderThreadEvent::Base + 6;
    QVulkanRenderThreadDestroyEvent() : QVulkanRenderThreadEvent(Type) { }
};

class QVulkanRenderThreadEventQueue
{
public:
    void addEvent(QVulkanRenderThreadEvent *e);
    QVulkanRenderThreadEvent *takeEvent(bool wait);
    bool hasMoreEvents();

private:
    std::queue<QVulkanRenderThreadEvent *> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_waiting = false;
};

class QVulkanRenderThread
{
public:
    QVulkanRenderThread(QVulkanRenderLoopPrivate *d_ptr) : d(d_ptr) { }

    void start();

    void processEvents();
    void processEventsAndWaitForMore();
    void postEvent(QVulkanRenderThreadEvent *e);

    std::thread::id id() const { return t.get_id(); }
    void join() { t.join(); }
    std::mutex *mutex() { return &m_mutex; }
    std::condition_variable *waitCondition() { return &m_condition; }
    void setActive() { m_active = true; }
    void setUpdatePending();

private:
    void run();
    void processEvent(QVulkanRenderThreadEvent *e);
    void obscure();
    void resize();

    std::thread t;
    QVulkanRenderLoopPrivate *d;
    QVulkanRenderThreadEventQueue m_eventQueue;
    volatile bool m_active;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    uint m_sleeping : 1;
    uint m_stopEventProcessing : 1;
    uint m_pendingUpdate : 1;
    uint m_pendingObscure : 1;
    uint m_pendingResize : 1;
    uint m_pendingDestroy : 1;
};

QT_END_NAMESPACE

#endif // QVULKANRENDERLOOP_P_H
