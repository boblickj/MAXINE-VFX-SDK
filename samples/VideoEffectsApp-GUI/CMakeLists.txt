set(SOURCE_FILES VideoEffectsAppGUI.cpp ../../nvvfx/src/nvVideoEffectsProxy.cpp ../../nvvfx/src/nvCVImageProxy.cpp)

# Set Visual Studio source filters
source_group("Source Files" FILES ${SOURCE_FILES})

add_executable(VideoEffectsAppGUI ${SOURCE_FILES})
target_include_directories(VideoEffectsAppGUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../utils)
target_include_directories(VideoEffectsAppGUI PUBLIC ${SDK_INCLUDES_PATH})

set(FLTK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/fltk)
set(FLTK_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/fltk-src)

if(NOT IS_DIRECTORY ${FLTK_DIR}) 
    message("Compiling FLTK library...")
    file(REMOVE_RECURSE ${FLTK_SRC_DIR})
    execute_process(COMMAND git clone https://github.com/fltk/fltk.git ${FLTK_SRC_DIR})
    execute_process(COMMAND cmake . -DCMAKE_INSTALL_PREFIX=${FLTK_DIR} WORKING_DIRECTORY ${FLTK_SRC_DIR})
    execute_process(COMMAND cmake --build . --target install --config Release WORKING_DIRECTORY ${FLTK_SRC_DIR})
    file(REMOVE_RECURSE ${FLTK_SRC_DIR})
endif()

set(FLTK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/fltk/CMake)
FIND_PACKAGE(FLTK REQUIRED NO_MODULE)

if(MSVC)
    target_link_libraries(VideoEffectsAppGUI PUBLIC
        opencv490
        NVVideoEffects
        ${CMAKE_CURRENT_SOURCE_DIR}/../external/cuda/lib/x64/cudart.lib
        fltk
        )

    set(OPENCV_PATH_STR ${CMAKE_CURRENT_SOURCE_DIR}/../external/opencv/bin)
    set(VFXSDK_PATH_STR ${CMAKE_CURRENT_SOURCE_DIR}/../../bin) # Also the location for CUDA/NVTRT/libcrypto
    set(PATH_STR "PATH=%PATH%" ${VFXSDK_PATH_STR} ${OPENCV_PATH_STR})
    set_target_properties(VideoEffectsAppGUI PROPERTIES
        FOLDER SampleApps
        VS_DEBUGGER_ENVIRONMENT "${PATH_STR}"
        )
else()

    target_link_libraries(VideoEffectsAppGUI PUBLIC
        NVVideoEffects
        NVCVImage
        OpenCV
        TensorRT
        CUDA
        fltk
        )
endif()
