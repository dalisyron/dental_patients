cmake_minimum_required(VERSION 3.21)

# ---- Project ----------------------------------------------------------------
project(DentalPatients
    VERSION 1.0.0
    DESCRIPTION "Persian dental patient records manager"
    LANGUAGES CXX
)

set(APP_DISPLAY_NAME      "Dental Patients")
set(APP_DISPLAY_NAME_FA   "مدیریت بیماران دندانپزشکی")
set(APP_PUBLISHER         "Dental Patients")
set(APP_EXE_NAME          "DentalPatients")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Aggressive optimisation for the target machine (2 GB RAM, slow CPU).
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF>)
    add_link_options($<$<CONFIG:Release>:/OPT:ICF>)
endif()

# ---- Qt ---------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql Test)
qt_standard_project_setup()

# ---- Generated headers ------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# ---- Sources ----------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/core/PersianText.h
    src/core/PersianText.cpp
    src/db/Patient.h
    src/db/Database.h
    src/db/Database.cpp
    src/db/PatientRepository.h
    src/db/PatientRepository.cpp
    src/db/CsvImporter.h
    src/db/CsvImporter.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/AnchoredPlaceholderPlainTextEdit.h
    src/ui/AnchoredPlaceholderPlainTextEdit.cpp
    src/ui/PatientTableModel.h
    src/ui/PatientTableModel.cpp
    src/ui/PatientDialog.h
    src/ui/PatientDialog.cpp
    src/ui/TrashDialog.h
    src/ui/TrashDialog.cpp
    src/ui/AboutDialog.h
    src/ui/AboutDialog.cpp
)

# ---- Resources --------------------------------------------------------------
qt_add_resources(APP_RESOURCES_QRC resources/app.qrc)

# ---- Executable -------------------------------------------------------------
qt_add_executable(${APP_EXE_NAME} WIN32
    ${APP_SOURCES}
    ${APP_RESOURCES_QRC}
    assets/icons/app.rc
)

target_include_directories(${APP_EXE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(${APP_EXE_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

target_compile_definitions(${APP_EXE_NAME} PRIVATE
    DENTAL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    DENTAL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    DENTAL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    DENTAL_VERSION_STRING="${PROJECT_VERSION}"
)

set_target_properties(${APP_EXE_NAME} PROPERTIES
    OUTPUT_NAME "DentalPatients"
)

# ---- Bundle CSV seed data next to the binary in the install dir -------------
install(TARGETS ${APP_EXE_NAME} DESTINATION .)
install(FILES "${CMAKE_SOURCE_DIR}/patient_list_merged_sorted.csv"
        DESTINATION . OPTIONAL)

# ---- Tests ------------------------------------------------------------------
option(DENTAL_BUILD_TESTS "Build unit tests" ON)
if(DENTAL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- windeployqt for the installer ------------------------------------------
# Run this after build to populate dist/ with all required Qt DLLs.
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXE)
        add_custom_target(deploy
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "$<TARGET_FILE:${APP_EXE_NAME}>"
                    "${CMAKE_BINARY_DIR}/dist/"
            COMMAND ${CMAKE_COMMAND} -E copy
                    "${CMAKE_SOURCE_DIR}/patient_list_merged_sorted.csv"
                    "${CMAKE_BINARY_DIR}/dist/"
            COMMAND ${WINDEPLOYQT_EXE}
                    --release
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-quick-import
                    --compiler-runtime
                    "${CMAKE_BINARY_DIR}/dist/$<TARGET_FILE_NAME:${APP_EXE_NAME}>"
            DEPENDS ${APP_EXE_NAME}
            COMMENT "Running windeployqt -> ${CMAKE_BINARY_DIR}/dist"
        )
    endif()
endif()
