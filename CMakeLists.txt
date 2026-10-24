# CMakeList.txt: проект CMake для FastBinary; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#
cmake_minimum_required (VERSION 3.10)


# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("FastBinary")

# Локальная установка Qt (Windows, MSVC 2022 x64).
# Если Qt установлен в другом месте, измените путь или задайте CMAKE_PREFIX_PATH/Qt6_DIR.
set(QT_ROOT "E:/Qt/6.12.0/msvc2022_64" CACHE PATH "Root directory of Qt installation")
if(EXISTS "${QT_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.12 REQUIRED COMPONENTS Widgets)

# Добавьте источник в исполняемый файл этого проекта.
add_executable (FastBinary
        "main.cpp"
        PE/pe.cpp
        PE/pe.h
        WINAPI/TYPES.h
        PE/Memory/memory.cpp
        PE/Memory/memory.h
)

set_property(TARGET FastBinary PROPERTY CXX_STANDARD 26)

# Максимальный уровень предупреждений, но без превращения их в ошибки.
target_compile_options(FastBinary PRIVATE /W4)
target_compile_options(FastBinary PRIVATE $<$<CONFIG:Debug>:/DEBUG /OPT:REF /OPT:ICF>)


# Корень проекта в путях включений, чтобы работали include вида "WINAPI/TYPES.h"
target_include_directories(FastBinary PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(FastBinary Qt6::Widgets)