import pathlib

import json

import pytest

from testsuite.databases.pgsql import discover


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
def service_env() -> dict:
    SECDIST_CONFIG = {
        "postgresql_settings": {
            "databases": {
                "pg_working_day": [
                    {
                        "shard_number": 0,
                        "hosts": [
                            "host=rc1b-dk3v16aam2cveh01.mdb.yandexcloud.net port=6432 user=trenin17 password=trenin17 dbname=working_day_db_1"
                        ]
                    }
                ]
            }
        }
    }

    return {'SECDIST_CONFIG': json.dumps(SECDIST_CONFIG)}
