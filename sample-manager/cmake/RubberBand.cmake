set(RB "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/rubberband")
add_library(core_rubberband STATIC
    ${RB}/src/RubberBandStretcher.cpp ${RB}/src/RubberBandLiveShifter.cpp
    ${RB}/src/faster/AudioCurveCalculator.cpp ${RB}/src/faster/CompoundAudioCurve.cpp
    ${RB}/src/faster/HighFrequencyAudioCurve.cpp ${RB}/src/faster/SilentAudioCurve.cpp
    ${RB}/src/faster/PercussiveAudioCurve.cpp ${RB}/src/faster/R2Stretcher.cpp
    ${RB}/src/faster/StretcherChannelData.cpp ${RB}/src/faster/StretcherProcess.cpp
    ${RB}/src/common/Allocators.cpp ${RB}/src/common/FFT.cpp ${RB}/src/common/Log.cpp
    ${RB}/src/common/Profiler.cpp ${RB}/src/common/Resampler.cpp
    ${RB}/src/common/StretchCalculator.cpp ${RB}/src/common/sysutils.cpp
    ${RB}/src/common/mathmisc.cpp ${RB}/src/common/Thread.cpp ${RB}/src/common/BQResampler.cpp
    ${RB}/src/finer/R3Stretcher.cpp ${RB}/src/finer/R3LiveShifter.cpp)
target_include_directories(core_rubberband PUBLIC ${RB})
target_compile_definitions(core_rubberband PRIVATE USE_BUILTIN_FFT USE_BQRESAMPLER)
if(UNIX)
    target_compile_definitions(core_rubberband PRIVATE USE_PTHREADS)
endif()
if(MSVC)
    target_compile_definitions(core_rubberband PRIVATE NOMINMAX _USE_MATH_DEFINES)
endif()
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(sincos "math.h" CORE_HAS_SINCOS)
if(NOT CORE_HAS_SINCOS)
    target_compile_definitions(core_rubberband PRIVATE LACK_SINCOS)
endif()
find_package(Threads REQUIRED)
target_link_libraries(core_rubberband PUBLIC Threads::Threads)
