cmake_minimum_required(VERSION 3.8)
project(RepoManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
add_executable(main main.cpp main_manifest.rc)
add_executable(logger logger.cpp)

# Default output directories
set_target_properties(main PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
set_target_properties(logger PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

if(MSVC)
	# 使用 UTF-8 源编码并启用 MSVC 异常处理（与 Visual Studio 默认一致）
	target_compile_options(main PRIVATE /utf-8 /EHsc)
	target_compile_options(logger PRIVATE /utf-8 /EHsc)

	# 仅在 Release 配置下启用 /O2（/O2 与 Debug 的 /RTC1 不兼容，无条件添加会导致 Debug 构建失败）
	target_compile_options(main PRIVATE $<$<CONFIG:Release>:/O2>)
	target_compile_options(logger PRIVATE $<$<CONFIG:Release>:/O2>)

	# 使用 DLL CRT（动态链接运行时库）以匹配 Visual Studio 的运行时环境并避免 side-by-side 清单问题
	# 可用值示例: MultiThreadedDLL, MultiThreadedDebugDLL 等
	set_property(TARGET main PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
	set_property(TARGET logger PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

	# main 的 manifest 已通过将 main_manifest.rc 添加到 add_executable 的源列表来嵌入
	# 在此处避免重复添加以免导致 CMake 重复配置或报错
else()
	# best-effort for non-MSVC toolchains on Windows (e.g., MinGW)
	target_compile_options(main PRIVATE -O2)
	target_compile_options(logger PRIVATE -O2)
endif()
