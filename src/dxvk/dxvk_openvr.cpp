#include "dxvk_instance.h"
#include "dxvk_openvr.h"
#include "../util/util_win32_compat.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include "openvr.h"

namespace dxvk {
  VrInstance VrInstance::s_instance;

  VrInstance:: VrInstance() {
    m_no_vr = env::getEnvVar("DXVK_VR") != "1";
  }
  VrInstance::~VrInstance() { }


  std::string_view VrInstance::getName() {
    return "OpenVR";
  }
  
  
  DxvkNameSet VrInstance::getInstanceExtensions() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return m_insExtensions;
  }


  DxvkNameSet VrInstance::getDeviceExtensions(uint32_t adapterId) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    if (adapterId < m_devExtensions.size())
      return m_devExtensions[adapterId];
    
    return DxvkNameSet();
  }


  void VrInstance::initInstanceExtensions() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_no_vr || m_initializedDevExt)
        return;

  #ifdef _WIN32
    if (!m_vr_key)
    {
        LSTATUS status;

        if ((status = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Wine\\VR", 0, KEY_READ, &m_vr_key)))
            Logger::info(str::format("OpenVR: could not open registry key, status ", status));
    }
  #endif

    if (!m_vr_key && !m_compositor)
      m_compositor = this->getCompositor();

    if (!m_vr_key && !m_compositor)
      return;
    
    m_insExtensions = this->queryInstanceExtensions();
    m_initializedInsExt = true;
  }


  void VrInstance::initDeviceExtensions(const DxvkInstance* instance) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (m_no_vr || (!m_vr_key && !m_compositor) || m_initializedDevExt)
      return;
    
    for (uint32_t i = 0; instance->enumAdapters(i) != nullptr; i++) {
      m_devExtensions.push_back(this->queryDeviceExtensions(
        instance->enumAdapters(i)));
    }

    m_initializedDevExt = true;
    this->shutdown();
  }

  bool VrInstance::waitVrKeyReady() const {
  #ifdef _WIN32
    DWORD type, value, wait_status, size;
    LSTATUS status;
    HANDLE event;

    size = sizeof(value);
    if ((status = RegQueryValueExA(m_vr_key, "state", nullptr, &type, reinterpret_cast<BYTE*>(&value), &size)))
    {
        Logger::err(str::format("OpenVR: could not query value, status ", status));
        return false;
    }
    if (type != REG_DWORD)
    {
        Logger::err(str::format("OpenVR: unexpected value type ", type));
        return false;
    }

    if (value)
        return value == 1;

    event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    while (1)
    {
        if (RegNotifyChangeKeyValue(m_vr_key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, event, TRUE))
        {
            Logger::err("Error registering registry change notification");
            goto done;
        }
        size = sizeof(value);
        if ((status = RegQueryValueExA(m_vr_key, "state", nullptr, &type, reinterpret_cast<BYTE*>(&value), &size)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            goto done;
        }
        if (value)
            break;
        while ((wait_status = WaitForSingleObject(event, 1000)) == WAIT_TIMEOUT)
            Logger::warn("VR state wait timeout (retrying)");

        if (wait_status != WAIT_OBJECT_0)
        {
            Logger::err(str::format("Got unexpected wait status ", wait_status));
            break;
        }
    }

  done:
    CloseHandle(event);
    return value == 1;
  #else
    return 0;
  #endif
  }

  DxvkNameSet VrInstance::queryInstanceExtensions() const {
    std::vector<char> extensionList;
    DWORD len;

  #ifdef _WIN32
    if (m_vr_key)
    {
        LSTATUS status;
        DWORD type;

        if (!this->waitVrKeyReady())
            return DxvkNameSet();

        len = 0;
        if ((status = RegQueryValueExA(m_vr_key, "openvr_vulkan_instance_extensions", nullptr, &type, nullptr, &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
        extensionList.resize(len);
        if ((status = RegQueryValueExA(m_vr_key, "openvr_vulkan_instance_extensions", nullptr, &type, reinterpret_cast<BYTE*>(extensionList.data()), &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
    }
    else
  #endif
    {
        len = m_compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
        extensionList.resize(len);
        len = m_compositor->GetVulkanInstanceExtensionsRequired(extensionList.data(), len);
    }
    return parseExtensionList(std::string(extensionList.data(), len));
  }


  DxvkNameSet VrInstance::queryDeviceExtensions(Rc<DxvkAdapter> adapter) const {
    std::vector<char> extensionList;
    DWORD len;

  #ifdef _WIN32
    if (m_vr_key)
    {
        LSTATUS status;
        char name[256];
        DWORD type;

        if (!this->waitVrKeyReady())
            return DxvkNameSet();

        sprintf(name, "PCIID:%04x:%04x", adapter->deviceProperties().vendorID, adapter->deviceProperties().deviceID);
        len = 0;
        if ((status = RegQueryValueExA(m_vr_key, name, nullptr, &type, nullptr, &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
        extensionList.resize(len);
        if ((status = RegQueryValueExA(m_vr_key, name, nullptr, &type, reinterpret_cast<BYTE*>(extensionList.data()), &len)))
        {
            Logger::err(str::format("OpenVR: could not query value, status ", status));
            return DxvkNameSet();
        }
    }
    else
  #endif
    {
        len = m_compositor->GetVulkanDeviceExtensionsRequired(adapter->handle(), nullptr, 0);
        extensionList.resize(len);
        len = m_compositor->GetVulkanDeviceExtensionsRequired(adapter->handle(), extensionList.data(), len);
    }
    return parseExtensionList(std::string(extensionList.data(), len));
  }
  
  
  DxvkNameSet VrInstance::parseExtensionList(const std::string& str) const {
    DxvkNameSet result;
    
    std::stringstream strstream(str);
    std::string       section;
    
    while (std::getline(strstream, section, ' '))
      result.add(section.c_str());
    
    return result;
  }
  
  
  vr::IVRCompositor* VrInstance::getCompositor() {
    // Skip OpenVR initialization if requested

    if (!vr::VR_IsRuntimeInstalled()) {
      Logger::info("OpenVR: Failed to locate module");
      return nullptr;
    }

    // If the app has not initialized OpenVR yet, we need
    // to do it now in order to grab a compositor instance
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_InitInternal2(&error, vr::VRApplication_Background, nullptr);
    m_initializedOpenVr = error == vr::VRInitError_None;

    if (error != vr::VRInitError_None) {
      Logger::warn("OpenVR: Failed to initialize OpenVR");
      return nullptr;
    }

    vr::IVRCompositor* compositor = reinterpret_cast<vr::IVRCompositor*>(
      vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &error));

    if (error != vr::VRInitError_None || !compositor) {
      Logger::warn("OpenVR: Failed to query compositor interface");
      this->shutdown();
      return nullptr;
    }

    Logger::info("OpenVR: Compositor interface found");
    return compositor;
  }


  void VrInstance::shutdown() {
  #ifdef _WIN32
    if (m_vr_key)
    {
        RegCloseKey(m_vr_key);
        m_vr_key = nullptr;
    }
  #endif

    if (m_initializedOpenVr)
      vr::VR_ShutdownInternal();

    m_initializedOpenVr = false;
  }
}
