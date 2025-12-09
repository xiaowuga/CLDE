set(INC_PATH include)
set(SRC_PATH src)
set(LIB_PATH lib)

set(SOURCES
    ${SRC_PATH}/HDRSwitch.cpp
)
set(SHARED_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib)

if(NOT USE_PREBUILT_LIBS)
    # 创建库
    set(LIB_TYPE PUBLIC)
    add_library(HDRSwitch SHARED ${SOURCES})
else()
    set(LIB_TYPE INTERFACE)
    find_library(HDRSwitch_LIB NAMES HDRSwitch PATHS ${SHARED_LIB} NO_DEFAULT_PATH)
    if(HDRSwitch_LIB)
        add_library(HDRSwitch SHARED IMPORTED)
        set_target_properties(HDRSwitch PROPERTIES IMPORTED_LOCATION ${HDRSwitch_LIB})
        
    else()
        message(FATAL_ERROR "Prebuilt library HDRSwitch not found")
    endif()
endif()

target_include_directories(HDRSwitch ${LIB_TYPE}
    ${INC_PATH}
)


target_link_libraries(HDRSwitch ${LIB_TYPE} ${GlobalLibs} Basic ARInput)

set(HDRSwitch_LIB_PATH ${SHARED_LIB} PARENT_SCOPE)

if(NOT USE_PREBUILT_LIBS)
    # set library output
    set(LIBRARY_OUTPUT_PATH ${SHARED_LIB})
    file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
    set_target_properties(HDRSwitch PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH})
endif()
