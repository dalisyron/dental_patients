find_package(Qt6 6.5 REQUIRED COMPONENTS Test Widgets)

set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/PersianText.cpp
    ${CMAKE_SOURCE_DIR}/src/db/Database.cpp
    ${CMAKE_SOURCE_DIR}/src/db/PatientRepository.cpp
)

function(dp_add_test name source)
    qt_add_executable(${name} ${source} ${CORE_SOURCES})
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
    )
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Sql Qt6::Test Qt6::Gui)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

dp_add_test(tst_persian_text   tst_persian_text.cpp)
dp_add_test(tst_repository     tst_repository.cpp)
dp_add_test(tst_backup_restore tst_backup_restore.cpp)
dp_add_test(tst_migrations     tst_migrations.cpp)

qt_add_executable(tst_text_fields
    tst_text_fields.cpp
    ${CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/ui/AnchoredPlaceholderPlainTextEdit.h
    ${CMAKE_SOURCE_DIR}/src/ui/AnchoredPlaceholderPlainTextEdit.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PatientDialog.h
    ${CMAKE_SOURCE_DIR}/src/ui/PatientDialog.cpp
)
target_include_directories(tst_text_fields PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(tst_text_fields PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Test
    Qt6::Widgets
)
add_test(NAME tst_text_fields COMMAND tst_text_fields)
