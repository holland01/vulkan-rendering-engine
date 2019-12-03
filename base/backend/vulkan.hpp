#pragma once

#include <vulkan/vulkan.h>
#include "common.hpp"

#define VK_FN(expr) vk_call((expr), #expr, __LINE__, __FILE__)

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

  class renderer {
    darray<VkPhysicalDevice> m_vk_physical_devs;
    darray<VkLayerProperties> m_vk_layers;

    VkInstance m_vk_instance {VK_NULL_HANDLE};

    VkResult m_vk_result{VK_SUCCESS};

    struct vk_layer_info {
      const char* name{nullptr};
      bool enabled{false};
    };

    static inline darray<vk_layer_info> s_layers = {
      { "VK_LAYER_KHRONOS_validation", true }
    };

  public:
    ~renderer() {
      free_mem();
    }

    bool ok() const { return m_vk_result == VK_SUCCESS; }

    uint32_t num_devices() const { return m_vk_physical_devs.size(); }

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

    bool init() {
      VkApplicationInfo app_info = {};
      VkInstanceCreateInfo create_info = {};

      app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      app_info.pApplicationName = "Renderer";
      app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);
      app_info.applicationVersion = 1;

      create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      create_info.pApplicationInfo = &app_info;

      uint32_t ext_count = 0;
      const char** extensions = glfwGetRequiredInstanceExtensions(&ext_count);

      create_info.enabledExtensionCount = ext_count;
      create_info.ppEnabledExtensionNames = extensions;

      std::stringstream ss ;
      ss << "Creating Vulkan instance with the following GLFW extensions:\n";
      for (uint32_t i = 0; i < ext_count; ++i) {
        ss << "--- " << extensions[i] << "\n";
      }
      write_logf("%s\n", ss.str().c_str());

      m_vk_result = VK_FN(vkCreateInstance(&create_info, nullptr, &m_vk_instance));

      if (m_vk_result == VK_SUCCESS) {
        uint32_t device_count = 0;
        m_vk_result = VK_FN(vkEnumeratePhysicalDevices(m_vk_instance, &device_count, nullptr));
        if (m_vk_result == VK_SUCCESS) {
          m_vk_physical_devs.resize(device_count);
          m_vk_result = VK_FN(vkEnumeratePhysicalDevices(m_vk_instance,
                                                         &device_count,
                                                         &m_vk_physical_devs[0]));
        }
      }

      return m_vk_result == VK_SUCCESS;
    }

    void free_mem() {
      if (m_vk_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_vk_instance, nullptr);
        m_vk_instance = VK_NULL_HANDLE;
      }
    }
  };
}