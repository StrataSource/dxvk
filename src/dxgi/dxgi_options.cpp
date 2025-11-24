#include "dxgi_options.h"

#include <unordered_map>

namespace dxvk {

  static bool isNvapiEnabled() {
    return env::getEnvVar("DXVK_ENABLE_NVAPI") == "1";
  }


  static bool isHDRDisallowed(bool enableUe4Workarounds) {
#ifdef _WIN32
    // Unreal Engine 4 titles use AGS/NVAPI to try and enable
    // HDR globally.
    // The game checks IDXGIOutput::GetDesc1's ColorSpace
    // being HDR10 to see if it should enable HDR.
    // Many of these UE4 games statically link against AGS.
    //
    // This is a problem as when UE4 tries to enable HDR via AGS,
    // it does not check if AGSContext, and the display info etc
    // are nullptr unlike the rest of the code using AGS.
    // So we need to special-case UE4 titles to disable reporting a HDR
    // when they are in DX11 mode.
    //
    // The simplest way to do this is to key off the fact that all
    // UE4 titles have an executable ending with "-Win64-Shipping".
    //
    // We check if d3d12.dll is present, to determine what path in
    // UE4 we are on, as there are some games that ship both and support HDR.
    // (eg. The Dark Pictures: House of Ashes, 1281590)
    // Luckily for us, they only load d3d12.dll on the D3D12 render path
    // so we can key off that to force disable HDR only in D3D11.
    std::string exeName = env::getExeName();
    bool isUE4 = enableUe4Workarounds || exeName.find("-Win64-Shipping") != std::string::npos;
    bool hasD3D12 = GetModuleHandleA("d3d12") != nullptr;

    if (isUE4 && !hasD3D12 && !isNvapiEnabled())
      return true;
#endif
    return false;
  }

  
  DxgiOptions::DxgiOptions(const Config& config) {
    this->maxFrameRate = config.getOption<int32_t>("dxgi.maxFrameRate", 0);
    this->syncInterval = config.getOption<int32_t>("dxgi.syncInterval", -1);

    // We don't support dcomp swapchains and some games may rely on them failing on creation
    this->enableDummyCompositionSwapchain = config.getOption<bool>("dxgi.enableDummyCompositionSwapchain", false);

    this->enableHDR = config.getOption<bool>("dxgi.enableHDR", env::getEnvVar("DXVK_HDR") == "1");

    bool enableUe4Workarounds = config.getOption<bool>("dxgi.enableUe4Workarounds", false);

    if (this->enableHDR && isHDRDisallowed(enableUe4Workarounds)) {
      Logger::info("HDR was configured to be enabled, but has been force disabled as a UE4 DX11 game was detected.");
      this->enableHDR = false;
    }
  }
  
}
