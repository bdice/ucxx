/**
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * See file LICENSE for terms.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <ucp/api/ucp.h>

#include <ucxx/component.h>
#include <ucxx/config.h>
#include <ucxx/constructors.h>
#include <ucxx/initializer.h>
#include <ucxx/log.h>
#include <ucxx/utils.h>

namespace ucxx {

class UCXXWorker;

class UCXXContext : public UCXXComponent {
 private:
  ucp_context_h _handle{nullptr};
  UCXXConfig _config{};
  uint64_t _feature_flags{0};
  bool _cuda_support{false};

 public:
  static constexpr uint64_t default_feature_flags =
    UCP_FEATURE_TAG | UCP_FEATURE_WAKEUP | UCP_FEATURE_STREAM | UCP_FEATURE_AM | UCP_FEATURE_RMA;

  UCXXContext()                   = delete;
  UCXXContext(const UCXXContext&) = delete;
  UCXXContext& operator=(UCXXContext const&) = delete;
  UCXXContext(UCXXContext&& o)               = delete;
  UCXXContext& operator=(UCXXContext&& o) = delete;

  UCXXContext(UCXXConfigMap ucx_config, uint64_t feature_flags)
    : _config{UCXXConfig(ucx_config)}, _feature_flags{feature_flags}
  {
    ucp_params_t ucp_params;

    ucxx::UCXXInitializer::getInstance();

    // UCP
    std::memset(&ucp_params, 0, sizeof(ucp_params));
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES;
    ucp_params.features   = feature_flags;

    assert_ucs_status(ucp_init(&ucp_params, this->_config.get_handle(), &this->_handle));

    // UCX supports CUDA if "cuda" is part of the TLS or TLS is "all"
    auto config_map = this->_config.get();
    auto tls        = config_map.find("TLS");
    if (tls != config_map.end())
      this->_cuda_support = tls->second == "all" || tls->second.find("cuda") != std::string::npos;

    ucxx_info("UCP initiated using config: ");
    for (const auto& kv : config_map)
      ucxx_info("  %s: %s", kv.first.c_str(), kv.second.c_str());
  }

  static std::shared_ptr<UCXXContext> create(UCXXConfigMap ucx_config, uint64_t feature_flags)
  {
    // TODO: make constructor private, probably using pImpl
    return std::make_shared<UCXXContext>(ucx_config, feature_flags);
  }

  ~UCXXContext()
  {
    if (this->_handle != nullptr) ucp_cleanup(this->_handle);
  }

  UCXXConfigMap get_config() { return this->_config.get(); }

  ucp_context_h get_handle() { return this->_handle; }

  std::string get_info()
  {
    FILE* text_fd = create_text_fd();
    ucp_context_print_info(this->_handle, text_fd);
    return decode_text_fd(text_fd);
  }

  uint64_t get_feature_flags() const { return _feature_flags; }

  std::shared_ptr<UCXXWorker> createWorker()
  {
    auto context = std::dynamic_pointer_cast<UCXXContext>(shared_from_this());
    auto worker  = ucxx::createWorker(context);
    return worker;
  }
};

}  // namespace ucxx
