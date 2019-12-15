// This is a one-stop shop Vulkan implementation. All Vulkan code should reside in the base/backend folder.
// All information on the API that was used to produce this implementation has been garnered from the following 
// primary resources:
// 
// - http://vulkan-tutorial.com
// - https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html

#pragma once

#if defined(BASE_ENABLE_VULKAN)

#include "common.hpp"
#include <optional>
#include <set>

#define VK_FN(expr) vk_call((expr), #expr, __LINE__, __FILE__)
#define RVK_FN(expr) m_vk_result = VK_FN(expr)

namespace vulkan {
  VkResult vk_call(VkResult call, const char* expr, int line, const char* file) {
    if (call != VK_SUCCESS) {
      write_logf("VULKAN ERROR: %s@%s:%i -> 0x%x\n", 
                 expr, 
                 file, 
                 line, 
                 call);
    }
    return call;
  }

  struct queue_family_indices {
    std::optional<uint32_t> graphics_family{};
    std::optional<uint32_t> present_family{};

    bool ok() const {
      return graphics_family.has_value() && present_family.has_value();
    }
  };

  struct swapchain_support_details {
    VkSurfaceCapabilitiesKHR capabilities;
    darray<VkSurfaceFormatKHR> formats;
    darray<VkPresentModeKHR> present_modes;
  };

  class renderer {
    darray<VkPhysicalDevice> m_vk_physical_devs;

    darray<VkImage> m_vk_swapchain_images;

    VkInstance m_vk_instance {VK_NULL_HANDLE};

    VkPhysicalDevice m_vk_curr_pdevice{VK_NULL_HANDLE};

    VkDevice m_vk_curr_ldevice{VK_NULL_HANDLE};

    VkQueue m_vk_graphics_queue{VK_NULL_HANDLE};
    VkQueue m_vk_present_queue{VK_NULL_HANDLE};

    VkSurfaceKHR m_vk_khr_surface{VK_NULL_HANDLE};

    VkSwapchainKHR m_vk_khr_swapchain{VK_NULL_HANDLE};

    VkResult m_vk_result{VK_SUCCESS};

    struct vk_layer_info {
      const char* name{nullptr};
      bool enable{false};
    };

    static inline darray<vk_layer_info> s_layers = {
      { "VK_LAYER_LUNARG_standard_validation", true },
      { "VK_LAYER_LUNARG_core_validation", true },
      { "VK_LAYER_LUNARG_parameter_validation", false }
    };

    static inline darray<const char*> s_device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    queue_family_indices query_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) {
      queue_family_indices indices {};

      if (ok()) {
        uint32_t queue_fam_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                        &queue_fam_count, 
                                                        nullptr);
        ASSERT(queue_fam_count > 0);
        darray<VkQueueFamilyProperties> queue_props(queue_fam_count);

        vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                  &queue_fam_count,
                                                  queue_props.data());

        uint32_t i = 0;
        while (i < queue_fam_count) {
          if ((queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphics_family = i;
          }

          if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            if (present_support) {
              indices.present_family = i;
            }
          }
          i++;
        }
      }
      
      return indices;
    }

    swapchain_support_details query_swapchain_support(VkPhysicalDevice device) {
      swapchain_support_details details;
      ASSERT(m_vk_khr_surface != VK_NULL_HANDLE);
      if (m_vk_khr_surface != VK_NULL_HANDLE) {
        if (ok()) {
          RVK_FN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_vk_khr_surface, &details.capabilities));
        }
        if (ok()) {
          uint32_t format_count = 0;
          RVK_FN(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vk_khr_surface, &format_count, nullptr));
          if (ok()) {
            ASSERT(format_count != 0);
            details.formats.resize(format_count);
            RVK_FN(vkGetPhysicalDeviceSurfaceFormatsKHR(device, 
                                                        m_vk_khr_surface, 
                                                        &format_count, 
                                                        details.formats.data()));
          }
        }
        if (ok()) {
          uint32_t present_mode_count = 0;
          RVK_FN(vkGetPhysicalDeviceSurfacePresentModesKHR(device, 
                                                            m_vk_khr_surface, 
                                                            &present_mode_count,
                                                            nullptr));
          if (ok()) {
            ASSERT(present_mode_count != 0);
            details.present_modes.resize(present_mode_count);
            RVK_FN(vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                              m_vk_khr_surface,
                                                              &present_mode_count,
                                                              details.present_modes.data()));
          }
        }
      }
      return details;
    }

    bool swapchain_ok(VkPhysicalDevice device) {
      swapchain_support_details details = 
        query_swapchain_support(device);
      
      return !details.formats.empty() && 
             !details.present_modes.empty();
    }

    bool check_device_extensions(VkPhysicalDevice device) {
      bool r = false;
      if (ok()) {
        uint32_t extension_count;
        RVK_FN(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));

        if (ok()) {
          std::vector<VkExtensionProperties> avail_ext(extension_count);

          RVK_FN(vkEnumerateDeviceExtensionProperties(device, 
                                                      nullptr, 
                                                      &extension_count, 
                                                      avail_ext.data()));
          
          if (ok()) {
            std::set<std::string> required_ext(s_device_extensions.begin(), 
                                               s_device_extensions.end());

            for (uint32_t i = 0; i < extension_count; ++i) {
              required_ext.erase(avail_ext[i].extensionName);
            }

            r = required_ext.empty();
          }
        }
      }
      return r;
    }

    void setup_device_and_queues() {
      if (ok_pdev()) {
        queue_family_indices indices = 
          query_queue_families(m_vk_curr_pdevice, m_vk_khr_surface);
        
        ASSERT(indices.ok());

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

        std::set<uint32_t> unique_queue_indices = {
          indices.present_family.value(),
          indices.graphics_family.value()
        };

        write_logf("present family queue: %" PRIu32 "; graphics family queue: %" PRIu32 "\n",
                    indices.present_family.value(),
                    indices.graphics_family.value());

        float priority = 1.0f;
        for (uint32_t queue: unique_queue_indices) {
          VkDeviceQueueCreateInfo queue_create_info = {};
          queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
          queue_create_info.queueFamilyIndex = queue;
          queue_create_info.queueCount = 1;
          queue_create_info.pQueuePriorities = &priority;
          queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures dev_features = {};
        
        VkDeviceCreateInfo dev_create_info = {};
        
        dev_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        dev_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        dev_create_info.pQueueCreateInfos = queue_create_infos.data();
        
        dev_create_info.pEnabledFeatures = &dev_features;
        
        auto avail_layers = query_layers();

        dev_create_info.enabledLayerCount = static_cast<uint32_t>(avail_layers.size());
        dev_create_info.ppEnabledLayerNames = ( !avail_layers.empty() )
                                        ? avail_layers.data()
                                        : nullptr;

        dev_create_info.enabledExtensionCount = static_cast<uint32_t>(s_device_extensions.size());
        dev_create_info.ppEnabledExtensionNames = s_device_extensions.data();

        RVK_FN(vkCreateDevice(m_vk_curr_pdevice, 
                              &dev_create_info, 
                              nullptr, 
                              &m_vk_curr_ldevice));

        if (ok_ldev()) {
          vkGetDeviceQueue(m_vk_curr_ldevice, 
                           indices.graphics_family.value(), 
                           0, 
                           &m_vk_graphics_queue);

          vkGetDeviceQueue(m_vk_curr_ldevice,
                          indices.present_family.value(),
                          0,
                          &m_vk_present_queue);
        }

        ASSERT(m_vk_graphics_queue != VK_NULL_HANDLE);
      }
    }

    bool setup_surface() {
      if (ok()) {
        RVK_FN(glfwCreateWindowSurface(m_vk_instance, 
                                       g_m.device_ctx->window(),
                                       nullptr,
                                       &m_vk_khr_surface));
      }
      return m_vk_khr_surface != VK_NULL_HANDLE;
    }

  public:
    ~renderer() {
      free_mem();
    }

    bool ok() const {
      bool r = m_vk_result == VK_SUCCESS;
      ASSERT(r);
      return r; 
    }

    bool ok_surface() const {
      bool r = ok() && m_vk_khr_surface != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_pdev() const { 
      bool r = ok_surface() && m_vk_curr_pdevice != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_ldev() const {
      bool r = ok_pdev() && m_vk_curr_ldevice != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    bool ok_swapchain() const {
      bool r = ok_ldev() && m_vk_khr_swapchain != VK_NULL_HANDLE;
      ASSERT(r);
      return r;
    }

    uint32_t num_devices() const { return m_vk_physical_devs.size(); }

    bool is_device_suitable(VkPhysicalDevice device) {
      VkPhysicalDeviceProperties properties;
      VkPhysicalDeviceFeatures features;
      vkGetPhysicalDeviceProperties(device, &properties);
      vkGetPhysicalDeviceFeatures(device, &features);

      bool type_ok = 
        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

      queue_family_indices indices = query_queue_families(device, m_vk_khr_surface);

      bool extensions_supported = check_device_extensions(device);

      return type_ok && 
             indices.ok() && 
             extensions_supported && 
             swapchain_ok(device);
    }

    void set_physical_device(uint32_t device) {
      ASSERT(device < num_devices());
      if (ok()) {
        bool can_use = is_device_suitable(m_vk_physical_devs[device]);
        ASSERT(can_use);
        if (can_use) {
          m_vk_curr_pdevice = m_vk_physical_devs[device];
        }
      }
    }

    void print_device_info(uint32_t device) {
      if (ok()) {
        if (device < m_vk_physical_devs.size()) {
          VkPhysicalDeviceProperties props = {};
          vkGetPhysicalDeviceProperties(m_vk_physical_devs[device],
                                       &props);

          std::stringstream ss; 

          ss << "device " << device << "\n"
             << "--- apiVersion: " << props.apiVersion << "\n"
             << "--- driverVersion: " << props.driverVersion << "\n"
             << "--- vendorID: " << SS_HEX(props.vendorID) << "\n"
             << "--- deviceID: " << SS_HEX(props.deviceID) << "\n"
             << "--- deviceType: " << SS_HEX(props.deviceType) << "\n"
             << "--- deviceName: " << props.deviceName << "\n"
             << "--- piplineCacheUUID: <OMITTED> \n" 
             << "--- limits: <OMITTED> \n"
             << "--- sparseProperties: <OMITTED> \n";

          write_logf("%s", ss.str().c_str());
        }
      }
    }

    void query_physical_devices() {
      if (ok()) {
        uint32_t device_count = 0;
        RVK_FN(vkEnumeratePhysicalDevices(m_vk_instance, 
                                          &device_count, 
                                          nullptr));

        if (ok()) {
          m_vk_physical_devs.resize(device_count);

          RVK_FN(vkEnumeratePhysicalDevices(m_vk_instance,
                                            &device_count,
                                            &m_vk_physical_devs[0]));
        }
      }
    }

    darray<const char*> query_layers() {
      darray<const char*> ret;
      if (ok()) {
        uint32_t layer_count = 0;
        RVK_FN(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
        if (ok()) {
          darray<VkLayerProperties> avail_layers(layer_count);
          RVK_FN(vkEnumerateInstanceLayerProperties(&layer_count, &avail_layers[0]));

          std::stringstream ss;
          ss << "Vulkan Layers found:\n";
          for (const auto& properties: avail_layers) {
            ss << "--- " << properties.layerName << "\n";
          }
          write_logf("%s", ss.str().c_str());

          for (const auto& info: s_layers) {
            if (info.enable) {
              for (const auto& properties: avail_layers) {
                if (strcmp(properties.layerName, info.name) == 0) {
                  ret.push_back(info.name);
                  break;
                }
              }
            }
          }
        }
      }
      return ret;
    }

    bool init_context() {
      auto avail_layers = query_layers();

      VkApplicationInfo app_info = {};
      VkInstanceCreateInfo create_info = {};

      app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      app_info.pApplicationName = "Renderer";
      app_info.apiVersion = VK_API_VERSION_1_1;
      app_info.applicationVersion = 1;

      create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      create_info.pApplicationInfo = &app_info;

      uint32_t ext_count = 0;
      const char** extensions = glfwGetRequiredInstanceExtensions(&ext_count);

      create_info.enabledExtensionCount = ext_count;
      create_info.ppEnabledExtensionNames = extensions;

      create_info.enabledLayerCount = static_cast<uint32_t>(avail_layers.size());
      create_info.ppEnabledLayerNames = ( !avail_layers.empty() )
                                        ? avail_layers.data()
                                        : nullptr;

      std::stringstream ss ;
      ss << "Creating Vulkan instance with the following GLFW extensions:\n";
      for (uint32_t i = 0; i < ext_count; ++i) {
        ss << "--- " << extensions[i] << "\n";
      }
      write_logf("%s\n", ss.str().c_str());

      RVK_FN(vkCreateInstance(&create_info, nullptr, &m_vk_instance));

      if (ok()) {
        if (setup_surface()) {
          query_physical_devices();
        }
      }

      return m_vk_result == VK_SUCCESS;
    }

    void setup_swapchain() {
      if (ok_ldev()) {
        if (swapchain_ok(m_vk_curr_pdevice)) {
          swapchain_support_details details = query_swapchain_support(m_vk_curr_pdevice);
          // Make sure that we have the standard SRGB colorspace available
          VkSurfaceFormatKHR surface_format;
          {
            bool chosen = false;
            for (const auto& format: details.formats) {
              if (format.format == VK_FORMAT_B8G8R8A8_UNORM
                    && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surface_format = format;
                chosen = true;
                break;
              }
            }
            ASSERT(chosen);
          }

          // FIFO_KHR implies vertical sync. There may be a slight latency,
          // but for our purposes this shouldn't a problem.
          // We can deal with potential issues that can occur
          // with MAILBOX_KHR (provides triple buffering/lower latency)
          // if necessary.
          VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
          
          // Here we select the swapchain dimensions (the dimensions of the images that have been
          // rendered into). We want to make them as close as possible to the actual
          // window dimensions; sometimes this will be exact, other times it won't, depending
          // on the system.
          //
          // When the swapchain capabilities are queried, the driver will set the currentExtent
          // width and height to the window dimensions if it asks us to match the dimensions with the window.
          // Otherwise, we have a bit more wiggle room. The driver tells us this by setting
          // currentExtent.width to UINT32_MAX. 
          //
          // For now, it's best to keep things simple and be conservative, 
          // so we'll operate under the (likely) assumption that UINT32_MAX _is not_ set, 
          // and add support for more varied dimension requirements
          // if they pop up in the future.
          VkExtent2D swap_extent;
          {
            ASSERT(details.capabilities.currentExtent.width != UINT32_MAX);
            swap_extent = details.capabilities.currentExtent;
          }

          // Image count represents the amount of images on the swapchain.
          // We'll use a small image count for now. Simple semantics first;
          // we can optimize later as necessary.
          uint32_t image_count = details.capabilities.minImageCount;
          ASSERT(image_count != 0);
          if (image_count == 1) {
            image_count++;
          }
          ASSERT(image_count <= details.capabilities.maxImageCount);

          //
          // Here we actually create the swapchain.
          //
          // --
          // On imageArrayLayers: always set to 1 unless we want to use 
          // stereoscopic 3D rendering.
          //
          // --
          // On imageUsage: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT implies that render commands
          // will write directly to the images on the swapchain. If we want to perform
          // offscreen rendering, something like VK_IMAGE_USAGE_TRANSFER_DST_BIT
          // is considered an option.
          //
          // --
          // On imageSharingMode: something to keep in mind for queue family indices is that
          // both the graphics family queue and present family queue
          // can be shared (use the same family index). 
          //
          // Assuming that they are _not_ shared, then images will be drawn _on_
          // the swapchain using the graphics queue. They will then be submitted 
          // through the presentation queue.
          //
          // VK_SHARING_MODE_EXCLUSIVE implies that an image is owned by one
          // queue family at a time and ownership must be explicitly transferred before
          // using it in another queue family; this is what we use if the indices
          // are the same.
          //
          // VK_SHARING_MODE_CONCURRENT implies that images can be used across
          // multiple queue families without explicit ownership transfers. In advance,
          // we specify which queue family indices the transfers will occur between.
          //
          // --
          // On preTransform: represents a transform that is applied to images on the swapchain.
          // Can range from anything to a 90 degreee clockwise rotation or a flip. For now,
          // there is no need, so we simply use the current transform that's provided by 
          // the swapchain capabilities.
          //
          // --
          // On compositeAlpha: use VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR to ensure that we don't
          // use the alpha channel to perform a blend operation with other windows in the 
          // window system.
          //
          // --
          // On clipped: when other windows in the window system obscure portions of our 
          // window, we can use to tell the driver to not draw those portions. 
          // While the tutorial this references suggests using VK_TRUE here,
          // we instead use VK_FALSE. Technically speaking there isn't a clear indication
          // that using VK_TRUE will cause issues, but considering that framebuffer reads
          // are crucial to this application, it makes sense to avoid any potential
          // sources of error, as using VK_TRUE will definitely prevent writes to portions
          // of the screen that could be important for post processing (example: post processing
          // that relies on interpolation of already written data that's adjacent to something
          // which is visible, but itself is currently _not_ visible).
          //
          // --
          // On oldSwapChain: at the time of this writing, we keep this set to VK_NULL_HANDLE. This will need to be
          // none-null once we support things like window resizing, or anything that requires
          // re-initialization of the swapchain.
          // 
          {
            VkSwapchainCreateInfoKHR create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

            create_info.surface = m_vk_khr_surface;
            
            create_info.minImageCount = image_count;
            
            create_info.imageExtent = swap_extent;

            create_info.imageFormat = surface_format.format;
            create_info.imageColorSpace = surface_format.colorSpace;
            create_info.imageArrayLayers = 1;
            create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            queue_family_indices queue_indices = query_queue_families(m_vk_curr_pdevice, 
                                                                      m_vk_khr_surface);
            
            std::array<uint32_t, 2> array_indices = {
              queue_indices.graphics_family.value(),
              queue_indices.present_family.value()
            };

            if (queue_indices.graphics_family != queue_indices.present_family) {
              create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
              create_info.queueFamilyIndexCount = 2;
              create_info.pQueueFamilyIndices = array_indices.data();
            }
            else {
              create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
              create_info.queueFamilyIndexCount = 0;
              create_info.pQueueFamilyIndices = nullptr;
            }

            create_info.preTransform = details.capabilities.currentTransform;

            create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

            create_info.presentMode = present_mode;
            create_info.clipped = VK_FALSE;

            create_info.oldSwapchain = VK_NULL_HANDLE;

            RVK_FN(vkCreateSwapchainKHR(m_vk_curr_ldevice, &create_info, nullptr, &m_vk_khr_swapchain));

            if (ok_swapchain()) {
              uint32_t count = 0;
              RVK_FN(vkGetSwapchainImagesKHR(m_vk_curr_ldevice, 
                                             m_vk_khr_swapchain, 
                                             &count,
                                             nullptr));
              if (ok_swapchain()) {
                m_vk_swapchain_images.resize(count);
                RVK_FN(vkGetSwapchainImagesKHR(m_vk_curr_ldevice, 
                                              m_vk_khr_swapchain, 
                                              &count,
                                              m_vk_swapchain_images.data()));
              }
            }
          }
        }
      }
    }

    void setup_graphics_pipeline() {
      setup_device_and_queues();
      setup_swapchain();
    }

    template <class vkHandleType, void (*vkDestroyFn)(vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_handle(vkHandleType& handle) {
      if (handle != VK_NULL_HANDLE) {
        vkDestroyFn(handle, nullptr);
        handle = VK_NULL_HANDLE;
      }
    }

    template <class vkHandleType, 
              void (*vkDestroyFn)(VkInstance, vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_instance_handle(vkHandleType& handle) {
      if (handle != VK_NULL_HANDLE) {
        vkDestroyFn(m_vk_instance, handle, nullptr);
        handle = VK_NULL_HANDLE;
      }
    }

    template <class vkHandleType,
              void (*vkDestroyFn)(VkDevice, vkHandleType, const VkAllocationCallbacks*)>
    void free_vk_ldevice_handle(vkHandleType& handle) {
      if (ok_ldev()) {
        if (handle != VK_NULL_HANDLE) {
          vkDestroyFn(m_vk_curr_ldevice, handle, nullptr);
          handle = VK_NULL_HANDLE;
        }
      }
    }

    void free_mem() {
      free_vk_ldevice_handle<VkSwapchainKHR, &vkDestroySwapchainKHR>(m_vk_khr_swapchain);

      // vkDestroyDevice will destroy any associated queues along with it.
      free_vk_handle<VkDevice, &vkDestroyDevice>(m_vk_curr_ldevice);
      free_vk_instance_handle<VkSurfaceKHR, &vkDestroySurfaceKHR>(m_vk_khr_surface);
      free_vk_handle<VkInstance, &vkDestroyInstance>(m_vk_instance);
    }
  };
}

#endif // BASE_ENABLE_VULKAN