project(${CMAKE_PROJECT_NAME}_lib)

file(GLOB_RECURSE SRC_FILES
    *.cpp
    controllers/*.cpp
)

file(GLOB_RECURSE CONTROLLERS_FILES
    controllers/*.cpp
)

list(REMOVE_ITEM SRC_FILES ${CONTROLLERS_FILES})

add_library(${PROJECT_NAME}
    ${SRC_FILES}
)

set(DATABASE_PATH ${ABSOLUTE_DATA_PATH}/database.db)
get_filename_component(ABSOLUTE_DATABASE_PATH ${DATABASE_PATH} ABSOLUTE)
set(DOCUMENT_ROOT_PATH ${ABSOLUTE_DATA_PATH}/document_root)
get_filename_component(ABSOLUTE_DOCUMENT_ROOT_PATH ${DOCUMENT_ROOT_PATH} ABSOLUTE)

target_compile_definitions(${PROJECT_NAME} PUBLIC PROJECT_NAME="${CMAKE_PROJECT_NAME}")
target_compile_definitions(${PROJECT_NAME} PUBLIC DOMAIN_NAME="$<IF:$<CONFIG:Debug>,localhost,infirmary.damian-tomczak.pl>")
target_compile_definitions(${PROJECT_NAME} PUBLIC DATABASE_PATH="${ABSOLUTE_DATABASE_PATH}")
target_compile_definitions(${PROJECT_NAME} PRIVATE DOCUMENT_ROOT_PATH="${ABSOLUTE_DOCUMENT_ROOT_PATH}")
target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<BOOL:${SQLITECPP_ENABLE_ASSERT_HANDLER}>:SQLITECPP_ENABLE_ASSERT_HANDLER>)

if(UNIX)
    target_compile_options(${PROJECT_NAME} PUBLIC
        $<$<NOT:$<CONFIG:Debug>>:-DNDEBUG>
        $<IF:$<CONFIG:Debug>, -g, -O3>
        -Wall -Werror -Wno-unused-function -Wfatal-errors
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${ABSOLUTE_EXTERNAL_PATH}/fmt/include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    SQLiteCpp
    fmt::fmt
    drogon
    ssl
    mailio
    Boost::program_options
)
