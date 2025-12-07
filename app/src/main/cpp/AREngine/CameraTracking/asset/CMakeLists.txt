# cmake_minimum_required(VERSION 3.10)

# include_directories(
#         ${OpenCV_INCLUDE_DIRS})

# link_directories(${OpenCV_LIBS})

# message(STATUS "[asset]")


cmake_minimum_required(VERSION 3.10)

# Find the specific version of OpenCV required for the submodule
# find_package(OpenCV 3 REQUIRED PATHS "/home/lab/lab-lib/opencv/3.3.1/opencv-3.3.1/build")
find_package(OpenCV 4.5 REQUIRED)

# Include the OpenCV directories for this submodule
include_directories(${OpenCV_INCLUDE_DIRS})

# Link the OpenCV libraries for this submodule
link_directories(${OpenCV_LIB_DIR})

message(STATUS "[SLAM asset] Using OpenCV version ${OpenCV_VERSION}")
