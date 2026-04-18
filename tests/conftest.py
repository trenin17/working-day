import hashlib
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

        components['handler-v1-documents-create-stamp-for-nep'][
            'pyservice-url'
        ] = mockserver_info.url('document/create-stamp-for-nep')

        components['handler-v1-documents-send'][
            'pyservice-url'
        ] = mockserver_info.url('document/convert')

        components['handler-v1-documents-chain-update'][
            'pyservice-url'
        ] = mockserver_info.url('document/create-stamp-for-nep')
        components['handler-v1-documents-chain-update'][
            'pyservice-nep-sign-url'
        ] = mockserver_info.url('document/nep-sign')

        components['handler-v1-documents-nep-sign'][
            'pyservice-url'
        ] = mockserver_info.url('document/nep-sign')

        components['handler-v1-documents-nep-verify'][
            'pyservice-url'
        ] = mockserver_info.url('document/nep-verify')

        components['handler-v1-attendance-export-to-excel'][
            'pyservice-url'
        ] = mockserver_info.url('attendance/export-to-excel')

        components['handler-v1-documents-generate-from-template'][
            'pyservice-url'
        ] = mockserver_info.url('documents/generate-from-template')

        components['handler-v1-tracker-tasks-documents-send'][
            'pyservice-url'
        ] = mockserver_info.url('tracker/tasks/documents/convert')

        components['handler-v1-documents-download-with-signatures'][
            'pyservice-url'
        ] = mockserver_info.url('document/build-archive')

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

    @mockserver.json_handler('/document/create-stamp-for-nep')
    def mock(request):
        return {
            'response': 'OK'
        }

    @mockserver.json_handler('/document/convert')
    def mock(request):
        return {
            'response': 'OK'
        }

    @mockserver.json_handler('/attendance/export-to-excel')
    def mock(request):
        return {
            'response': 'OK'
        }

    @mockserver.json_handler('/documents/generate-from-template')
    def mock(request):
        return {
            'response': 'OK'
        }

    @mockserver.json_handler('/tracker/tasks/documents/convert')
    def mock(request):
        return {
            'response': 'OK'
        }

    @mockserver.json_handler('/document/build-archive')
    def mock(request):
        return {'url': 's3 download test link'}

    @mockserver.json_handler('/document/nep-sign')
    def mock(request):
        data = request.json
        doc_id = data.get('document_id') or ''
        if doc_id == 'doc_nep_pyservice_sign_500':
            return mockserver.make_response(
                json={'error': 'mock upstream failure'},
                status=500,
            )
        pub = data.get('public_key') or ''
        ph = hashlib.sha256(pub.encode('utf-8')).hexdigest()
        safe_doc = doc_id or 'doc'
        return {
            'signature_path': f'{safe_doc}_mock_nep.p7s',
            'timestamp': '2026-03-25T12:00:00Z',
            'public_key_hash': ph,
        }

    @mockserver.json_handler('/document/nep-verify')
    def mock(request):
        data = request.json
        sp = data.get('signature_path') or ''
        if sp == '__MOCK_VERIFY_HTTP_502__':
            return mockserver.make_response(
                json={'error': 'mock verify upstream'},
                status=502,
            )
        if sp == '__MOCK_VERIFY_RETURN_INVALID__':
            return {
                'valid': False,
                'integrity_ok': False,
                'signature_ok': True,
                'message': 'mock: document modified',
            }
        return {
            'valid': True,
            'integrity_ok': True,
            'signature_ok': True,
            'message': 'Подпись действительна',
        }

    # /// [mockserver]
