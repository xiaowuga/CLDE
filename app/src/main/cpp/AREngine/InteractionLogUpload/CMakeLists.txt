set(CMAKE_CXX_STANDARD 17)

set(INC_PATH include)
set(SRC_PATH src)

set(SHARED_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib)

set(SOURCES ${SRC_PATH}/InteractionLogUpload.cpp)

if(NOT USE_PREBUILT_LIBS)
    # 创建库
    set(LIB_TYPE PUBLIC)
    add_library(InteractionLogUpload SHARED ${SOURCES})
else()
    set(LIB_TYPE INTERFACE)
    find_library(InteractionLogUpload_LIB NAMES InteractionLogUpload PATHS ${SHARED_LIB} NO_DEFAULT_PATH)
    if(InterationLogUpload_LIB)
        add_library(InteractionLogUpload SHARED IMPORTED)
        set_target_properties(InteractionLogUpload PROPERTIES IMPORTED_LOCATION ${ARInput_LIB})
    else()
        message(FATAL_ERROR "Prebuilt library Location not found")
    endif()
endif()


target_include_directories(InteractionLogUpload PUBLIC ${INC_PATH})

target_link_libraries(InteractionLogUpload PUBLIC  ${GlobalLibs} Basic ARInput)

set(InteractionLogUpload_LIB_PATH ${SHARED_LIB} PARENT_SCOPE)

if(NOT USE_PREBUILT_LIBS)
    # set library output
    set(LIBRARY_OUTPUT_PATH ${SHARED_LIB})
    file(MAKE_DIRECTORY ${LIBRARY_OUTPUT_PATH})
    set_target_properties(InteractionLogUpload PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH})
endif()
