This is the Qt Vulkan test bed.

The goal is to experiment with basic - but efficient - Vulkan enablers,
focusing mainly on getting Vulkan-rendered content into a QWindow, in order to
have a more clear view on what kind of (minimal) enablers could be added in Qt
5.9 and beyond.

==================================

This is a real Qt module. Build with qmake -r and make install. Applications
can then simply do QT += vulkan in their .pro files.

When building against a Vulkan SDK where the headers are not available in the
default search paths, do something like 'qmake -r VULKAN_INCLUDE_PATH=c:/VulkanSDK/1.0.17.0/include'
to specify the location of the Vulkan headers.

==================================

Instead of the traditional QGLWidget, QOpenGLWindow, etc. model (i.e. paintGL)
we use a QVulkanRenderLoop that gets attached to a QWindow, and gets in turn a
QVulkanFrameWorker attached to it. The render loop runs its own dedicated thread
and this is the thread the worker's functions are invoked on. This is similar
the Qt Quick scenegraph's threaded render loop - except that the interface here
is asynchronous and the worker can also submit one or more command buffers from
additional worker threads if it wants to.

To be as portable as possible, all Vulkan functions are resolved dynamically,
either via QLibrary or the device/instance-level getProcAddr, so no libs are
needed at link time.

The number of frames prepared without blocking (i.e. without waiting for the
previous submission to finish executing) can be changed from the default 1 to 2
or 3 via setFramesInFlight(). By default the FIFO present mode is used, meaning
the renderloop's thread will get throttled based on the vsync. Pass Unthrottled
to switch to mailbox mode instead. The swapchain uses 2 buffers by default,
pass TrippleBuffer to request 3 instead. The standard validation layer can be
requested by setting EnableValidation. Without further ado, here's the API, it
should be self-explanatory:

```
class Q_VULKAN_EXPORT QVulkanFrameWorker
{
public:
    virtual ~QVulkanFrameWorker() { }
    virtual void init() = 0;
    virtual void resize(const QSize &size) = 0;
    virtual void cleanup() = 0;
    virtual void queueFrame(int frame, VkQueue queue, VkSemaphore waitSem, VkSemaphore signalSem) = 0;
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
```

================================

Currently only Windows and Linux (X11) are supported, and only
Windows has been tested in practice. Other WSIs will get added later on.

Note: NVIDIA drivers older than 368 are not recommended. For example, 365 was
unable to provide vsync and was always running unthrottled.

TODO:
  1. other WSIs
  2. issue some real draw calls in the example
  3. play a bit with shaders (SPIR-V, VK_NV_glsl_shader, etc.)
  4. more examples, more threads, more everything
