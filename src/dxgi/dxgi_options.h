#pragma once

#include "../util/config/config.h"

#include "../dxvk/dxvk_include.h"

#include "dxgi_include.h"

namespace dxvk {

  /**
   * \brief DXGI options
   * 
   * Per-app options that control the
   * behaviour of some DXGI classes.
   */
  struct DxgiOptions {
    DxgiOptions(const Config& config);

    /// Enable HDR
    bool enableHDR;

    /// Enable support for dummy composition swapchains
    bool enableDummyCompositionSwapchain;

    /// Limit frame rate
    int32_t maxFrameRate;

    /// Sync interval. Overrides the value
    /// passed to IDXGISwapChain::Present.
    int32_t syncInterval;
  };
  
}
