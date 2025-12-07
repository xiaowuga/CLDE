set(CMAKE_CXX_STANDARD 17)

set(INC_PATH include)
set(SRC_PATH src)

set(SHARED_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(SOURCES ${SRC_PATH}/MarkerLocation.cpp)

if(NOT USE_PREBUILT_LIBS)
    # 创建库
    set(LIB_TYPE PUBLIC)
    add_library(MarkerLocation SHARED ${SOURCES})
else()
    set(LIB_TYPE INTERFACE)
    find_library(MarkerLocation_LIB NAMES MarkerLocation PATHS ${SHARED_LIB} NO_DEFAULT_PATH)
    if(MarkerLocation_LIB)
        add_library(MarkerLocation SHARED IMPORTED)
        set_target_properties(MarkerLocation PROPERTIES IMPORTED_LOCATION ${ARInput_LIB})
    else()
        message(FATAL_ERROR "Prebuilt library Location not found")
    endif()
endif()


target_include_directories(MarkerLocation PUBLIC ${INC_PATH})

target_link_libraries(MarkerLocation PUBLIC  ${GlobalLibs} Basic ARInput)

set(MarkerLocation_LIB_PATH ${SHARED_LIB} PARENT_SCOPE)

if(NOT USE_PREBUILT_LIBS)
    # set library output
    set(LIBRARY_OUTPUT_PATH ${SHARED_LIB})
    file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
    set_target_properties(MarkerLocation PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH})
endif()
