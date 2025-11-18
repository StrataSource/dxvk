// Copyright (c) 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "optimizer.hpp"

#include <cassert>
#include <charconv>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "build_module.h"
#include "pass_manager.h"
#include "log.h"
#include "strip_nonsemantic_info_pass.h"
#include "spirv_optimizer_options.h"
#include "make_unique.h"
#include "string_utils.h"

namespace spvtools {

std::vector<std::string> GetVectorOfStrings(const char** strings,
                                            const size_t string_count) {
  std::vector<std::string> result;
  for (uint32_t i = 0; i < string_count; i++) {
    result.emplace_back(strings[i]);
  }
  return result;
}

struct Optimizer::PassToken::Impl {
  Impl(std::unique_ptr<opt::Pass> p) : pass(std::move(p)) {}

  std::unique_ptr<opt::Pass> pass;  // Internal implementation pass.
};

Optimizer::PassToken::PassToken(
    std::unique_ptr<Optimizer::PassToken::Impl> impl)
    : impl_(std::move(impl)) {}

Optimizer::PassToken::PassToken(std::unique_ptr<opt::Pass>&& pass)
    : impl_(MakeUnique<Optimizer::PassToken::Impl>(std::move(pass))) {}

Optimizer::PassToken::PassToken(PassToken&& that)
    : impl_(std::move(that.impl_)) {}

Optimizer::PassToken& Optimizer::PassToken::operator=(PassToken&& that) {
  impl_ = std::move(that.impl_);
  return *this;
}

Optimizer::PassToken::~PassToken() {}

struct Optimizer::Impl {
  explicit Impl(spv_target_env env) : target_env(env), pass_manager() {}

  spv_target_env target_env;      // Target environment.
  opt::PassManager pass_manager;  // Internal implementation pass manager.
  std::unordered_set<uint32_t> live_locs;  // Arg to debug dead output passes
};

Optimizer::Optimizer(spv_target_env env) : impl_(new Impl(env)) {
  assert(env != SPV_ENV_WEBGPU_0);
}

Optimizer::~Optimizer() {}

void Optimizer::SetMessageConsumer(MessageConsumer c) {
  // All passes' message consumer needs to be updated.
  for (uint32_t i = 0; i < impl_->pass_manager.NumPasses(); ++i) {
    impl_->pass_manager.GetPass(i)->SetMessageConsumer(c);
  }
  impl_->pass_manager.SetMessageConsumer(std::move(c));
}

const MessageConsumer& Optimizer::consumer() const {
  return impl_->pass_manager.consumer();
}

Optimizer& Optimizer::RegisterPass(PassToken&& p) {
  // Change to use the pass manager's consumer.
  p.impl_->pass->SetMessageConsumer(consumer());
  impl_->pass_manager.AddPass(std::move(p.impl_->pass));
  return *this;
}

void Optimizer::SetTargetEnv(const spv_target_env env) {
  impl_->target_env = env;
}

bool Optimizer::Run(const uint32_t* original_binary,
                    const size_t original_binary_size,
                    std::vector<uint32_t>* optimized_binary) const {
  return Run(original_binary, original_binary_size, optimized_binary,
             OptimizerOptions());
}

bool Optimizer::Run(const uint32_t* original_binary,
                    const size_t original_binary_size,
                    std::vector<uint32_t>* optimized_binary,
                    const ValidatorOptions& validator_options,
                    bool skip_validation) const {
  OptimizerOptions opt_options;
  opt_options.set_run_validator(!skip_validation);
  opt_options.set_validator_options(validator_options);
  return Run(original_binary, original_binary_size, optimized_binary,
             opt_options);
}

bool Optimizer::Run(const uint32_t* original_binary,
                    const size_t original_binary_size,
                    std::vector<uint32_t>* optimized_binary,
                    const spv_optimizer_options opt_options) const {
  std::unique_ptr<opt::IRContext> context = BuildModule(
      impl_->target_env, consumer(), original_binary, original_binary_size);
  if (context == nullptr) return false;

  context->set_max_id_bound(opt_options->max_id_bound_);
  context->set_preserve_bindings(opt_options->preserve_bindings_);
  context->set_preserve_spec_constants(opt_options->preserve_spec_constants_);

  impl_->pass_manager.SetValidatorOptions(&opt_options->val_options_);
  impl_->pass_manager.SetTargetEnv(impl_->target_env);
  auto status = impl_->pass_manager.Run(context.get());

  if (status == opt::Pass::Status::Failure) {
    return false;
  }

#ifndef NDEBUG
  // We do not keep the result id of DebugScope in struct DebugScope.
  // Instead, we assign random ids for them, which results in integrity
  // check failures. In addition, propagating the OpLine/OpNoLine to preserve
  // the debug information through transformations results in integrity
  // check failures. We want to skip the integrity check when the module
  // contains DebugScope or OpLine/OpNoLine instructions.
  if (status == opt::Pass::Status::SuccessWithoutChange &&
      !context->module()->ContainsDebugInfo()) {
    std::vector<uint32_t> optimized_binary_with_nop;
    context->module()->ToBinary(&optimized_binary_with_nop,
                                /* skip_nop = */ false);
    assert(optimized_binary_with_nop.size() == original_binary_size &&
           "Binary size unexpectedly changed despite the optimizer saying "
           "there was no change");

    // Compare the magic number to make sure the binaries were encoded in the
    // endianness.  If not, the contents of the binaries will be different, so
    // do not check the contents.
    if (optimized_binary_with_nop[0] == original_binary[0]) {
      assert(memcmp(optimized_binary_with_nop.data(), original_binary,
                    original_binary_size) == 0 &&
             "Binary content unexpectedly changed despite the optimizer saying "
             "there was no change");
    }
  }
#endif  // !NDEBUG

  // Note that |original_binary| and |optimized_binary| may share the same
  // buffer and the below will invalidate |original_binary|.
  optimized_binary->clear();
  context->module()->ToBinary(optimized_binary, /* skip_nop = */ true);

  return true;
}

Optimizer& Optimizer::SetPrintAll(std::ostream* out) {
  impl_->pass_manager.SetPrintAll(out);
  return *this;
}

Optimizer& Optimizer::SetTimeReport(std::ostream* out) {
  impl_->pass_manager.SetTimeReport(out);
  return *this;
}

Optimizer& Optimizer::SetValidateAfterAll(bool validate) {
  impl_->pass_manager.SetValidateAfterAll(validate);
  return *this;
}

Optimizer::PassToken CreateStripNonSemanticInfoPass() {
  return MakeUnique<Optimizer::PassToken::Impl>(
      MakeUnique<opt::StripNonSemanticInfoPass>());
}

std::vector<const char*> Optimizer::GetPassNames() const {
  std::vector<const char*> v;
  for (uint32_t i = 0; i < impl_->pass_manager.NumPasses(); i++) {
    v.push_back(impl_->pass_manager.GetPass(i)->name());
  }
  return v;
}
}  // namespace spvtools

extern "C" {

SPIRV_TOOLS_EXPORT spv_optimizer_t* spvOptimizerCreate(spv_target_env env) {
  return reinterpret_cast<spv_optimizer_t*>(new spvtools::Optimizer(env));
}

SPIRV_TOOLS_EXPORT void spvOptimizerDestroy(spv_optimizer_t* optimizer) {
  delete reinterpret_cast<spvtools::Optimizer*>(optimizer);
}

SPIRV_TOOLS_EXPORT void spvOptimizerSetMessageConsumer(
    spv_optimizer_t* optimizer, spv_message_consumer consumer) {
  reinterpret_cast<spvtools::Optimizer*>(optimizer)->
      SetMessageConsumer(
          [consumer](spv_message_level_t level, const char* source,
                     const spv_position_t& position, const char* message) {
            return consumer(level, source, &position, message);
          });
}

SPIRV_TOOLS_EXPORT
spv_result_t spvOptimizerRun(spv_optimizer_t* optimizer,
                             const uint32_t* binary,
                             const size_t word_count,
                             spv_binary* optimized_binary,
                             const spv_optimizer_options options) {
  std::vector<uint32_t> optimized;

  if (!reinterpret_cast<spvtools::Optimizer*>(optimizer)->
      Run(binary, word_count, &optimized, options)) {
    return SPV_ERROR_INTERNAL;
  }

  auto result_binary = new spv_binary_t();
  if (!result_binary) {
      *optimized_binary = nullptr;
      return SPV_ERROR_OUT_OF_MEMORY;
  }

  result_binary->code = new uint32_t[optimized.size()];
  if (!result_binary->code) {
      delete result_binary;
      *optimized_binary = nullptr;
      return SPV_ERROR_OUT_OF_MEMORY;
  }
  result_binary->wordCount = optimized.size();

  memcpy(result_binary->code, optimized.data(),
         optimized.size() * sizeof(uint32_t));

  *optimized_binary = result_binary;

  return SPV_SUCCESS;
}

}  // extern "C"
