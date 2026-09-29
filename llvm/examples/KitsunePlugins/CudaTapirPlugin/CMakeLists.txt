set(KITSUNE_CUDA_ENABLED ON CACHE INTERNAL "Enable 'cuda' tapir target" FORCE)

# Lookup the unique directories containing the libraries in LIBS and add them
# to the list OUT.
macro(populate_unique_parents LIBS OUT)
  foreach (LIB IN LISTS ${LIBS})
    get_filename_component(DIR ${LIB} DIRECTORY)
    if (NOT DIR IN_LIST ${OUT})
      list(APPEND ${OUT} ${DIR})
    endif ()
  endforeach ()
endmacro ()

if (KITSUNE_CUDA_ENABLED)
  message(STATUS "Kitsune: cuda tapir target enabled")

  find_package(CUDAToolkit ${KITSUNE_CUDA_VERSION_MIN} REQUIRED)

  # CUDAToolkit does not provide a CUDAToolkit_PREFIX variable. CUDAToolkit_ROOT
  # is not set either, even if the CUDAToolkit is found. The prefix relies on
  # environment variables, so prefer those. Otherwise, make a best guess at
  # figuring out what it is from the variables that are set.
  #
  # Alternatively, we could require setting CUDA_PATH in the environment. That
  # is probably not necessary. On HPC machines (which generally give us the most
  # grief), CUDA_PATH is often set,so checking for it first should be ok. We
  # probably want to test this across a range of systems though.
  if (DEFINED $ENV{CUDA_PATH})
    message(STATUS "  Setting cuda prefix from CUDA_PATH environment variable")
    set(KISTUNE_CUDA_PREFIX $ENV{CUDA_PATH})
  elseif (DEFINED $ENV{CUDA_HOME})
    message(STATUS "  Setting cuda prefix from CUDA_HOME environment variable")
    set(KITSUNE_CUDA_PREFIX $ENV{CUDA_HOME})
  elseif (CUDAToolkit_ROOT)
    message(STATUS "  Setting cuda prefix from CUDAToolkit_ROOT")
    set(KITSUNE_CUDA_PREFIX ${CUDAToolkit_ROOT})
  else ()
    message(STATUS "  Setting cuda prefix to parent of CUDAToolkit_BIN_DIR")
    get_filename_component(KITSUNE_CUDA_PREFIX ${CUDAToolkit_BIN_DIR} DIRECTORY)
  endif ()
  if (NOT KITSUNE_CUDA_PREFIX)
    message(FATAL_ERROR "Could not determine cuda prefix")
  endif()

  set(KITSUNE_CUDA_VERSION_MAJOR ${CUDAToolkit_VERSION_MAJOR})
  set(KITSUNE_CUDA_VERSION_MINOR ${CUDAToolkit_VERSION_MINOR})
  set(KITSUNE_CUDA_VERSION_PATCH ${CUDAToolkit_VERSION_PATCH})

  set(KITSUNE_CUDA_INCLUDE_DIR ${CUDAToolkit_INCLUDE_DIRS})
  set(KITSUNE_CUDA_BINARY_DIR ${CUDAToolkit_BIN_DIR})
  set(KITSUNE_CUDA_LIBRARY_DIR ${CUDAToolkit_LIBRARY_DIR})

  message(STATUS "  Cuda version: ${KITSUNE_CUDA_VERSION_STRING}")
  message(STATUS "  Cuda prefix: ${KITSUNE_CUDA_PREFIX}")
  message(STATUS "  Cuda binary dir: ${KITSUNE_CUDA_BINARY_DIR}")
  message(STATUS "  Cuda include dir: ${KITSUNE_CUDA_INCLUDE_DIR}")
  message(STATUS "  Cuda library dir: ${KITSUNE_CUDA_LIBRARY_DIR}")

  # This should almost always be the libcuda that is installed on the system,
  # so look for it in the "usual" places.
  find_library(KITSUNE_CUDA_LIB_CUDA cuda REQUIRED)
  list(APPEND KITSUNE_CUDA_LIB_NAMES cuda)
  list(APPEND KITSUNE_CUDA_LIB_LIBS ${KITSUNE_CUDA_LIB_CUDA})

  find_library(KITSUNE_CUDA_LIB_CUDART_STATIC cudart_static REQUIRED
    PATHS ${KITSUNE_CUDA_LIBRARY_DIR}
    NO_DEFAULT_PATH)
  list(APPEND KITSUNE_CUDA_LIB_NAMES cudart_static)
  list(APPEND KITSUNE_CUDA_LIB_LIBS ${KITSUNE_CUDA_LIB_CUDART_STATIC})

  populate_unique_parents(KITSUNE_CUDA_LIB_LIBS KITSUNE_CUDA_LIB_DIRS)
  string(REPLACE ";" "|" KITSUNE_CUDA_LIB_NAMES "${KITSUNE_CUDA_LIB_NAMES}")
  string(REPLACE ";" "|" KITSUNE_CUDA_LIB_LIBS "${KITSUNE_CUDA_LIB_LIBS}")
  string(REPLACE ";" "|" KITSUNE_CUDA_LIB_DIRS "${KITSUNE_CUDA_LIB_DIRS}")

  find_program(KITSUNE_CUDA_PTXAS ptxas REQUIRED
    PATHS ${KITSUNE_CUDA_BINARY_DIR}
    NO_DEFAULT_PATH)

  find_program(KITSUNE_CUDA_FATBINARY fatbinary REQUIRED
    PATHS ${KITSUNE_CUDA_BINARY_DIR}
    NO_DEFAULT_PATH)

  set(KITSUNE_CUDA_ARCH_DEFAULT "sm_120"
    CACHE STRING
    "The default NVIDIA GPU architecture used by the cuda tapir target when a GPU is not detected on the system")

  set(KITSUNE_CUDA_EXTRA_PREPROCESSOR_FLAGS
    ""
    CACHE STRING
    "Additional preprocessor flags needed for the Cuda target")

  set(KITSUNE_CUDA_EXTRA_COMPILER_FLAGS
    ""
    CACHE STRING
    "Additional compiler flags needed for the Cuda target")

  set(KITSUNE_CUDA_EXTRA_LINKER_FLAGS
    ""
    CACHE STRING
    "Additional linker flags needed for the Cuda target")
endif()
set(KITSUNE_CUDA_VERSION_STRING "${KITSUNE_CUDA_VERSION_MAJOR}.${KITSUNE_CUDA_VERSION_MINOR}.${KITSUNE_CUDA_VERSION_PATCH}")


set(KITSUNE_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

set(SOURCES
  TTPluginCuda.cpp
  CudaABI.cpp
  ${KITSUNE_LIB_DIR}/Transforms/Tapir/KitsuneUtils.cpp
  ${KITSUNE_LIB_DIR}/Analysis/TapirTargetAnalysis.cpp
  )

# Using add_llvm_library is only needed because we are building this within
# LLVM's build system.
add_llvm_pass_plugin(tt-plugin-cuda
  ${SOURCES}
  DEPENDS
  intrinsics_gen
  BUILDTREE_ONLY

  LINK_COMPONENTS
  KitCodeGen
  KitCore
  KitSupport
  KitTransforms
)

target_compile_definitions(tt-plugin-cuda PUBLIC KITSUNE_CUDA_ENABLED)

# # Add include directories for the pass plugin target
# target_include_directories(tt-plugin-cuda PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/../include
# )
