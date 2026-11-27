# Read by CMake after Platform/Windows-GNU.cmake (CMAKE_USER_MAKE_RULES_OVERRIDE, set in mingw-w64.cmake).
# DLLs without the "lib" prefix: renderer.dll, nre_c.dll. Import libraries keep it (librenderer.dll.a).
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_MODULE_PREFIX  "")
