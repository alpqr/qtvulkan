This is the Qt Vulkan test bed.

The goal is to experiment with basic Vulkan enablers, focusing mainly on
getting Vulkan-rendered content into a QWindow, in order to have a more clear
view on what kind of (minimal) enablers could be added in Qt 5.9 and beyond.

Do not expect to see fancy demos here - there is no actual geometry rendering
in the examples yet, apart from clearing. However, QVulkanRenderLoop itself is
pretty complete and stable already.

==================================

This is a real Qt module. Build with qmake -r and make install. Applications
can then simply do QT += vulkan in their .pro files.

When building against a Vulkan SDK where the headers are not available in the
default search paths, do something like 'qmake -r VULKAN_INCLUDE_PATH=c:/VulkanSDK/1.0.17.0/include'
to specify the location of the Vulkan headers.

==================================

The main goal here is to demonstrate the model where the submission/present
thread gets throttled based on the vsync, while allowing multiple frames in
flight without pipeline stalls. (i.e. something that is missing from the
majority of "tutorials" out there)

To be as portable as possible, all Vulkan functions are resolved dynamically,
either via QLibrary or the device/instance-level getProcAddr, so no libs are
needed at link time.

Instead of the traditional QGLWidget, QOpenGLWindow, etc. model (i.e. paintGL)
we use a QVulkanRenderLoop that gets attached to a QWindow, and gets in turn a
QVulkanFrameWorker attached to it. This interface is asynchronous and the worker
can submit one or more command buffers potentially from separate worker threads
if it wants to.

The number of frames prepared without blocking (1, 2, or 3) can be decided by
the application. The standard validation layer can be requested by setting the
appropriate flag. Without further ado, here's the API, it should be
self-explanatory:

```
class Q_VULKAN_EXPORT QVulkanFrameWorker : public QObject
{
    Q_OBJECT

public:
    virtual void init() = 0;
    virtual void cleanup() = 0;
    virtual void queueFrame(int frame, VkQueue queue, VkSemaphore waitSem, VkSemaphore signalSem) = 0;

Q_SIGNALS:
    void queued();
};

class Q_VULKAN_EXPORT QVulkanRenderLoop
{
public:
    enum Flag {
        EnableValidation = 0x01,
        Unthrottled = 0x02,
        UpdateContinuously = 0x04
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QVulkanRenderLoop(QWindow *window);
    ~QVulkanRenderLoop();

    QVulkanFunctions *functions();

    void setFlags(Flags flags);
    void setFramesInFlight(int frameCount);

    void setWorker(QVulkanFrameWorker *worker);

    void update();

    // for QVulkanFrameWorker
    VkInstance instance() const;
    VkPhysicalDevice physicalDevice() const;
    VkDevice device() const;
    VkCommandPool commandPool() const;
    uint32_t hostVisibleMemoryIndex() const;
    VkImage currentSwapChainImage() const;
    VkImageView currentSwapChainImageView() const;
    VkFormat swapChainFormat() const;
    VkImage depthStencilImage() const;
    VkImageView depthStencilImageView() const;
    VkFormat depthStencilFormat() const;
```

================================

Currently only Windows and Linux (X11) are supported, and only
Windows has been tested in practice. Other WSIs will get added later on.

Note: NVIDIA drivers older than 368 are not recommended. For example, 365 was
unable to provide vsync and was always running unthrottled.

Initialization and teardown is currently tied to expose/obscure events. When
these happen is windowing system dependent (f.ex. Windows obscures on minimize
only). Releasing all resources on obscure is not necessarily ideal for real
apps but will do for demo purposes.

TODO:
  1. other WSIs
  2. issue some real draw calls in the example
  3. test dedicated worker threads in the example
  4. explore threading further: what if we want the QVulkanRenderLoop to live on a dedicate thread which it can throttle without affecting the main/gui thread?
  5. play a bit with shaders (SPIR-V, VK_NV_glsl_shader, etc.)
