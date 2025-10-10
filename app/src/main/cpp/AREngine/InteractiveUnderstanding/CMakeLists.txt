set(INC_PATH include)
set(SRC_PATH src)
set(LIB_PATH lib)
set(ASSET_PATH asset)
# 添加源文件
file(GLOB_RECURSE INCLUDES ${INC_PATH}/*.h ${INC_PATH}/*.hpp)
file(GLOB_RECURSE SOURCES ${SRC_PATH}/*.cpp)
# 设置库目录
set(SHARED_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib)


if(NOT USE_PREBUILT_LIBS)
        # 创建库
	set(LIB_TYPE PUBLIC)
	add_library(InteractiveUnderstanding ${SOURCES} ${INCLUDES})
else()
        set(LIB_TYPE INTERFACE)
        find_library(InteractiveUnderstanding_LIB NAMES InteractiveUnderstanding PATHS ${SHARED_LIB} NO_DEFAULT_PATH)
        if(InteractiveUnderstanding_LIB)
                add_library(InteractiveUnderstanding_LIB IMPORTED)
                set_target_properties(InteractiveUnderstanding_LIB PROPERTIES IMPORTED_LOCATION ${InteractiveUnderstanding_LIB})
        else()
                message(FATAL_ERROR "Prebuilt library InteractiveUnderstanding not found")
        endif()
endif()

# 设置库的包含目录
target_include_directories(InteractiveUnderstanding ${LIB_TYPE} ${INC_PATH})

target_link_libraries(InteractiveUnderstanding ${LIB_TYPE}
        ${GlobalLibs}
        Basic
        cadDataManager
)

# 用于向上层传递, link目录以供链接目录
set(InteractiveUnderstanding_LIB_PATH ${SHARED_LIB} PARENT_SCOPE)

# 导出动态链接库
#if(NOT USE_PREBUILT_LIBS)
#        # set library output
#        set(LIBRARY_OUTPUT_PATH ${SHARED_LIB})
#        file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
#        set_target_properties(InteractiveUnderstanding PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH})
#endif()

