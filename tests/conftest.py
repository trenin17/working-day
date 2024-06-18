import pathlib

import json

import pytest

from testsuite import utils
from testsuite.databases.pgsql import discover


USERVER_CONFIG_HOOKS = ['userver_config_pyservice']

pytest_plugins = ['pytest_userver.plugins.postgresql']


@pytest.fixture(scope='session')
def service_source_dir():
    """Path to root directory service."""
    return pathlib.Path(__file__).parent.parent


@pytest.fixture(scope='session')
def initial_data_path(service_source_dir):
    """Path for find files with data"""
    return [
        service_source_dir / 'postgresql/data',
    ]


@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    """Create schemas databases for tests"""
    databases = discover.find_schemas(
        'working_day',  # service name that goes to the DB connection
        [service_source_dir.joinpath('postgresql/schemas')],
    )
    return pgsql_local_create(list(databases.values()))


@pytest.fixture(scope='session')
def userver_config_pyservice(mockserver_info):
    def do_patch(config_yaml, config_vars):
        components = config_yaml['components_manager']['components']

        components['handler-v1-abscence-verdict'][
            'pyservice-url'
        ] = mockserver_info.url('document/generate')

        components['handler-v1-documents-sign'][
            'pyservice-url'
        ] = mockserver_info.url('document/sign')

    return do_patch
    # /// [patch configs]


# /// [mockserver]
@pytest.fixture(autouse=True)
def mock_pyservice(mockserver) -> None:
    @mockserver.json_handler('/document/generate')
    def mock(request):
        return {
            'response': 'OK'
        }

    @mockserver.json_handler('/document/sign')
    def mock(request):
        return {
            'response': 'OK'
        }
    # /// [mockserver]
