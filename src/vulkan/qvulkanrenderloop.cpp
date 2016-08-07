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

#include "qvulkanrenderloop_p.h"
#include <QVulkanFunctions>
#include <vector>

QT_BEGIN_NAMESPACE


/*
    Command buffer and queue behavior without a QVulkanFrameWorker set:

    1. CPU wait for fence -> this is where the thread is throttled to vsync if FIFO

    2. Build command buffer:
      - prologue (transitions) from beginFrame, or init+beginFrame if this is the first frame after init
      - renderFrame adds a clear
      - epilogue (transitions) from endFrame

    3. SUBMIT with wait for acquire; signal renderDone; signal fence

    4. PRESENT with wait for renderDone

    With N frames in flight this needs N command buffers, semaphores and fences.

    *******************

    With a QVulkanFrameWorker set:

    1. CPU wait for fence

    2. Build command buffer A with prologue (transitions)

    3. SUBMIT command buffer A with wait for acquire; signal workerWait; no fence

    4. ask the worker to SUBMIT one or more command buffers with the first one waiting for workerWait, the last one signaling workerSignal, no fence
       (async; the worker can spawn worker threads if it wants to; must emit queued() after its last vkQueueSubmit)

    5. Build command buffer B with epilogue (transitions)

    6. SUBMIT command buffer B with wait for workerSignal; signal renderDone; signal fence

    7. PRESENT with wait for renderDone

    Here the prologue and epilogue need an extra command buffer per frame-in-flight since they are submitted separately.
 */

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#define DECLARE_DEBUG_VAR(variable) \
    static inline bool debug_ ## variable() \
    { static bool value = !strcmp(getenv("QVULKAN_DEBUG"), STRINGIFY(variable)); return value; }

DECLARE_DEBUG_VAR(render)

void log(const char *fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buf, sizeof(buf) - 1, fmt, args);
    strcat(buf, "\n");
    OutputDebugStringA(buf);
}

QVulkanRenderLoop::QVulkanRenderLoop(QWindow *window)
    : d(new QVulkanRenderLoopPrivate(this, window))
{
}

QVulkanRenderLoop::~QVulkanRenderLoop()
{
    delete d;
}

QVulkanFunctions *QVulkanRenderLoop::functions()
{
    return d->f;
}

void QVulkanRenderLoop::setFlags(Flags flags)
{
    if (d->m_inited) {
        log("Cannot change flags after rendering has started");
        return;
    }
    d->m_flags = flags;
}

void QVulkanRenderLoop::setFramesInFlight(int frameCount)
{
    if (d->m_inited) {
        log("Cannot change number of frames in flight after rendering has started");
        return;
    }
    if (frameCount < 1 || frameCount > QVulkanRenderLoopPrivate::MAX_FRAMES_IN_FLIGHT) {
        log("Invalid frames-in-flight count");
        return;
    }
    d->m_framesInFlight = frameCount;
}

void QVulkanRenderLoop::setWorker(QVulkanFrameWorker *worker)
{
    if (d->m_inited) {
        log("Cannot change worker after rendering has started");
        return;
    }
    if (worker == d->m_worker)
        return;
    d->m_worker = worker;
}

void QVulkanRenderLoop::update()
{
    if (!d->m_inited)
        return;

    if (std::this_thread::get_id() == d->m_thread->id())
        d->m_thread->setUpdatePending();
    else
        d->postThreadEvent(new QVulkanRenderThreadUpdateEvent);
}

void QVulkanRenderLoop::frameQueued()
{
    if (!d->m_inited)
        return;

    if (std::this_thread::get_id() == d->m_thread->id())
        d->endFrame();
    else
        d->postThreadEvent(new QVulkanRenderThreadFrameQueuedEvent);
}

VkInstance QVulkanRenderLoop::instance() const
{
    return d->m_vkInst;
}

VkPhysicalDevice QVulkanRenderLoop::physicalDevice() const
{
    return d->m_vkPhysDev;
}

const VkPhysicalDeviceLimits *QVulkanRenderLoop::physicalDeviceLimits() const
{
    return &d->m_physDevProps.limits;
}

uint32_t QVulkanRenderLoop::hostVisibleMemoryIndex() const
{
    return d->m_hostVisibleMemIndex;
}

VkDevice QVulkanRenderLoop::device() const
{
    return d->m_vkDev;
}

VkCommandPool QVulkanRenderLoop::commandPool() const
{
    return d->m_vkCmdPool;
}

int QVulkanRenderLoop::swapChainImageCount() const
{
    return d->m_swapChainBufferCount;
}

int QVulkanRenderLoop::currentSwapChainImageIndex() const
{
    return d->m_currentSwapChainBuffer;
}

VkImage QVulkanRenderLoop::swapChainImage(int idx) const
{
    return d->m_swapChainImages[idx];
}

VkImageView QVulkanRenderLoop::swapChainImageView(int idx) const
{
    return d->m_swapChainImageViews[idx];
}

VkFormat QVulkanRenderLoop::swapChainFormat() const
{
    return d->m_colorFormat;
}

VkImage QVulkanRenderLoop::depthStencilImage() const
{
    return d->m_ds;
}

VkImageView QVulkanRenderLoop::depthStencilImageView() const
{
    return d->m_dsView;
}

VkFormat QVulkanRenderLoop::depthStencilFormat() const
{
    return VK_FORMAT_D24_UNORM_S8_UINT;
}

QVulkanRenderLoopPrivate::QVulkanRenderLoopPrivate(QVulkanRenderLoop *q_ptr, QWindow *window)
    : q(q_ptr),
      m_window(window),
      f(QVulkanFunctions::instance())
{
    window->installEventFilter(this);
}

QVulkanRenderLoopPrivate::~QVulkanRenderLoopPrivate()
{
    if (m_thread) {
        postThreadEvent(new QVulkanRenderThreadDestroyEvent);
        m_thread->join();
        delete m_thread;
    }
}

bool QVulkanRenderLoopPrivate::eventFilter(QObject *watched, QEvent *event)
{
    QWindow *window = static_cast<QWindow *>(watched);
    if (event->type() == QEvent::Expose) {
        if (window->isExposed()) {
            if (!m_thread) {
                m_thread = new QVulkanRenderThread(this);
                m_thread->setActive();
                m_thread->start();
            }
            m_thread->mutex()->lock();
            m_winId = window->winId();
            const QSize wsize = window->size();
            m_windowSize = std::make_pair(wsize.width(), wsize.height());
            postThreadEvent(new QVulkanRenderThreadExposeEvent, false);
        } else if (m_inited) {
            postThreadEvent(new QVulkanRenderThreadObscureEvent);
        }
    } else if (event->type() == QEvent::Resize) {
        if (m_inited && window->isExposed()) {
            m_thread->mutex()->lock();
            const QSize wsize = window->size();
            m_windowSize = std::make_pair(wsize.width(), wsize.height());
            postThreadEvent(new QVulkanRenderThreadResizeEvent, false);
        }
    }

    return false;
}

void QVulkanRenderLoopPrivate::postThreadEvent(QVulkanRenderThreadEvent *e, bool needsLock)
{
    std::unique_lock<std::mutex> lock(*m_thread->mutex(), std::defer_lock);
    if (needsLock)
        lock.lock();
    m_thread->postEvent(e);
    m_thread->waitCondition()->wait(lock);
    if (!needsLock)
        m_thread->mutex()->unlock();
}

void QVulkanRenderThreadEventQueue::addEvent(QVulkanRenderThreadEvent *e)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.push(e);
    if (m_waiting)
        m_condition.notify_one();
}

QVulkanRenderThreadEvent *QVulkanRenderThreadEventQueue::takeEvent(bool wait)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_queue.empty() && wait) {
        m_waiting = true;
        m_condition.wait(lock);
        m_waiting = false;
    }
    QVulkanRenderThreadEvent *e = m_queue.front();
    m_queue.pop();
    return e;
}

bool QVulkanRenderThreadEventQueue::hasMoreEvents()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return !m_queue.empty();
}

void QVulkanRenderThread::postEvent(QVulkanRenderThreadEvent *e)
{
    m_eventQueue.addEvent(e);
}

void QVulkanRenderThread::processEvents()
{
    while (m_eventQueue.hasMoreEvents()) {
        QVulkanRenderThreadEvent *e = m_eventQueue.takeEvent(false);
        processEvent(e);
        delete e;
    }
}

void QVulkanRenderThread::processEventsAndWaitForMore()
{
    m_stopEventProcessing = false;
    while (!m_stopEventProcessing) {
        QVulkanRenderThreadEvent *e = m_eventQueue.takeEvent(true);
        processEvent(e);
        delete e;
    }
}

void QVulkanRenderThread::processEvent(QVulkanRenderThreadEvent *e)
{
    switch (e->type()) {
    case QVulkanRenderThreadExposeEvent::Type:
        m_mutex.lock();
        if (debug_render())
            log("render thread - expose");
        if (!d->m_inited)
            d->init();
        m_pendingUpdate = true;
        if (m_sleeping)
            m_stopEventProcessing = true;
        m_condition.notify_one();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadObscureEvent::Type:
        m_mutex.lock();
        if (!d->m_frameActive) {
            if (debug_render())
                log("render thread - obscure");
            obscure();
        } else {
            m_pendingObscure = true;
        }
        m_condition.notify_one();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadResizeEvent::Type:
        m_mutex.lock();
        if (!d->m_frameActive) {
            if (debug_render())
                log("render thread - resize");
            resize();
        } else {
            m_pendingResize = true;
        }
        m_condition.notify_one();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadUpdateEvent::Type:
        m_mutex.lock();
        setUpdatePending();
        m_condition.notify_one();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadFrameQueuedEvent::Type:
        m_mutex.lock();
        if (debug_render())
            log("render thread - worker ready");
        d->endFrame();
        if (m_sleeping)
            m_stopEventProcessing = true;
        m_condition.notify_one();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadDestroyEvent::Type:
        m_mutex.lock();
        if (!d->m_frameActive) {
            if (debug_render())
                log("render thread - destroy");
            m_active = false;
            if (m_sleeping)
                m_stopEventProcessing = true;
        } else {
            m_pendingDestroy = true;
        }
        m_condition.notify_one();
        m_mutex.unlock();
        break;
    default:
        log("Unknown render thread event %d", e->type());
        break;
    }
}

void QVulkanRenderThread::setUpdatePending()
{
    m_pendingUpdate = true;
    if (m_sleeping)
        m_stopEventProcessing = true;
}

void QVulkanRenderThread::obscure()
{
    if (!d->m_inited)
        return;

    if (!d->m_flags.testFlag(QVulkanRenderLoop::DontReleaseOnObscure)) {
        d->f->vkDeviceWaitIdle(d->m_vkDev);
        d->cleanup();
    }

    m_pendingUpdate = false;
}

void QVulkanRenderThread::resize()
{
    if (!d->m_inited)
        return;

    d->f->vkDeviceWaitIdle(d->m_vkDev);
    d->recreateSwapChain();

    if (d->m_worker)
        d->m_worker->resize(d->m_windowSize.first, d->m_windowSize.second);
}

void QVulkanRenderThread::start()
{
    t = std::thread(&QVulkanRenderThread::run, this);
}

void QVulkanRenderThread::run()
{
    if (debug_render())
        log("render thread - start");

    m_sleeping = false;
    m_stopEventProcessing = false;
    m_pendingUpdate = false;
    m_pendingObscure = false;
    m_pendingResize = false;
    m_pendingDestroy = false;

    while (m_active) {
        if (m_pendingDestroy) {
            if (debug_render())
                log("render thread - processing pending destroy");
            m_active = false;
            continue;
        }
        if (m_pendingObscure && !d->m_frameActive) {
            m_pendingObscure = false;
            m_pendingResize = false;
            if (debug_render())
                log("render thread - processing pending obscure");
            obscure();
        }
        if (m_pendingResize && !d->m_frameActive) {
            m_pendingResize = false;
            m_pendingUpdate = true;
            if (debug_render())
                log("render thread - processing pending resize");
            resize();
        }
        if (m_pendingUpdate && !d->m_frameActive) {
            m_pendingUpdate = false;
            if (d->beginFrame())
                d->renderFrame();
        }

        processEvents();
        QCoreApplication::processEvents();

        if (!m_pendingUpdate
                && !m_pendingDestroy
                && !(m_pendingObscure && !d->m_frameActive)
                && !(m_pendingResize && !d->m_frameActive)) {
            m_sleeping = true;
            processEventsAndWaitForMore();
            m_sleeping = false;
        }
    }

    if (debug_render())
        log("render thread - exit");
}

void QVulkanRenderLoopPrivate::init()
{
    if (m_inited)
        return;

    createDeviceAndSurface();
    recreateSwapChain();

    m_inited = true;
    if (debug_render())
        log("VK window renderer initialized");

    if (m_worker) {
        m_worker->init();
        m_worker->resize(m_windowSize.first, m_windowSize.second);
    }
}

void QVulkanRenderLoopPrivate::cleanup()
{
    if (!m_inited)
        return;

    if (m_worker)
        m_worker->cleanup();

    if (debug_render())
        log("Stopping VK window renderer");

    m_inited = false;

    releaseDeviceAndSurface();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackFunc(VkDebugReportFlagsEXT flags,
                                                        VkDebugReportObjectTypeEXT objectType,
                                                        uint64_t object,
                                                        size_t location,
                                                        int32_t messageCode,
                                                        const char *pLayerPrefix,
                                                        const char *pMessage,
                                                        void *pUserData)
{
    Q_UNUSED(flags);
    Q_UNUSED(objectType);
    Q_UNUSED(object);
    Q_UNUSED(location);
    Q_UNUSED(pUserData);
    log("DEBUG: %s: %d: %s", pLayerPrefix, messageCode, pMessage);
    return VK_FALSE;
}

void QVulkanRenderLoopPrivate::transitionImage(VkCommandBuffer cmdBuf,
                                               VkImage image,
                                               VkImageLayout oldLayout, VkImageLayout newLayout,
                                               VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                               bool ds)
{
    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = !ds ? VK_IMAGE_ASPECT_COLOR_BIT : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;

    f->vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            0,
                            0, nullptr,
                            0, nullptr,
                            1, &barrier);
}

void QVulkanRenderLoopPrivate::createDeviceAndSurface()
{
    VkApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "qqvk";
    appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    uint32_t layerCount = 0;
    f->vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (debug_render())
        log("%d instance layers", layerCount);
    std::vector<char *> enabledLayers;
    if (layerCount) {
        std::vector<VkLayerProperties> layerProps(layerCount);
        f->vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
        for (const VkLayerProperties &p : qAsConst(layerProps)) {
            if (m_flags.testFlag(QVulkanRenderLoop::EnableValidation) && !strcmp(p.layerName, "VK_LAYER_LUNARG_standard_validation"))
                enabledLayers.push_back(strdup(p.layerName));
        }
    }
    if (debug_render() && !enabledLayers.empty()) {
        log("enabling %d instance layers", enabledLayers.size());
        for (auto s : enabledLayers)
            log("  %s", s);
    }

    m_hasDebug = false;
    uint32_t extCount = 0;
    f->vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (debug_render())
        log("%d instance extensions", extCount);
    std::vector<char *> enabledExtensions;
    if (extCount) {
        std::vector<VkExtensionProperties> extProps(extCount);
        f->vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProps.data());
        for (const VkExtensionProperties &p : qAsConst(extProps)) {
            if (!strcmp(p.extensionName, "VK_EXT_debug_report")) {
                enabledExtensions.push_back(strdup(p.extensionName));
                m_hasDebug = true;
            } else if (!strcmp(p.extensionName, "VK_KHR_surface")
                       || !strcmp(p.extensionName, "VK_KHR_win32_surface"))
            {
                enabledExtensions.push_back(strdup(p.extensionName));
            }
        }
    }
    if (debug_render() && !enabledExtensions.empty()) {
        log("enabling %d instance extensions", enabledExtensions.size());
        for (auto s : enabledExtensions)
            log("  %s", s);
    }

    VkInstanceCreateInfo instInfo;
    memset(&instInfo, 0, sizeof(instInfo));
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    if (!enabledLayers.empty()) {
        instInfo.enabledLayerCount = uint32_t(enabledLayers.size());
        instInfo.ppEnabledLayerNames = enabledLayers.data();
    }
    if (!enabledExtensions.empty()) {
        instInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());
        instInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }

    VkResult err = f->vkCreateInstance(&instInfo, nullptr, &m_vkInst);
    if (err != VK_SUCCESS) {
        log("Failed to create Vulkan instance: %d", err);
        abort();
    }

    for (auto s : enabledLayers) free(s);
    for (auto s : enabledExtensions) free(s);

    if (m_hasDebug) {
        vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(f->vkGetInstanceProcAddr(m_vkInst, "vkCreateDebugReportCallbackEXT"));
        vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(f->vkGetInstanceProcAddr(m_vkInst, "vkDestroyDebugReportCallbackEXT"));
        vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(f->vkGetInstanceProcAddr(m_vkInst, "vkDebugReportMessageEXT"));

        VkDebugReportCallbackCreateInfoEXT dbgCallbackInfo;
        memset(&dbgCallbackInfo, 0, sizeof(dbgCallbackInfo));
        dbgCallbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        dbgCallbackInfo.flags =  VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        dbgCallbackInfo.pfnCallback = &debugCallbackFunc;
        err = vkCreateDebugReportCallbackEXT(m_vkInst, &dbgCallbackInfo, nullptr, &m_debugCallback);
        if (err != VK_SUCCESS) {
            log("Failed to create debug report callback: %d", err);
            m_hasDebug = false;
        }
    }

    createSurface();

    uint32_t devCount = 0;
    f->vkEnumeratePhysicalDevices(m_vkInst, &devCount, nullptr);
    if (debug_render())
        log("%d physical devices", devCount);
    if (!devCount) {
        log("No physical devices");
        abort();
    }
    // Just pick the first physical device for now.
    devCount = 1;
    err = f->vkEnumeratePhysicalDevices(m_vkInst, &devCount, &m_vkPhysDev);
    if (err != VK_SUCCESS) {
        log("Failed to enumerate physical devices: %d", err);
        abort();
    }

    f->vkGetPhysicalDeviceProperties(m_vkPhysDev, &m_physDevProps);
    if (debug_render())
        log("Device name: %s\nDriver version: %d.%d.%d", m_physDevProps.deviceName,
               VK_VERSION_MAJOR(m_physDevProps.driverVersion), VK_VERSION_MINOR(m_physDevProps.driverVersion),
               VK_VERSION_PATCH(m_physDevProps.driverVersion));

    layerCount = 0;
    f->vkEnumerateDeviceLayerProperties(m_vkPhysDev, &layerCount, nullptr);
    if (debug_render())
        log("%d device layers", layerCount);
    enabledLayers.clear();
    if (layerCount) {
        std::vector<VkLayerProperties> layerProps(layerCount);
        f->vkEnumerateDeviceLayerProperties(m_vkPhysDev, &layerCount, layerProps.data());
        for (const VkLayerProperties &p : qAsConst(layerProps)) {
            // If the validation layer is enabled for the instance, it has to
            // be enabled for the device too, otherwise be prepared for
            // mysterious errors...
            if (m_flags.testFlag(QVulkanRenderLoop::EnableValidation) && !strcmp(p.layerName, "VK_LAYER_LUNARG_standard_validation"))
                enabledLayers.push_back(strdup(p.layerName));
        }
    }
    if (debug_render() && !enabledLayers.empty()) {
        log("enabling %d device layers", enabledLayers.size());
        for (auto s : enabledLayers)
            log("  %s", s);
    }

    extCount = 0;
    f->vkEnumerateDeviceExtensionProperties(m_vkPhysDev, nullptr, &extCount, nullptr);
    if (debug_render())
        log("%d device extensions", extCount);
    enabledExtensions.clear();
    if (extCount) {
        std::vector<VkExtensionProperties> extProps(extCount);
        f->vkEnumerateDeviceExtensionProperties(m_vkPhysDev, nullptr, &extCount, extProps.data());
        for (const VkExtensionProperties &p : qAsConst(extProps)) {
            if (!strcmp(p.extensionName, "VK_KHR_swapchain")
                || !strcmp(p.extensionName, "VK_NV_glsl_shader"))
            {
                enabledExtensions.push_back(strdup(p.extensionName));
            }
        }
    }
    if (debug_render() && !enabledExtensions.empty()) {
        log("enabling %d device extensions", enabledExtensions.size());
        for (auto s : enabledExtensions)
            log("  %s", s);
    }

    uint32_t queueCount = 0;
    f->vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysDev, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueCount);
    f->vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysDev, &queueCount, queueFamilyProps.data());
    int gfxQueueFamilyIdx = -1;
    for (int i = 0; i < queueFamilyProps.size(); ++i) {
        if (debug_render())
            log("queue family %d: flags=0x%x count=%d", i, queueFamilyProps[i].queueFlags, queueFamilyProps[i].queueCount);
        bool ok = (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        ok |= physicalDeviceSupportsPresent(i);
        if (ok) {
            gfxQueueFamilyIdx = i;
            break;
        }
    }
    if (gfxQueueFamilyIdx == -1) {
        log("No presentable graphics queue family found");
        abort();
    }

    VkDeviceQueueCreateInfo queueInfo;
    memset(&queueInfo, 0, sizeof(queueInfo));
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = gfxQueueFamilyIdx;
    queueInfo.queueCount = 1;
    const float prio[] = { 0 };
    queueInfo.pQueuePriorities = prio;

    VkDeviceCreateInfo devInfo;
    memset(&devInfo, 0, sizeof(devInfo));
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    if (!enabledLayers.empty()) {
        devInfo.enabledLayerCount = uint32_t(enabledLayers.size());
        devInfo.ppEnabledLayerNames = enabledLayers.data();
    }
    if (!enabledExtensions.empty()) {
        devInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());
        devInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }

    err = f->vkCreateDevice(m_vkPhysDev, &devInfo, nullptr, &m_vkDev);
    if (err != VK_SUCCESS) {
        log("Failed to create device: %d", err);
        abort();
    }

    for (auto s : enabledLayers) free(s);
    for (auto s : enabledExtensions) free(s);

    f->vkGetDeviceQueue(m_vkDev, gfxQueueFamilyIdx, 0, &m_vkQueue);

    VkCommandPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = gfxQueueFamilyIdx;
    err = f->vkCreateCommandPool(m_vkDev, &poolInfo, nullptr, &m_vkCmdPool);
    if (err != VK_SUCCESS) {
        log("Failed to create command pool: %d", err);
        abort();
    }

    m_hostVisibleMemIndex = 0;
    bool hostVisibleMemIndexSet = false;
    f->vkGetPhysicalDeviceMemoryProperties(m_vkPhysDev, &m_vkPhysDevMemProps);
    for (uint32_t i = 0; i < m_vkPhysDevMemProps.memoryTypeCount; ++i) {
        const VkMemoryType *memType = m_vkPhysDevMemProps.memoryTypes;
        if (debug_render())
            log("memtype %d: flags=0x%x", i, memType[i].propertyFlags);
        // Find a host visible, host coherent memtype. If there is one that is
        // cached as well (in addition to being coherent), prefer that.
        if (memType[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            if (!hostVisibleMemIndexSet
                    || (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
                hostVisibleMemIndexSet = true;
                m_hostVisibleMemIndex = i;
            }
        }
    }
    if (debug_render())
        log("picked memtype %d for host visible memory", m_hostVisibleMemIndex);

    m_colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
}

void QVulkanRenderLoopPrivate::releaseDeviceAndSurface()
{
    releaseSurface();
    f->vkDestroyCommandPool(m_vkDev, m_vkCmdPool, nullptr);
    f->vkDestroyDevice(m_vkDev, nullptr);

    if (m_hasDebug)
        vkDestroyDebugReportCallbackEXT(m_vkInst, m_debugCallback, nullptr);

    f->vkDestroyInstance(m_vkInst, nullptr);
}

void QVulkanRenderLoopPrivate::createSurface()
{
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkDestroySurfaceKHR"));
    vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

#if defined(Q_OS_WIN)
    vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkCreateWin32SurfaceKHR"));
    vkGetPhysicalDeviceWin32PresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));

    VkWin32SurfaceCreateInfoKHR surfaceInfo;
    memset(&surfaceInfo, 0, sizeof(surfaceInfo));
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    surfaceInfo.hwnd = HWND(m_winId);
    VkResult err = vkCreateWin32SurfaceKHR(m_vkInst, &surfaceInfo, nullptr, &m_surface);
    if (err != VK_SUCCESS) {
        log("Failed to create Win32 surface: %d", err);
        abort();
    }
#endif

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_frameCmdBuf[i][0] = VK_NULL_HANDLE;
        m_frameCmdBuf[i][1] = VK_NULL_HANDLE;
        m_frameCmdBufRecording[i] = false;
        m_frameFence[i] = VK_NULL_HANDLE;
        m_acquireSem[i] = VK_NULL_HANDLE;
        m_renderSem[i] = VK_NULL_HANDLE;
        m_workerWaitSem[i] = VK_NULL_HANDLE;
        m_workerSignalSem[i] = VK_NULL_HANDLE;
    }
}

void QVulkanRenderLoopPrivate::releaseSurface()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        for (int j = 0; j < 2; ++j) {
            if (m_frameCmdBuf[i][j] != VK_NULL_HANDLE) {
                f->vkFreeCommandBuffers(m_vkDev, m_vkCmdPool, 1, &m_frameCmdBuf[i][j]);
                m_frameCmdBuf[i][j] = VK_NULL_HANDLE;
            }
        }
        if (m_frameFence[i] != VK_NULL_HANDLE) {
            f->vkDestroyFence(m_vkDev, m_frameFence[i], nullptr);
            m_frameFence[i] = VK_NULL_HANDLE;
        }
        if (m_acquireSem[i] != VK_NULL_HANDLE) {
            f->vkDestroySemaphore(m_vkDev, m_acquireSem[i], nullptr);
            m_acquireSem[i] = VK_NULL_HANDLE;
        }
        if (m_renderSem[i] != VK_NULL_HANDLE) {
            f->vkDestroySemaphore(m_vkDev, m_renderSem[i], nullptr);
            m_renderSem[i] = VK_NULL_HANDLE;
        }
        if (m_workerWaitSem[i] != VK_NULL_HANDLE) {
            f->vkDestroySemaphore(m_vkDev, m_workerWaitSem[i], nullptr);
            m_workerWaitSem[i] = VK_NULL_HANDLE;
        }
        if (m_workerSignalSem[i] != VK_NULL_HANDLE) {
            f->vkDestroySemaphore(m_vkDev, m_workerSignalSem[i], nullptr);
            m_workerSignalSem[i] = VK_NULL_HANDLE;
        }
    }

    if (m_swapChain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < m_swapChainBufferCount; ++i)
            f->vkDestroyImageView(m_vkDev, m_swapChainImageViews[i], nullptr);
        vkDestroySwapchainKHR(m_vkDev, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
        f->vkDestroyImageView(m_vkDev, m_dsView, nullptr);
        f->vkDestroyImage(m_vkDev, m_ds, nullptr);
        f->vkFreeMemory(m_vkDev, m_dsMem, nullptr);
        m_dsMem = VK_NULL_HANDLE;
    }

    vkDestroySurfaceKHR(m_vkInst, m_surface, nullptr);
}

bool QVulkanRenderLoopPrivate::physicalDeviceSupportsPresent(int queueFamilyIdx)
{
    bool ok = false;
#if defined(Q_OS_WIN)
    ok |= bool(vkGetPhysicalDeviceWin32PresentationSupportKHR(m_vkPhysDev, queueFamilyIdx));
#endif
    VkBool32 supported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_vkPhysDev, queueFamilyIdx, m_surface, &supported);
    ok |= bool(supported);
    return ok;
}

void QVulkanRenderLoopPrivate::recreateSwapChain()
{
    if (!m_windowSize.first || !m_windowSize.second)
        return;

    if (!vkCreateSwapchainKHR) {
        vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(f->vkGetDeviceProcAddr(m_vkDev, "vkCreateSwapchainKHR"));
        vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(f->vkGetDeviceProcAddr(m_vkDev, "vkDestroySwapchainKHR"));
        vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(f->vkGetDeviceProcAddr(m_vkDev, "vkGetSwapchainImagesKHR"));
        vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(f->vkGetDeviceProcAddr(m_vkDev, "vkAcquireNextImageKHR"));
        vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(f->vkGetDeviceProcAddr(m_vkDev, "vkQueuePresentKHR"));
    }

    VkColorSpaceKHR colorSpace = VkColorSpaceKHR(0);
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysDev, m_surface, &formatCount, nullptr);
    if (formatCount) {
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysDev, m_surface, &formatCount, formats.data());
        if (formats[0].format != VK_FORMAT_UNDEFINED) {
            m_colorFormat = formats[0].format;
            colorSpace = formats[0].colorSpace;
        }
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkPhysDev, m_surface, &surfaceCaps);
    uint32_t reqBufferCount = !m_flags.testFlag(QVulkanRenderLoop::TrippleBuffer) ? 2 : 3;
    if (surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);
    Q_ASSERT(surfaceCaps.minImageCount <= MAX_SWAPCHAIN_BUFFERS);
    reqBufferCount = qMin<uint32_t>(reqBufferCount, MAX_SWAPCHAIN_BUFFERS);

    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if (bufferSize.width == uint32_t(-1))
        bufferSize.width = m_windowSize.first;
    if (bufferSize.height == uint32_t(-1))
        bufferSize.height = m_windowSize.second;

    VkSurfaceTransformFlagBitsKHR preTransform = surfaceCaps.currentTransform;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    if (m_flags.testFlag(QVulkanRenderLoop::Unthrottled)) {
        uint32_t presModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysDev, m_surface, &presModeCount, nullptr);
        if (presModeCount > 0) {
            std::vector<VkPresentModeKHR> presModes(presModeCount);
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysDev, m_surface, &presModeCount, presModes.data()) == VK_SUCCESS) {
                if (std::find(presModes.begin(), presModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presModes.end())
                    presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                else if (std::find(presModes.begin(), presModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presModes.end())
                    presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    VkSwapchainKHR oldSwapChain = m_swapChain;
    VkSwapchainCreateInfoKHR swapChainInfo;
    memset(&swapChainInfo, 0, sizeof(swapChainInfo));
    swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainInfo.surface = m_surface;
    swapChainInfo.minImageCount = reqBufferCount;
    swapChainInfo.imageFormat = m_colorFormat;
    swapChainInfo.imageColorSpace = colorSpace;
    swapChainInfo.imageExtent = bufferSize;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainInfo.preTransform = preTransform;
    swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainInfo.presentMode = presentMode;
    swapChainInfo.clipped = true;
    swapChainInfo.oldSwapchain = oldSwapChain;

    if (debug_render())
        log("creating new swap chain of %d buffers, size %dx%d", reqBufferCount, bufferSize.width, bufferSize.height);

    VkResult err = vkCreateSwapchainKHR(m_vkDev, &swapChainInfo, nullptr, &m_swapChain);
    if (err != VK_SUCCESS) {
        log("Failed to create swap chain: %d", err);
        abort();
    }

    if (oldSwapChain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < m_swapChainBufferCount; ++i)
            f->vkDestroyImageView(m_vkDev, m_swapChainImageViews[i], nullptr);
        vkDestroySwapchainKHR(m_vkDev, oldSwapChain, nullptr);
    }

    m_swapChainBufferCount = 0;
    err = vkGetSwapchainImagesKHR(m_vkDev, m_swapChain, &m_swapChainBufferCount, nullptr);
    if (err != VK_SUCCESS || m_swapChainBufferCount < 2) {
        log("Failed to get swapchain images: %d (count=%d)", err, m_swapChainBufferCount);
        abort();
    }

    err = vkGetSwapchainImagesKHR(m_vkDev, m_swapChain, &m_swapChainBufferCount, m_swapChainImages);
    if (err != VK_SUCCESS) {
        log("Failed to get swapchain images: %d", err);
        abort();
    }

    if (debug_render())
        log("actual swap chain buffer count: %d", m_swapChainBufferCount);

    for (uint32_t i = 0; i < m_swapChainBufferCount; ++i) {
        VkImageViewCreateInfo imgViewInfo;
        memset(&imgViewInfo, 0, sizeof(imgViewInfo));
        imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgViewInfo.image = m_swapChainImages[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = m_colorFormat;
        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
        err = f->vkCreateImageView(m_vkDev, &imgViewInfo, nullptr, &m_swapChainImageViews[i]);
        if (err != VK_SUCCESS) {
            log("Failed to create swapchain image view %d: %d", i, err);
            abort();
        }
    }

    m_currentSwapChainBuffer = 0;
    m_currentFrame = 0;

    m_frameActive = false;
    m_frameCmdBufRecording[m_currentFrame] = false;
    ensureFrameCmdBuf(m_currentFrame, 0);

    for (uint32_t i = 0; i < m_swapChainBufferCount; ++i) {
        transitionImage(m_frameCmdBuf[m_currentFrame][0], m_swapChainImages[i],
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        0, 0);
    }

    for (int i = 0; i < m_framesInFlight; ++i) {
        m_frameFenceActive[i] = false;
        if (m_frameFence[i] == VK_NULL_HANDLE) {
            VkFenceCreateInfo fenceInfo = {
                VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                nullptr,
                0
            };
            err = f->vkCreateFence(m_vkDev, &fenceInfo, nullptr, &m_frameFence[i]);
            if (err != VK_SUCCESS) {
                log("Failed to create fence: %d", err);
                abort();
            }
        } else {
            f->vkResetFences(m_vkDev, 1, &m_frameFence[i]);
        }
        VkSemaphoreCreateInfo semInfo = {
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            nullptr,
            0
        };
        if (m_acquireSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_acquireSem[i]);
            if (err != VK_SUCCESS) {
                log("Failed to create acquire semaphore: %d", err);
                abort();
            }
        }
        if (m_renderSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_renderSem[i]);
            if (err != VK_SUCCESS) {
                log("Failed to create render semaphore: %d", err);
                abort();
            }
        }
        if (m_workerWaitSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_workerWaitSem[i]);
            if (err != VK_SUCCESS) {
                log("Failed to create worker wait semaphore: %d", err);
                abort();
            }
        }
        if (m_workerSignalSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_workerSignalSem[i]);
            if (err != VK_SUCCESS) {
                log("Failed to create worker signal semaphore: %d", err);
                abort();
            }
        }
    }

    if (m_dsMem != VK_NULL_HANDLE) {
        f->vkDestroyImageView(m_vkDev, m_dsView, nullptr);
        f->vkDestroyImage(m_vkDev, m_ds, nullptr);
        f->vkFreeMemory(m_vkDev, m_dsMem, nullptr);
    }

    VkImageCreateInfo imgInfo;
    memset(&imgInfo, 0, sizeof(imgInfo));
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
    imgInfo.extent.width = bufferSize.width;
    imgInfo.extent.height = bufferSize.height;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    err = f->vkCreateImage(m_vkDev, &imgInfo, nullptr, &m_ds);
    if (err != VK_SUCCESS) {
        log("Failed to create depth-stencil buffer: %d", err);
        abort();
    }

    VkMemoryRequirements dsMemReq;
    f->vkGetImageMemoryRequirements(m_vkDev, m_ds, &dsMemReq);
    unsigned long memTypeIndex = 0;
    if (dsMemReq.memoryTypeBits)
        _BitScanForward64(&memTypeIndex, dsMemReq.memoryTypeBits);

    VkMemoryAllocateInfo memInfo;
    memset(&memInfo, 0, sizeof(memInfo));
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = dsMemReq.size;
    memInfo.memoryTypeIndex = uint32_t(memTypeIndex);
    if (debug_render())
        log("allocating %lu bytes for depth-stencil", memInfo.allocationSize);

    err = f->vkAllocateMemory(m_vkDev, &memInfo, nullptr, &m_dsMem);
    if (err != VK_SUCCESS) {
        log("Failed to allocate depth-stencil memory: %d", err);
        abort();
    }

    err = f->vkBindImageMemory(m_vkDev, m_ds, m_dsMem, 0);
    if (err != VK_SUCCESS) {
        log("Failed to bind image memory for depth-stencil: %d", err);
        abort();
    }

    transitionImage(m_frameCmdBuf[m_currentFrame][0], m_ds,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, true);

    VkImageViewCreateInfo imgViewInfo;
    memset(&imgViewInfo, 0, sizeof(imgViewInfo));
    imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgViewInfo.image = m_ds;
    imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgViewInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
    imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
    err = f->vkCreateImageView(m_vkDev, &imgViewInfo, nullptr, &m_dsView);
    if (err != VK_SUCCESS) {
        log("Failed to create depth-stencil view: %d", err);
        abort();
    }
}

void QVulkanRenderLoopPrivate::ensureFrameCmdBuf(int frame, int subIndex)
{
    if (m_frameCmdBuf[frame][subIndex] != VK_NULL_HANDLE) {
        if (m_frameCmdBufRecording[frame])
            return;
        f->vkFreeCommandBuffers(m_vkDev, m_vkCmdPool, 1, &m_frameCmdBuf[frame][subIndex]);
    }

    VkCommandBufferAllocateInfo cmdBufInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_vkCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
    };
    VkResult err = f->vkAllocateCommandBuffers(m_vkDev, &cmdBufInfo, &m_frameCmdBuf[frame][subIndex]);
    if (err != VK_SUCCESS) {
        log("Failed to allocate frame command buffer: %d", err);
        abort();
    }

    VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
    err = f->vkBeginCommandBuffer(m_frameCmdBuf[frame][subIndex], &cmdBufBeginInfo);
    if (err != VK_SUCCESS) {
        log("Failed to begin frame command buffer: %d", err);
        abort();
    }

    m_frameCmdBufRecording[frame] = true;
}

bool QVulkanRenderLoopPrivate::beginFrame()
{
    Q_ASSERT(!m_frameActive);
    m_frameActive = true;

    if (m_frameFenceActive[m_currentFrame]) {
        if (debug_render())
            log("wait fence %p", m_frameFence[m_currentFrame]);
        f->vkWaitForFences(m_vkDev, 1, &m_frameFence[m_currentFrame], true, UINT64_MAX);
        f->vkResetFences(m_vkDev, 1, &m_frameFence[m_currentFrame]);
    }

    VkResult err = vkAcquireNextImageKHR(m_vkDev, m_swapChain, UINT64_MAX,
                                         m_acquireSem[m_currentFrame], VK_NULL_HANDLE,
                                         &m_currentSwapChainBuffer);
    if (err != VK_SUCCESS) {
        if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            log("out of date in acquire");
            f->vkDeviceWaitIdle(m_vkDev);
            recreateSwapChain();
            return false;
        } else if (err != VK_SUBOPTIMAL_KHR) {
            log("Failed to acquire next swapchain image: %d", err);
            return false;
        }
    }

    if (debug_render()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_beginFrameTimePoint);
        log("current swapchain buffer is %d, current frame is %d, elapsed since last %lld ms",
            m_currentSwapChainBuffer, m_currentFrame, elapsed);
        m_beginFrameTimePoint = std::chrono::steady_clock::now();
    }

    m_frameFenceActive[m_currentFrame] = true;
    ensureFrameCmdBuf(m_currentFrame, 0);

    transitionImage(m_frameCmdBuf[m_currentFrame][0], m_swapChainImages[m_currentSwapChainBuffer],
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    if (m_worker)
        submitFrameCmdBuf(m_acquireSem[m_currentFrame], m_workerWaitSem[m_currentFrame], 0, false);

    return true;
}

void QVulkanRenderLoopPrivate::submitFrameCmdBuf(VkSemaphore waitSem, VkSemaphore signalSem, int subIndex, bool fence)
{
    VkResult err = f->vkEndCommandBuffer(m_frameCmdBuf[m_currentFrame][subIndex]);
    if (err != VK_SUCCESS) {
        log("Failed to end frame command buffer: %d", err);
        abort();
    }

    m_frameCmdBufRecording[m_currentFrame] = false;

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_frameCmdBuf[m_currentFrame][subIndex];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSem;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSem;
    VkPipelineStageFlags psf = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    submitInfo.pWaitDstStageMask = &psf;
    err = f->vkQueueSubmit(m_vkQueue, 1, &submitInfo, fence ? m_frameFence[m_currentFrame] : VK_NULL_HANDLE);
    if (err != VK_SUCCESS) {
        log("Failed to submit to command queue: %d", err);
        return;
    }
}

void QVulkanRenderLoopPrivate::endFrame()
{
    Q_ASSERT(m_frameActive);
    m_frameActive = false;

    int subIndex = 0;
    if (m_worker) {
        subIndex = 1;
        ensureFrameCmdBuf(m_currentFrame, subIndex);
    }

    transitionImage(m_frameCmdBuf[m_currentFrame][subIndex], m_swapChainImages[m_currentSwapChainBuffer],
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0);

    submitFrameCmdBuf(m_worker ? m_workerSignalSem[m_currentFrame] : m_acquireSem[m_currentFrame], m_renderSem[m_currentFrame], subIndex, true);

    VkPresentInfoKHR presInfo;
    memset(&presInfo, 0, sizeof(presInfo));
    presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presInfo.swapchainCount = 1;
    presInfo.pSwapchains = &m_swapChain;
    presInfo.pImageIndices = &m_currentSwapChainBuffer;
    presInfo.waitSemaphoreCount = 1;
    presInfo.pWaitSemaphores = &m_renderSem[m_currentFrame];

    VkResult err = vkQueuePresentKHR(m_vkQueue, &presInfo);
    if (err != VK_SUCCESS) {
        if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            log("out of date in present");
            f->vkDeviceWaitIdle(m_vkDev);
            recreateSwapChain();
            return;
        } else if (err != VK_SUBOPTIMAL_KHR) {
            log("Failed to present: %d", err);
        }
    }

    m_currentFrame = (m_currentFrame + 1) % m_framesInFlight;

    if (m_flags.testFlag(QVulkanRenderLoop::UpdateContinuously))
        q->update();
}

void QVulkanRenderLoopPrivate::renderFrame()
{
    Q_ASSERT(m_frameActive);

    if (m_worker) {
        Q_ASSERT(!m_frameCmdBufRecording[m_currentFrame]);
        m_worker->queueFrame(m_currentFrame, m_vkQueue, m_workerWaitSem[m_currentFrame], m_workerSignalSem[m_currentFrame]);
        return;
    }

    // No worker set -> just clear to green.

    Q_ASSERT(m_frameCmdBufRecording[m_currentFrame]);

    VkCommandBuffer &cb = m_frameCmdBuf[m_currentFrame][0];
    VkImage &img = m_swapChainImages[m_currentSwapChainBuffer];

    VkClearColorValue clearColor = { 0.0f, 1.0f, 0.0f, 1.0f };
    VkImageSubresourceRange subResRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    f->vkCmdClearColorImage(cb, img, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subResRange);

    endFrame();
}

QT_END_NAMESPACE
