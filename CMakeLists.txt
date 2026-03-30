cmake_minimum_required(VERSION 3.12)
project(working_day CXX)

# Enable userver libraries that are needed in this project
set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)
set(USERVER_FEATURE_CHAOTIC ON CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 20)

include(GNUInstallDirs)

# JSON MOMENT
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# JSON MOMENT

add_subdirectory(third_party/userver)
userver_setup_environment()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
	src/views/v1/attendance/export_to_excel/view.cpp
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
	
	src/views/v1/messenger/create_chat/view.cpp
	src/views/v1/messenger/list_chats/view.cpp
	src/views/v1/messenger/recent_messages/view.cpp
	
	src/views/v1/tracker/projects/add/view.cpp
	src/views/v1/tracker/projects/list/view.cpp
	src/views/v1/tracker/projects/media/upload/view.cpp
	src/views/v1/tracker/projects/info/view.cpp
	src/views/v1/tracker/projects/edit/view.cpp
	src/views/v1/tracker/tasks/add/view.cpp
	src/views/v1/tracker/tasks/list/view.cpp
	src/views/v1/tracker/tasks/info/view.cpp
	src/views/v1/tracker/tasks/assigned_to_user/view.cpp
	src/views/v1/tracker/tasks/edit/view.cpp
	src/views/v1/tracker/tasks/media/upload/view.cpp
	src/views/v1/tracker/tasks/documents/send/view.cpp
	src/views/v1/tracker/tasks/documents/remove/view.cpp
	
	src/views/v1/documents/chain/update/view.cpp
	src/views/v1/documents/chain/add/view.cpp
	src/views/v1/documents/remove/view.cpp
	src/views/v1/documents/restore/view.cpp
	src/views/v1/documents/history/view.cpp
	src/views/v1/documents/generate_from_template/view.cpp
	
	src/views/v1/employee/permissions/list/view.cpp
	src/views/v1/employee/permissions/set/view.cpp

	src/views/v1/comments/add/view.cpp
	src/views/v1/comments/edit/view.cpp
	src/views/v1/comments/info/view.cpp
	src/views/v1/comments/remove/view.cpp

	src/analitics/request_capture_middleware.cpp
	src/analitics/frontend_websocket.cpp
	src/analitics/MLClient.cpp
	src/analitics/containers/lru_request_cache.cpp
	src/analitics/containers/request_data.cpp
	src/analitics/containers/request_collector.cpp

	src/ML/ml_process_manager.cpp
)
target_link_libraries(${PROJECT_NAME}_objs PUBLIC userver-postgresql)
# target_link_libraries(${PROJECT_NAME}_objs PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME}_objs PRIVATE ${AWSSDK_LINK_LIBRARIES}
${AWSSDK_PLATFORM_DEPS})
target_include_directories(${PROJECT_NAME}_objs PRIVATE src/)

# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)
target_link_libraries(${PROJECT_NAME}_objs 
    PUBLIC 
        nlohmann_json::nlohmann_json
)

target_include_directories(${PROJECT_NAME}_objs 
    SYSTEM PUBLIC 
        $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
)



# Unit Tests
add_executable(${PROJECT_NAME}_unittest
    src/hello_test.cpp
)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver-utest)
add_google_tests(${PROJECT_NAME}_unittest)

# ------------------------------------- ML MODULE -------------------------------------

set(ML_VENV_DIR "${CMAKE_BINARY_DIR}/.venv-ml")
set(ML_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/src/ML/main.py")
set(ML_REQUIREMENTS "${CMAKE_SOURCE_DIR}/src/ML/requirements.txt")

add_custom_target(setup-ml-venv ALL
    COMMAND ${CMAKE_COMMAND} -E echo "=== Setting up Python venv for ML service ==="
    COMMAND python3 -m venv "${ML_VENV_DIR}"
    COMMAND "${ML_VENV_DIR}/bin/pip" install --upgrade pip
    COMMAND "${ML_VENV_DIR}/bin/pip" install -r "${ML_REQUIREMENTS}"
    COMMENT "Creating Python virtual environment and installing ML dependencies"
    VERBATIM
)

add_dependencies(${PROJECT_NAME}_objs setup-ml-venv)
add_dependencies(${PROJECT_NAME} setup-ml-venv)

target_compile_definitions(${PROJECT_NAME}_objs PRIVATE
    CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
    CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    CMAKE_BINARY_DIR="${CMAKE_BINARY_DIR}"
    CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
# ------------------------------------- ML MODULE -------------------------------------

# Benchmarks
# add_executable(${PROJECT_NAME}_benchmark
# 	src/hello_benchmark.cpp
# )
# target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver-ubench)
# add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional Tests
#include(UserverTestsuite)

userver_testsuite_add_simple(
)

include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
	message(STATUS "Set install prefix: $ENV{PREFIX}")
	file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
	set(CMAKE_INSTALL_PREFIX ${PREFIX_PATH})
endif()

FILE(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${PROJECT_NAME})
install(FILES ${CONFIGS_FILES} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME} COMPONENT ${PROJECT_NAME})
