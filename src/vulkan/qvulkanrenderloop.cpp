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

#include "qvulkanrenderloop.h"
#include "qvulkanrenderloop_p.h"
#include <QVulkanFunctions>
#include <qalgorithms.h>
#include <QVector>
#include <QGuiApplication>
#include <QDebug>
#include <QElapsedTimer>

#ifdef Q_OS_LINUX
#include <qpa/qplatformnativeinterface.h>
#include <QtPlatformHeaders/qxcbwindowfunctions.h>
#endif

QT_BEGIN_NAMESPACE

static const uint32_t REQUESTED_SWAPCHAIN_BUFFERS = 2;


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


#define DECLARE_DEBUG_VAR(variable) \
    static bool debug_ ## variable() \
    { static bool value = qgetenv("QVULKAN_DEBUG").contains(QT_STRINGIFY(variable)); return value; }

DECLARE_DEBUG_VAR(render)

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
        qWarning("Cannot change flags after rendering has started");
        return;
    }
    d->m_flags = flags;
}

void QVulkanRenderLoop::setFramesInFlight(int frameCount)
{
    if (d->m_inited) {
        qWarning("Cannot change number of frames in flight after rendering has started");
        return;
    }
    if (frameCount < 1 || frameCount > QVulkanRenderLoopPrivate::MAX_FRAMES_IN_FLIGHT) {
        qWarning("Invalid frames-in-flight count");
        return;
    }
    d->m_framesInFlight = frameCount;
}

void QVulkanRenderLoop::setWorker(QVulkanFrameWorker *worker)
{
    if (d->m_inited) {
        qWarning("Cannot change worker after rendering has started");
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

    if (QThread::currentThread() == d->m_thread) {
        d->m_thread->setUpdatePending();
    } else {
        d->m_thread->mutex()->lock();
        d->m_thread->postEvent(new QVulkanRenderThreadUpdateEvent);
        d->m_thread->waitCondition()->wait(d->m_thread->mutex());
        d->m_thread->mutex()->unlock();
    }
}

void QVulkanRenderLoop::frameQueued()
{
    if (!d->m_inited)
        return;

    if (QThread::currentThread() == d->m_thread) {
        d->endFrame();
    } else {
        d->m_thread->mutex()->lock();
        d->m_thread->postEvent(new QVulkanRenderThreadFrameQueuedEvent);
        d->m_thread->waitCondition()->wait(d->m_thread->mutex());
        d->m_thread->mutex()->unlock();
    }
}

VkInstance QVulkanRenderLoop::instance() const
{
    return d->m_vkInst;
}

VkPhysicalDevice QVulkanRenderLoop::physicalDevice() const
{
    return d->m_vkPhysDev;
}

VkDevice QVulkanRenderLoop::device() const
{
    return d->m_vkDev;
}

VkCommandPool QVulkanRenderLoop::commandPool() const
{
    return d->m_vkCmdPool;
}

uint32_t QVulkanRenderLoop::hostVisibleMemoryIndex() const
{
    return d->m_hostVisibleMemIndex;
}

VkImage QVulkanRenderLoop::currentSwapChainImage() const
{
    return d->m_swapChainImages[d->m_currentSwapChainBuffer];
}

VkImageView QVulkanRenderLoop::currentSwapChainImageView() const
{
    return d->m_swapChainImageViews[d->m_currentSwapChainBuffer];
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
        m_thread->mutex()->lock();
        m_thread->postEvent(new QVulkanRenderThreadDestroyEvent);
        m_thread->waitCondition()->wait(m_thread->mutex());
        m_thread->mutex()->unlock();
        m_thread->wait();
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
#ifdef Q_OS_LINUX
            m_xcbConnection = static_cast<xcb_connection_t *>(qGuiApp->platformNativeInterface()->nativeResourceForIntegration(QByteArrayLiteral("connection")));
            m_xcbVisualId = QXcbWindowFunctions::visualId(window);
#endif
            m_windowSize = window->size();
            m_thread->postEvent(new QVulkanRenderThreadExposeEvent);
            m_thread->waitCondition()->wait(m_thread->mutex());
            m_thread->mutex()->unlock();
        } else if (m_inited) {
            m_thread->mutex()->lock();
            m_thread->postEvent(new QVulkanRenderThreadObscureEvent);
            m_thread->waitCondition()->wait(m_thread->mutex());
            m_thread->mutex()->unlock();
        }
    } else if (event->type() == QEvent::Resize) {
        if (m_inited && window->isExposed()) {
            m_thread->mutex()->lock();
            m_thread->postEvent(new QVulkanRenderThreadResizeEvent);
            m_thread->waitCondition()->wait(m_thread->mutex());
            m_thread->mutex()->unlock();
        }
    }

    return false;
}

void QVulkanRenderThreadEventQueue::addEvent(QEvent *e)
{
    m_mutex.lock();
    enqueue(e);
    if (m_waiting)
        m_condition.wakeOne();
    m_mutex.unlock();
}

QEvent *QVulkanRenderThreadEventQueue::takeEvent(bool wait)
{
    m_mutex.lock();
    if (isEmpty() && wait) {
        m_waiting = true;
        m_condition.wait(&m_mutex);
        m_waiting = false;
    }
    QEvent *e = dequeue();
    m_mutex.unlock();
    return e;
}

bool QVulkanRenderThreadEventQueue::hasMoreEvents()
{
    m_mutex.lock();
    bool has = !isEmpty();
    m_mutex.unlock();
    return has;
}

void QVulkanRenderThread::postEvent(QEvent *e)
{
    m_eventQueue.addEvent(e);
}

void QVulkanRenderThread::processEvents()
{
    while (m_eventQueue.hasMoreEvents()) {
        QEvent *e = m_eventQueue.takeEvent(false);
        processEvent(e);
        delete e;
    }
}

void QVulkanRenderThread::processEventsAndWaitForMore()
{
    m_stopEventProcessing = false;
    while (!m_stopEventProcessing) {
        QEvent *e = m_eventQueue.takeEvent(true);
        processEvent(e);
        delete e;
    }
}

void QVulkanRenderThread::processEvent(QEvent *e)
{
    switch (int(e->type())) {
    case QVulkanRenderThreadExposeEvent::Type:
        m_mutex.lock();
        if (Q_UNLIKELY(debug_render()))
            qDebug("render thread - expose");
        if (!d->m_inited)
            d->init();
        m_pendingUpdate = true;
        if (m_sleeping)
            m_stopEventProcessing = true;
        m_condition.wakeOne();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadObscureEvent::Type:
        m_mutex.lock();
        if (!d->m_frameActive) {
            if (Q_UNLIKELY(debug_render()))
                qDebug("render thread - obscure");
            obscure();
        } else {
            m_pendingObscure = true;
        }
        m_condition.wakeOne();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadResizeEvent::Type:
        m_mutex.lock();
        if (!d->m_frameActive) {
            if (Q_UNLIKELY(debug_render()))
                qDebug("render thread - resize");
            resize();
        } else {
            m_pendingResize = true;
        }
        m_condition.wakeOne();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadUpdateEvent::Type:
        m_mutex.lock();
        setUpdatePending();
        m_condition.wakeOne();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadFrameQueuedEvent::Type:
        m_mutex.lock();
        if (Q_UNLIKELY(debug_render()))
            qDebug("render thread - worker ready");
        d->endFrame();
        if (m_sleeping)
            m_stopEventProcessing = true;
        m_condition.wakeOne();
        m_mutex.unlock();
        break;
    case QVulkanRenderThreadDestroyEvent::Type:
        m_mutex.lock();
        if (!d->m_frameActive) {
            if (Q_UNLIKELY(debug_render()))
                qDebug("render thread - destroy");
            m_active = false;
            if (m_sleeping)
                m_stopEventProcessing = true;
        } else {
            m_pendingDestroy = true;
        }
        m_condition.wakeOne();
        m_mutex.unlock();
        break;
    default:
        qWarning("Unknown render thread event %d", e->type());
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
}

void QVulkanRenderThread::run()
{
    if (Q_UNLIKELY(debug_render()))
        qDebug("render thread - start");

    m_sleeping = false;
    m_stopEventProcessing = false;
    m_pendingUpdate = false;
    m_pendingObscure = false;
    m_pendingResize = false;
    m_pendingDestroy = false;

    while (m_active) {
        if (m_pendingDestroy) {
            if (Q_UNLIKELY(debug_render()))
                qDebug("render thread - processing pending destroy");
            m_active = false;
            continue;
        }
        if (m_pendingObscure && !d->m_frameActive) {
            m_pendingObscure = false;
            m_pendingResize = false;
            if (Q_UNLIKELY(debug_render()))
                qDebug("render thread - processing pending obscure");
            obscure();
        }
        if (m_pendingResize && !d->m_frameActive) {
            m_pendingResize = false;
            m_pendingUpdate = true;
            if (Q_UNLIKELY(debug_render()))
                qDebug("render thread - processing pending resize");
            resize();
        }
        if (m_pendingUpdate && !d->m_frameActive) {
            m_pendingUpdate = false;
            if (d->beginFrame())
                d->renderFrame();
        }

        processEvents();
        QCoreApplication::processEvents();

        if (!m_pendingUpdate) {
            m_sleeping = true;
            processEventsAndWaitForMore();
            m_sleeping = false;
        }
    }

    if (Q_UNLIKELY(debug_render()))
        qDebug("render thread - exit");
}

void QVulkanRenderLoopPrivate::init()
{
    if (m_inited)
        return;

    createDeviceAndSurface();
    recreateSwapChain();

    m_inited = true;
    if (Q_UNLIKELY(debug_render()))
        qDebug("VK window renderer initialized");

    if (m_worker)
        m_worker->init();
}

void QVulkanRenderLoopPrivate::cleanup()
{
    if (!m_inited)
        return;

    if (m_worker)
        m_worker->cleanup();

    if (Q_UNLIKELY(debug_render()))
        qDebug("Stopping VK window renderer");

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
    qDebug("DEBUG: %s: %d: %s", pLayerPrefix, messageCode, pMessage);
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
    if (Q_UNLIKELY(debug_render()))
        qDebug("%d instance layers", layerCount);
    QVector<char *> enabledLayers;
    if (layerCount) {
        QVector<VkLayerProperties> layerProps(layerCount);
        f->vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
        for (const VkLayerProperties &p : qAsConst(layerProps)) {
            if (m_flags.testFlag(QVulkanRenderLoop::EnableValidation) && !strcmp(p.layerName, "VK_LAYER_LUNARG_standard_validation"))
                enabledLayers.append(strdup(p.layerName));
        }
    }
    if (!enabledLayers.isEmpty())
        if (Q_UNLIKELY(debug_render()))
            qDebug() << "enabling instance layers" << enabledLayers;

    m_hasDebug = false;
    uint32_t extCount = 0;
    f->vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (Q_UNLIKELY(debug_render()))
        qDebug("%d instance extensions", extCount);
    QVector<char *> enabledExtensions;
    if (extCount) {
        QVector<VkExtensionProperties> extProps(extCount);
        f->vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProps.data());
        for (const VkExtensionProperties &p : qAsConst(extProps)) {
            if (!strcmp(p.extensionName, "VK_EXT_debug_report")) {
                enabledExtensions.append(strdup(p.extensionName));
                m_hasDebug = true;
            } else if (!strcmp(p.extensionName, "VK_KHR_surface")
                       || !strcmp(p.extensionName, "VK_KHR_win32_surface"))
            {
                enabledExtensions.append(strdup(p.extensionName));
            }
        }
    }
    if (!enabledExtensions.isEmpty())
        if (Q_UNLIKELY(debug_render()))
            qDebug() << "enabling instance extensions" << enabledExtensions;

    VkInstanceCreateInfo instInfo;
    memset(&instInfo, 0, sizeof(instInfo));
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    if (!enabledLayers.isEmpty()) {
        instInfo.enabledLayerCount = enabledLayers.count();
        instInfo.ppEnabledLayerNames = enabledLayers.constData();
    }
    if (!enabledExtensions.isEmpty()) {
        instInfo.enabledExtensionCount = enabledExtensions.count();
        instInfo.ppEnabledExtensionNames = enabledExtensions.constData();
    }

    VkResult err = f->vkCreateInstance(&instInfo, nullptr, &m_vkInst);
    if (err != VK_SUCCESS)
        qFatal("Failed to create Vulkan instance: %d", err);

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
            qWarning("Failed to create debug report callback: %d", err);
            m_hasDebug = false;
        }
    }

    createSurface();

    uint32_t devCount = 0;
    f->vkEnumeratePhysicalDevices(m_vkInst, &devCount, nullptr);
    if (Q_UNLIKELY(debug_render()))
        qDebug("%d physical devices", devCount);
    if (!devCount)
        qFatal("No physical devices");
    // Just pick the first physical device for now.
    devCount = 1;
    err = f->vkEnumeratePhysicalDevices(m_vkInst, &devCount, &m_vkPhysDev);
    if (err != VK_SUCCESS)
        qFatal("Failed to enumerate physical devices: %d", err);

    VkPhysicalDeviceProperties physDevProps;
    f->vkGetPhysicalDeviceProperties(m_vkPhysDev, &physDevProps);
    if (Q_UNLIKELY(debug_render()))
        qDebug("Device name: %s\nDriver version: %d.%d.%d", physDevProps.deviceName,
               VK_VERSION_MAJOR(physDevProps.driverVersion), VK_VERSION_MINOR(physDevProps.driverVersion),
               VK_VERSION_PATCH(physDevProps.driverVersion));

    layerCount = 0;
    f->vkEnumerateDeviceLayerProperties(m_vkPhysDev, &layerCount, nullptr);
    if (Q_UNLIKELY(debug_render()))
        qDebug("%d device layers", layerCount);
    enabledLayers.clear();
    if (layerCount) {
        QVector<VkLayerProperties> layerProps(layerCount);
        f->vkEnumerateDeviceLayerProperties(m_vkPhysDev, &layerCount, layerProps.data());
        for (const VkLayerProperties &p : qAsConst(layerProps)) {
            // If the validation layer is enabled for the instance, it has to
            // be enabled for the device too, otherwise be prepared for
            // mysterious errors...
            if (m_flags.testFlag(QVulkanRenderLoop::EnableValidation) && !strcmp(p.layerName, "VK_LAYER_LUNARG_standard_validation"))
                enabledLayers.append(strdup(p.layerName));
        }
    }
    if (!enabledLayers.isEmpty())
        if (Q_UNLIKELY(debug_render()))
            qDebug() << "enabling device layers" << enabledLayers;

    extCount = 0;
    f->vkEnumerateDeviceExtensionProperties(m_vkPhysDev, nullptr, &extCount, nullptr);
    if (Q_UNLIKELY(debug_render()))
        qDebug("%d device extensions", extCount);
    enabledExtensions.clear();
    if (extCount) {
        QVector<VkExtensionProperties> extProps(extCount);
        f->vkEnumerateDeviceExtensionProperties(m_vkPhysDev, nullptr, &extCount, extProps.data());
        for (const VkExtensionProperties &p : qAsConst(extProps)) {
            if (!strcmp(p.extensionName, "VK_KHR_swapchain")
                || !strcmp(p.extensionName, "VK_NV_glsl_shader"))
            {
                enabledExtensions.append(strdup(p.extensionName));
            }
        }
    }
    if (!enabledExtensions.isEmpty())
        if (Q_UNLIKELY(debug_render()))
            qDebug() << "enabling device extensions" << enabledExtensions;

    uint32_t queueCount = 0;
    f->vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysDev, &queueCount, nullptr);
    QVector<VkQueueFamilyProperties> queueFamilyProps(queueCount);
    f->vkGetPhysicalDeviceQueueFamilyProperties(m_vkPhysDev, &queueCount, queueFamilyProps.data());
    int gfxQueueFamilyIdx = -1;
    for (int i = 0; i < queueFamilyProps.count(); ++i) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("queue family %d: flags=0x%x count=%d", i, queueFamilyProps[i].queueFlags, queueFamilyProps[i].queueCount);
        bool ok = (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        ok |= physicalDeviceSupportsPresent(i);
        if (ok) {
            gfxQueueFamilyIdx = i;
            break;
        }
    }
    if (gfxQueueFamilyIdx == -1)
        qFatal("No presentable graphics queue family found");

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
    if (!enabledLayers.isEmpty()) {
        devInfo.enabledLayerCount = enabledLayers.count();
        devInfo.ppEnabledLayerNames = enabledLayers.constData();
    }
    if (!enabledExtensions.isEmpty()) {
        devInfo.enabledExtensionCount = enabledExtensions.count();
        devInfo.ppEnabledExtensionNames = enabledExtensions.constData();
    }

    err = f->vkCreateDevice(m_vkPhysDev, &devInfo, nullptr, &m_vkDev);
    if (err != VK_SUCCESS)
        qFatal("Failed to create device: %d", err);

    for (auto s : enabledLayers) free(s);
    for (auto s : enabledExtensions) free(s);

    f->vkGetDeviceQueue(m_vkDev, gfxQueueFamilyIdx, 0, &m_vkQueue);

    VkCommandPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = gfxQueueFamilyIdx;
    err = f->vkCreateCommandPool(m_vkDev, &poolInfo, nullptr, &m_vkCmdPool);
    if (err != VK_SUCCESS)
        qFatal("Failed to create command pool: %d", err);

    m_hostVisibleMemIndex = 0;
    f->vkGetPhysicalDeviceMemoryProperties(m_vkPhysDev, &m_vkPhysDevMemProps);
    for (uint32_t i = 0; i < m_vkPhysDevMemProps.memoryTypeCount; ++i) {
        const VkMemoryType *memType = m_vkPhysDevMemProps.memoryTypes;
        if (Q_UNLIKELY(debug_render()))
            qDebug("phys dev mem prop %d: flags=0x%x", i, memType[i].propertyFlags);
        if (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            m_hostVisibleMemIndex = i;
    }

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
    if (err != VK_SUCCESS)
        qFatal("Failed to create Win32 surface: %d", err);
#elif defined(Q_OS_LINUX)
    vkCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkCreateXcbSurfaceKHR"));
    vkGetPhysicalDeviceXcbPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>(f->vkGetInstanceProcAddr(m_vkInst, "vkGetPhysicalDeviceXcbPresentationSupportKHR"));

    VkXcbSurfaceCreateInfoKHR surfaceInfo;
    memset(&surfaceInfo, 0, sizeof(surfaceInfo));
    surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.connection = m_xcbConnection;
    surfaceInfo.window = m_winId;
    VkResult err = vkCreateXcbSurfaceKHR(m_vkInst, &surfaceInfo, nullptr, &m_surface);
    if (err != VK_SUCCESS)
        qFatal("Failed to create xcb surface: %d", err);
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
#elif defined(Q_OS_LINUX)
    ok |= bool(vkGetPhysicalDeviceXcbPresentationSupportKHR(m_vkPhysDev, queueFamilyIdx, m_xcbConnection, m_xcbVisualId));
#endif
    VkBool32 supported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_vkPhysDev, queueFamilyIdx, m_surface, &supported);
    ok |= bool(supported);
    return ok;
}

void QVulkanRenderLoopPrivate::recreateSwapChain()
{
    if (m_windowSize.isEmpty())
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
        QVector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysDev, m_surface, &formatCount, formats.data());
        if (formats[0].format != VK_FORMAT_UNDEFINED) {
            m_colorFormat = formats[0].format;
            colorSpace = formats[0].colorSpace;
        }
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkPhysDev, m_surface, &surfaceCaps);
    uint32_t reqBufferCount = REQUESTED_SWAPCHAIN_BUFFERS;
    if (surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);
    Q_ASSERT(surfaceCaps.minImageCount <= MAX_SWAPCHAIN_BUFFERS);
    reqBufferCount = qMin<uint32_t>(reqBufferCount, MAX_SWAPCHAIN_BUFFERS);

    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if (bufferSize.width == uint32_t(-1))
        bufferSize.width = m_windowSize.width();
    if (bufferSize.height == uint32_t(-1))
        bufferSize.height = m_windowSize.height();

    VkSurfaceTransformFlagBitsKHR preTransform = surfaceCaps.currentTransform;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    if (m_flags.testFlag(QVulkanRenderLoop::Unthrottled)) {
        uint32_t presModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysDev, m_surface, &presModeCount, nullptr);
        if (presModeCount > 0) {
            QVector<VkPresentModeKHR> presModes(presModeCount);
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkPhysDev, m_surface, &presModeCount, presModes.data()) == VK_SUCCESS) {
                if (presModes.contains(VK_PRESENT_MODE_MAILBOX_KHR))
                    presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                else if (presModes.contains(VK_PRESENT_MODE_IMMEDIATE_KHR))
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

    if (Q_UNLIKELY(debug_render()))
        qDebug("creating new swap chain of %d buffers, size %dx%d", reqBufferCount, bufferSize.width, bufferSize.height);

    VkResult err = vkCreateSwapchainKHR(m_vkDev, &swapChainInfo, nullptr, &m_swapChain);
    if (err != VK_SUCCESS)
        qFatal("Failed to create swap chain: %d", err);

    if (oldSwapChain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < m_swapChainBufferCount; ++i)
            f->vkDestroyImageView(m_vkDev, m_swapChainImageViews[i], nullptr);
        vkDestroySwapchainKHR(m_vkDev, oldSwapChain, nullptr);
    }

    m_swapChainBufferCount = 0;
    err = vkGetSwapchainImagesKHR(m_vkDev, m_swapChain, &m_swapChainBufferCount, nullptr);
    if (err != VK_SUCCESS || m_swapChainBufferCount < 2)
        qFatal("Failed to get swapchain images: %d (count=%d)", err, m_swapChainBufferCount);

    err = vkGetSwapchainImagesKHR(m_vkDev, m_swapChain, &m_swapChainBufferCount, m_swapChainImages);
    if (err != VK_SUCCESS)
        qFatal("Failed to get swapchain images: %d", err);

    if (Q_UNLIKELY(debug_render()))
        qDebug("swap chain buffer count: %d", m_swapChainBufferCount);

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
        if (err != VK_SUCCESS)
            qFatal("Failed to create swapchain image view %d: %d", i, err);
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
            if (err != VK_SUCCESS)
                qFatal("Failed to create fence: %d", err);
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
            if (err != VK_SUCCESS)
                qFatal("Failed to create acquire semaphore: %d", err);
        }
        if (m_renderSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_renderSem[i]);
            if (err != VK_SUCCESS)
                qFatal("Failed to create render semaphore: %d", err);
        }
        if (m_workerWaitSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_workerWaitSem[i]);
            if (err != VK_SUCCESS)
                qFatal("Failed to create worker wait semaphore: %d", err);
        }
        if (m_workerSignalSem[i] == VK_NULL_HANDLE) {
            err = f->vkCreateSemaphore(m_vkDev, &semInfo, nullptr, &m_workerSignalSem[i]);
            if (err != VK_SUCCESS)
                qFatal("Failed to create worker signal semaphore: %d", err);
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
    if (err != VK_SUCCESS)
        qFatal("Failed to create depth-stencil buffer: %d", err);

    VkMemoryRequirements dsMemReq;
    f->vkGetImageMemoryRequirements(m_vkDev, m_ds, &dsMemReq);
    uint memTypeIndex = 0;
    if (dsMemReq.memoryTypeBits)
        memTypeIndex = qCountTrailingZeroBits(dsMemReq.memoryTypeBits);

    VkMemoryAllocateInfo memInfo;
    memset(&memInfo, 0, sizeof(memInfo));
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = dsMemReq.size;
    memInfo.memoryTypeIndex = memTypeIndex;
    if (Q_UNLIKELY(debug_render()))
        qDebug("allocating %lu bytes for depth-stencil", memInfo.allocationSize);

    err = f->vkAllocateMemory(m_vkDev, &memInfo, nullptr, &m_dsMem);
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate depth-stencil memory: %d", err);

    err = f->vkBindImageMemory(m_vkDev, m_ds, m_dsMem, 0);
    if (err != VK_SUCCESS)
        qFatal("Failed to bind image memory for depth-stencil: %d", err);

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
    if (err != VK_SUCCESS)
        qFatal("Failed to create depth-stencil view: %d", err);
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
    if (err != VK_SUCCESS)
        qFatal("Failed to allocate frame command buffer: %d", err);

    VkCommandBufferBeginInfo cmdBufBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
    err = f->vkBeginCommandBuffer(m_frameCmdBuf[frame][subIndex], &cmdBufBeginInfo);
    if (err != VK_SUCCESS)
        qFatal("Failed to begin frame command buffer: %d", err);

    m_frameCmdBufRecording[frame] = true;
}

QElapsedTimer t;

bool QVulkanRenderLoopPrivate::beginFrame()
{
    Q_ASSERT(!m_frameActive);
    m_frameActive = true;

    if (m_frameFenceActive[m_currentFrame]) {
        if (Q_UNLIKELY(debug_render()))
            qDebug("wait fence %p", m_frameFence[m_currentFrame]);
        f->vkWaitForFences(m_vkDev, 1, &m_frameFence[m_currentFrame], true, UINT64_MAX);
        f->vkResetFences(m_vkDev, 1, &m_frameFence[m_currentFrame]);
    }

    VkResult err = vkAcquireNextImageKHR(m_vkDev, m_swapChain, UINT64_MAX,
                                         m_acquireSem[m_currentFrame], VK_NULL_HANDLE,
                                         &m_currentSwapChainBuffer);
    if (err != VK_SUCCESS) {
        if (err == VK_ERROR_OUT_OF_DATE_KHR) {
            qWarning("out of date in acquire");
            f->vkDeviceWaitIdle(m_vkDev);
            recreateSwapChain();
            return false;
        } else if (err != VK_SUBOPTIMAL_KHR) {
            qWarning("Failed to acquire next swapchain image: %d", err);
            return false;
        }
    }

    if (Q_UNLIKELY(debug_render()))
        qDebug("current swapchain buffer is %d, current frame is %d, elapsed since last %lld ms",
               m_currentSwapChainBuffer, m_currentFrame, t.restart());

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
    if (err != VK_SUCCESS)
        qFatal("Failed to end frame command buffer: %d", err);

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
        qWarning("Failed to submit to command queue: %d", err);
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
            qWarning("out of date in present");
            f->vkDeviceWaitIdle(m_vkDev);
            recreateSwapChain();
            return;
        } else if (err != VK_SUBOPTIMAL_KHR) {
            qWarning("Failed to present: %d", err);
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
