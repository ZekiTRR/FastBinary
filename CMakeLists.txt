cmake_minimum_required(VERSION 3.20)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(FastBinary LANGUAGES CXX)

# --- Qt (Windows, MSVC 2022 x64) ---
# Если Qt установлен в другом месте, измените путь или задайте CMAKE_PREFIX_PATH/Qt6_DIR.
set(QT_ROOT "E:/Qt/6.12.0/msvc2022_64" CACHE PATH "Root directory of Qt installation")
if(EXISTS "${QT_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)

# --- hash-library (stbrumme) через FetchContent ---
# У библиотеки нет собственного CMakeLists.txt, поэтому собираем её вручную.
include(FetchContent)

FetchContent_Declare(
    hash
    GIT_REPOSITORY https://github.com/stbrumme/hash-library.git
    GIT_TAG 70cc93763d49a24c4536c7f0a23248f9b40e30f5 # hash_library v8
)
FetchContent_MakeAvailable(hash)

add_library(hash_library STATIC
    ${hash_SOURCE_DIR}/md5.cpp
    ${hash_SOURCE_DIR}/md5.h
    ${hash_SOURCE_DIR}/sha1.cpp
    ${hash_SOURCE_DIR}/sha1.h
    ${hash_SOURCE_DIR}/sha256.cpp
    ${hash_SOURCE_DIR}/sha256.h
)
target_include_directories(hash_library PUBLIC ${hash_SOURCE_DIR})

# --- Capstone: только x86/x64, без утилит и тестов ---
set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARM_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARM64_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_M68K_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_MIPS_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_PPC_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_SPARC_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_SYSZ_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_XCORE_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_TMS320C64X_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_M680X_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_EVM_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_MOS65XX_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_WASM_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BPF_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_RISCV_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_SH_SUPPORT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_TRICORE_SUPPORT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    capstone
    GIT_REPOSITORY https://github.com/capstone-engine/capstone.git
    GIT_TAG 5.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(capstone)

# --- FastBinary ---
add_executable(FastBinary
    main.cpp
    PE/pe.cpp
    PE/pe.h
    PE/structures.cpp
    PE/structures.h
    PE/Memory/memory.cpp
    PE/Memory/memory.h
    PE/hash.h
    Disasm/disasm.cpp
    Disasm/disasm.h
    UI/MainWindow.cpp
    UI/MainWindow.h
    UI/HexEditor.cpp
    UI/HexEditor.h
    WINAPI/TYPES.h
)

set_property(TARGET FastBinary PROPERTY CXX_STANDARD 26)

# Корень проекта в путях включений, чтобы работали include вида "WINAPI/TYPES.h"
target_include_directories(FastBinary PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${capstone_SOURCE_DIR}/include
)

# Один вызов на все библиотеки, все с ключевым словом PRIVATE.
target_link_libraries(FastBinary PRIVATE
    Qt6::Widgets
    hash_library
    capstone
)

# Максимальный уровень предупреждений, но без превращения их в ошибки.
target_compile_options(FastBinary PRIVATE /W4)
target_compile_options(FastBinary PRIVATE $<$<CONFIG:Debug>:/DEBUG>)

# Сквозная проверка: парсер, Capstone, вкладки и HEX-редактор без ручного GUI.
add_executable(FastBinarySmoke
    tests/smoke.cpp
    PE/pe.cpp
    PE/pe.h
    PE/structures.cpp
    PE/structures.h
    PE/Memory/memory.cpp
    PE/Memory/memory.h
    PE/hash.h
    Disasm/disasm.cpp
    Disasm/disasm.h
    UI/MainWindow.cpp
    UI/MainWindow.h
    UI/HexEditor.cpp
    UI/HexEditor.h
    WINAPI/TYPES.h
)
set_property(TARGET FastBinarySmoke PROPERTY CXX_STANDARD 26)
target_include_directories(FastBinarySmoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${capstone_SOURCE_DIR}/include
)
target_link_libraries(FastBinarySmoke PRIVATE
    Qt6::Widgets
    hash_library
    capstone
)
target_compile_options(FastBinarySmoke PRIVATE /W4)

enable_testing()
add_test(NAME FastBinarySmoke COMMAND FastBinarySmoke $<TARGET_FILE:FastBinary>)
set_tests_properties(FastBinarySmoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${QT_ROOT}/bin;$ENV{PATH}"
)
