find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/PersianText.cpp
    ${CMAKE_SOURCE_DIR}/src/db/Database.cpp
    ${CMAKE_SOURCE_DIR}/src/db/PatientRepository.cpp
    ${CMAKE_SOURCE_DIR}/src/db/CsvImporter.cpp
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
dp_add_test(tst_csv_importer   tst_csv_importer.cpp)
dp_add_test(tst_backup_restore tst_backup_restore.cpp)

# Make the seed CSV available next to test binaries.
configure_file(${CMAKE_SOURCE_DIR}/patient_list_merged_sorted.csv
               ${CMAKE_BINARY_DIR}/patient_list_merged_sorted.csv COPYONLY)
