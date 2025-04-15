# CMakeList.txt : CMake project for FileUnpacker, include source and define
# project specific logic here.
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source to this project's executable.
add_executable (FileUnpacker "FileUnpacker.cpp" "FileUnpacker.h" "headers/FileFormats.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET FileUnpacker PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/RES 
              ${CMAKE_CURRENT_BINARY_DIR}/RES COPYONLY)
