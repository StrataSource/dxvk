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

#ifndef INCLUDE_SPIRV_TOOLS_OPTIMIZER_HPP_
#define INCLUDE_SPIRV_TOOLS_OPTIMIZER_HPP_

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "libspirv.hpp"

namespace spvtools {

namespace opt {
class Pass;
struct DescriptorSetAndBinding;
}  // namespace opt

// C++ interface for SPIR-V optimization functionalities. It wraps the context
// (including target environment and the corresponding SPIR-V grammar) and
// provides methods for registering optimization passes and optimizing.
//
// Instances of this class provides basic thread-safety guarantee.
class SPIRV_TOOLS_EXPORT Optimizer {
 public:
  // The token for an optimization pass. It is returned via one of the
  // Create*Pass() standalone functions at the end of this header file and
  // consumed by the RegisterPass() method. Tokens are one-time objects that
  // only support move; copying is not allowed.
  struct PassToken {
    struct SPIRV_TOOLS_LOCAL Impl;  // Opaque struct for holding internal data.

    PassToken(std::unique_ptr<Impl>);

    // Tokens for built-in passes should be created using Create*Pass functions
    // below; for out-of-tree passes, use this constructor instead.
    // Note that this API isn't guaranteed to be stable and may change without
    // preserving source or binary compatibility in the future.
    PassToken(std::unique_ptr<opt::Pass>&& pass);

    // Tokens can only be moved. Copying is disabled.
    PassToken(const PassToken&) = delete;
    PassToken(PassToken&&);
    PassToken& operator=(const PassToken&) = delete;
    PassToken& operator=(PassToken&&);

    ~PassToken();

    std::unique_ptr<Impl> impl_;  // Unique pointer to internal data.
  };

  // Constructs an instance with the given target |env|, which is used to decode
  // the binaries to be optimized later.
  //
  // The instance will have an empty message consumer, which ignores all
  // messages from the library. Use SetMessageConsumer() to supply a consumer
  // if messages are of concern.
  explicit Optimizer(spv_target_env env);

  // Disables copy/move constructor/assignment operations.
  Optimizer(const Optimizer&) = delete;
  Optimizer(Optimizer&&) = delete;
  Optimizer& operator=(const Optimizer&) = delete;
  Optimizer& operator=(Optimizer&&) = delete;

  // Destructs this instance.
  ~Optimizer();

  // Sets the message consumer to the given |consumer|. The |consumer| will be
  // invoked once for each message communicated from the library.
  void SetMessageConsumer(MessageConsumer consumer);

  // Returns a reference to the registered message consumer.
  const MessageConsumer& consumer() const;

  // Registers the given |pass| to this optimizer. Passes will be run in the
  // exact order of registration. The token passed in will be consumed by this
  // method.
  Optimizer& RegisterPass(PassToken&& pass);

  // Validates that |flag| has a valid format.  Strings accepted:
  //
  // --pass_name[=pass_args]
  // -O
  // -Os
  //
  // If |flag| takes one of the forms above, it returns true.  Otherwise, it
  // returns false.
  bool FlagHasValidForm(const std::string& flag) const;

  // Allows changing, after creation time, the target environment to be
  // optimized for and validated.  Should be called before calling Run().
  void SetTargetEnv(const spv_target_env env);

  // Optimizes the given SPIR-V module |original_binary| and writes the
  // optimized binary into |optimized_binary|. The optimized binary uses
  // the same SPIR-V version as the original binary.
  //
  // Returns true on successful optimization, whether or not the module is
  // modified. Returns false if |original_binary| fails to validate or if errors
  // occur when processing |original_binary| using any of the registered passes.
  // In that case, no further passes are executed and the contents in
  // |optimized_binary| may be invalid.
  //
  // By default, the binary is validated before any transforms are performed,
  // and optionally after each transform.  Validation uses SPIR-V spec rules
  // for the SPIR-V version named in the binary's header (at word offset 1).
  // Additionally, if the target environment is a client API (such as
  // Vulkan 1.1), then validate for that client API version, to the extent
  // that it is verifiable from data in the binary itself.
  //
  // It's allowed to alias |original_binary| to the start of |optimized_binary|.
  bool Run(const uint32_t* original_binary, size_t original_binary_size,
           std::vector<uint32_t>* optimized_binary) const;

  // DEPRECATED: Same as above, except passes |options| to the validator when
  // trying to validate the binary.  If |skip_validation| is true, then the
  // caller is guaranteeing that |original_binary| is valid, and the validator
  // will not be run.  The |max_id_bound| is the limit on the max id in the
  // module.
  bool Run(const uint32_t* original_binary, const size_t original_binary_size,
           std::vector<uint32_t>* optimized_binary,
           const ValidatorOptions& options, bool skip_validation) const;

  // Same as above, except it takes an options object.  See the documentation
  // for |OptimizerOptions| to see which options can be set.
  //
  // By default, the binary is validated before any transforms are performed,
  // and optionally after each transform.  Validation uses SPIR-V spec rules
  // for the SPIR-V version named in the binary's header (at word offset 1).
  // Additionally, if the target environment is a client API (such as
  // Vulkan 1.1), then validate for that client API version, to the extent
  // that it is verifiable from data in the binary itself, or from the
  // validator options set on the optimizer options.
  bool Run(const uint32_t* original_binary, const size_t original_binary_size,
           std::vector<uint32_t>* optimized_binary,
           const spv_optimizer_options opt_options) const;

  // Returns a vector of strings with all the pass names added to this
  // optimizer's pass manager. These strings are valid until the associated
  // pass manager is destroyed.
  std::vector<const char*> GetPassNames() const;

  // Sets the option to print the disassembly before each pass and after the
  // last pass.  If |out| is null, then no output is generated.  Otherwise,
  // output is sent to the |out| output stream.
  Optimizer& SetPrintAll(std::ostream* out);

  // Sets the option to print the resource utilization of each pass. If |out|
  // is null, then no output is generated. Otherwise, output is sent to the
  // |out| output stream.
  Optimizer& SetTimeReport(std::ostream* out);

  // Sets the option to validate the module after each pass.
  Optimizer& SetValidateAfterAll(bool validate);

 private:
  struct SPIRV_TOOLS_LOCAL Impl;  // Opaque struct for holding internal data.
  std::unique_ptr<Impl> impl_;    // Unique pointer to internal data.
};

// Creates a strip-nonsemantic-info pass.
// A strip-nonsemantic-info pass removes all reflections and explicitly
// non-semantic instructions.
Optimizer::PassToken CreateStripNonSemanticInfoPass();
}  // namespace spvtools

#endif  // INCLUDE_SPIRV_TOOLS_OPTIMIZER_HPP_
