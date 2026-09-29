# This library *MUST NOT* depend on LLVMCore. It is ok to depend on LLVMSupport
# and any other libraries that LLVMSupport depends on. But it must never depend
# on any libraries that transitively depend on LLVMCore.

add_llvm_component_library(LLVMKitSupport
  # CommandLineUtils.cpp
  # ErrorUtils.cpp
  Deserialize.cpp
  OptznLevelUtils.cpp
  ToString.cpp
  TTIDUtils.cpp

  COMPONENT_NAME
  KitSupport

  LINK_COMPONENTS
  Support
)
