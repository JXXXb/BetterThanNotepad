cmake_minimum_required(VERSION 3.15)
project(RepoManager LANGUAGES CXX)

# 1. 设置 C++ 标准（CMake 会自动传递给 MSVC）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 创建目标（必须在使用 target_compile_options 之前）
add_executable(main main.cpp main_manifest.rc)
add_executable(logger logger.cpp)

# 3. 设置输出目录
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
set_target_properties(logger PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# 4. 编译器专用配置
if(MSVC)
    # 强制启用 C++17/20 特性宏
    add_compile_definitions(
        _HAS_CXX17=1
        _HAS_CXX20=1
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    )
    
    # 基础编译选项（所有配置）
    target_compile_options(main PRIVATE
        /Zc:__cplusplus     # 让 __cplusplus 宏报告正确值
        /permissive-        # 严格标准一致性
        /utf-8              # UTF-8 编码
        /EHsc               # C++ 异常处理
    )
    target_compile_options(logger PRIVATE
        /Zc:__cplusplus
        /permissive-
        /utf-8
        /EHsc
    )
    
    # Release 优化（完整保留你的 /O2 和 /GL）
    target_compile_options(main PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
    )
    target_compile_options(logger PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
    )
    
    # 运行时库（动态链接 CRT）
    set_property(TARGET main PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    set_property(TARGET logger PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

else()
    # 非 MSVC 工具链（如 MinGW/GCC/Clang on Windows）
    # /O2 对应 -O2，/GL 对应 -flto（链接时优化）
    target_compile_options(main PRIVATE
        -std=c++23
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Release>:-flto>
    )
    target_compile_options(logger PRIVATE
        -std=c++23
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Release>:-flto>
    )
endif()