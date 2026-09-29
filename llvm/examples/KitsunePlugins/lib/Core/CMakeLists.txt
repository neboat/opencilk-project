add_llvm_component_library(LLVMKitCore
  # AddrSpaceUtils.cpp
  CommandLineOptions.cpp
  ConstantUtils.cpp
  EmbUtils.cpp
  # GlobalVariableUtils.cpp
  IntrinsicUtils.cpp
  KernelProperties.cpp
  # MetadataUtils.cpp
  ModuleUtils.cpp
  KitsuneTTOptions.cpp
  # TTPlugin.cpp
  TTUtils.cpp
  TargetUtils.cpp
  # TypeUtils.cpp

  # There is a circular dependence here between this library and LLVMCore. For
  # more details, see llvm/lib/IR/CMakeLists.txt. It would be nice if we could
  # get rid of these, but that is highly unlikely.
  LINK_COMPONENTS
  BitReader
  BitWriter
  CodeGen
  Core
  KitSupport
  MC
  Support
  TargetParser
)
