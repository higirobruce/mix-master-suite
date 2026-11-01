# All third-party code is fetched here and pinned (ADR-0001).
#  - JUCE and Catch2: source tarball of one commit, verified by SHA256 (content hash).
#  - clap-juce-extensions: git at a full commit SHA (needs its clap/clap-helpers submodules).
# Tarballs are cached in <source>/.cache/downloads so a verified file is reused across configures and CI runs.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
set(MM_DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/.cache/downloads)
# Fetched projects may declare an old cmake_minimum_required; CMake 4 refuses < 3.5 without this.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# JUCE 8.0.16-security  (tag commit ea77444750de45e036e0279bb0291144bf8e3688)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/ea77444750de45e036e0279bb0291144bf8e3688.tar.gz
    URL_HASH SHA256=79181e00e8511c837983478f561db082ed08028d60c53a0af8ac85ea220299da
    DOWNLOAD_DIR ${MM_DOWNLOAD_DIR}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# free-audio/clap-juce-extensions main @ 2026-11-01 (submodules: clap 1.2.7, clap-helpers)
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SHALLOW    FALSE
    GIT_SUBMODULES_RECURSE TRUE)

# Catch2 v3.8.1 (tag commit 2b60af89e23d28eefc081bc930831ee9d45ea58b)
FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/2b60af89e23d28eefc081bc930831ee9d45ea58b.tar.gz
    URL_HASH SHA256=553e6c94498c42edaecc9c8459f1a320a7e4cf49155635cf7123212bb9f09cf2
    DOWNLOAD_DIR ${MM_DOWNLOAD_DIR}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "" FORCE)  # ON breaks static libs with JUCE 8.0.16

FetchContent_MakeAvailable(JUCE clap-juce-extensions)
if(MM_BUILD_TESTS)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
