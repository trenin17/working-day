include(UserverTestsuite)

set(CONFIG_VARS_PATH "${CMAKE_SOURCE_DIR}/configs/config_vars.testsuite.yaml")
message("CONFIG_VARS_PATH ${CONFIG_VARS_PATH}")
if (EXISTS "${CONFIG_VARS_PATH}")
    set(PYTEST_ARGS_CONFIG_VARS "--service-config-vars=${CONFIG_VARS_PATH}")
else()
    set(PYTEST_ARGS_CONFIG_VARS "")
endif()

message("PYTEST_ARGS_CONFIG_VARS ${PYTEST_ARGS_CONFIG_VARS}")

userver_testsuite_add(
  SERVICE_TARGET working_day
  REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PYTEST_ARGS
  --service-config=${CMAKE_SOURCE_DIR}/configs/static_config.yaml
  --service-binary=${CMAKE_BINARY_DIR}/working_day
  --config-fallback=${CMAKE_SOURCE_DIR}/configs/dynamic_config_fallback.json
  ${PYTEST_ARGS_CONFIG_VARS}
)
