cmake_minimum_required(VERSION 3.12)
project(working_day CXX)

# Enable userver libraries that are needed in this project
set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)

include(GNUInstallDirs)
include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_subdirectory(third_party/userver)
userver_setup_environment()


set(CMAKE_EXPORT_COMPILE_COMMANDS, 1)
if(NOT BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
endif()


find_package(AWSSDK REQUIRED COMPONENTS s3 sts)

# Common sources
add_library(${PROJECT_NAME}_objs OBJECT
    src/hello.cpp
	src/views/v1/employee/add/view.cpp
	src/views/v1/employee/add_head/view.cpp
	src/views/v1/employee/info/view.cpp
	src/views/v1/employee/remove/view.cpp
	src/views/v1/employees/view.cpp
	src/views/v1/profile/upload_photo/view.cpp
	src/views/v1/profile/edit/view.cpp
	src/utils/s3_presigned_links.cpp
	src/auth/auth_bearer.cpp
	src/views/v1/authorize/view.cpp
	src/views/v1/abscence/request/view.cpp
	src/views/v1/abscence/verdict/view.cpp
	src/views/v1/notifications/view.cpp
	src/views/v1/actions/view.cpp
	src/views/v1/documents/vacation/view.cpp
	src/views/v1/attendance/add/view.cpp
	src/views/v1/abscence/split/view.cpp
	src/views/v1/abscence/reschedule/view.cpp
	src/views/v1/payments/add_bulk/view.cpp
	src/views/v1/payments/view.cpp
	src/core/json_compatible/struct.cpp
	src/utils/custom_implicit_options.cpp
	src/core/reverse_index/view.cpp
	src/views/v1/clear-tasks/view.cpp
	src/views/v1/search/basic/view.cpp
	src/views/v1/attendance/list_all/view.cpp
	src/views/v1/documents/upload/view.cpp
	src/views/v1/documents/send/view.cpp
	src/views/v1/documents/list/view.cpp
	src/views/v1/documents/download/view.cpp
	src/views/v1/search/full/view.cpp
	src/views/v1/search/suggest/view.cpp
	src/views/v1/documents/sign/view.cpp
	src/views/v1/documents/list_all/view.cpp
	src/views/v1/documents/get_signs/view.cpp
	src/views/v1/superuser/company/add/view.cpp
	src/views/v1/inventory/add/view.cpp
)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver-postgresql)
target_link_libraries(${PROJECT_NAME}_objs PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}_objs PRIVATE ${AWSSDK_LINK_LIBRARIES}
${AWSSDK_PLATFORM_DEPS})
target_include_directories(${PROJECT_NAME}_objs PRIVATE src/)


# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)


# Unit Tests
add_executable(${PROJECT_NAME}_unittest
    src/hello_test.cpp
)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver-utest)
add_google_tests(${PROJECT_NAME}_unittest)


# Benchmarks
# add_executable(${PROJECT_NAME}_benchmark
# 	src/hello_benchmark.cpp
# )
# target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver-ubench)
# add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional Tests
include(UserverTestsuite)

userver_testsuite_add_simple()

include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
	message(STATUS "Set install prefix: $ENV{PREFIX}")
	file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
	set(CMAKE_INSTALL_PREFIX ${PREFIX_PATH})
endif()

FILE(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PROJECT_NAME})
install(FILES ${CONFIGS_FILES} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME} COMPONENT ${PROJECT_NAME})
