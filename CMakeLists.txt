cmake_minimum_required(VERSION 3.21)

project(PersonalDataManager VERSION 1.5.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Sql)

qt_standard_project_setup()

qt_add_executable(personal_data_manager WIN32
    src/main.cpp
    src/datastore.cpp
    src/datastore.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/trendchart.cpp
    src/trendchart.h
    src/worknoteeditor.cpp
    src/worknoteeditor.h
    resources/resources.qrc
)

if(WIN32)
    target_sources(personal_data_manager PRIVATE resources/app_icon.rc)
endif()

# Keep Ninja rebuilds correct even when MSVC emits localized /showIncludes output.
set_property(SOURCE src/main.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.h"
)
set_property(SOURCE src/mainwindow.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/mainwindow.h;${CMAKE_CURRENT_SOURCE_DIR}/src/datastore.h;${CMAKE_CURRENT_SOURCE_DIR}/src/trendchart.h"
)
set_property(SOURCE src/datastore.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/datastore.h"
)
set_property(SOURCE src/trendchart.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/trendchart.h"
)

target_link_libraries(personal_data_manager PRIVATE Qt6::Widgets Qt6::Sql)

# Keep Qt Creator/debug runs isolated from the installed application's data.
target_compile_definitions(personal_data_manager PRIVATE
    $<$<CONFIG:Debug>:NOTHING_DEBUG_BUILD>
)

if(MSVC)
    target_compile_options(personal_data_manager PRIVATE /utf-8)
endif()

set_target_properties(personal_data_manager PROPERTIES
    OUTPUT_NAME "Nothing"
)

install(TARGETS personal_data_manager
    RUNTIME DESTINATION .
)

install(FILES resources/app_icon.ico
    DESTINATION .
    RENAME "Nothing.ico"
)
