include(FetchContent)

# Archive identity is checked before extraction. No WAD download is performed.
set(ROOTDOOM_DOOMGENERIC_REVISION e257a09527062bfb986b6e5e0780839b6cbd5b71)
FetchContent_Declare(doomgeneric
  URL https://codeload.github.com/ozkl/doomgeneric/tar.gz/${ROOTDOOM_DOOMGENERIC_REVISION}
  URL_HASH SHA256=26f8f60daa6c6e4d3c01e5eac9fb700d592dede803d13ac848154ff356b2a2e7
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(doomgeneric)

if(NOT EXISTS "${doomgeneric_SOURCE_DIR}/doomgeneric/doomgeneric.h")
  message(FATAL_ERROR "Incomplete doomgeneric source at ${doomgeneric_SOURCE_DIR}")
endif()
