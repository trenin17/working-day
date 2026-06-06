"""
Интеграционные тесты НЭП на C++-бэкенде (userver testsuite).

Рядом по смыслу с test_basic.py.

- POST /v1/employee/keys/generate
- POST /v1/documents/nep-sign, GET /v1/documents/nep-verify (моки pyservice в conftest.py;
  ветки pyservice: doc_id ``doc_nep_pyservice_sign_500``, пути подписи ``__MOCK_*__``)

Доп. SQL: ``nep_remove_first_signature_password.sql``, ``nep_verify_stranger_nep.sql``,
``nep_cpp_handlers_edge_cases.sql``.

Юнит-тесты NEPSigner — python_service/tests/.
"""
import hashlib

import pytest


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_employee_keys_generate_success(service_client):
    """third_id без ключей в initial_data: успешная генерация RSA, hash и has_nep."""
    r0 = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'third_id'},
        headers={'Authorization': 'Bearer third_token'},
    )
    assert r0.status == 200
    assert r0.json()['has_nep'] is False

    response = await service_client.post(
        '/v1/employee/keys/generate',
        headers={'Authorization': 'Bearer third_token'},
        params={'signature_password': '654321'},
    )
    assert response.status == 200
    body = response.json()
    pk = body['public_key']
    assert 'BEGIN PUBLIC KEY' in pk
    assert 'END PUBLIC KEY' in pk
    h = body['public_key_hash']
    assert len(h) == 64
    assert all(c in '0123456789abcdef' for c in h)
    assert hashlib.sha256(pk.encode('utf-8')).hexdigest() == h

    r1 = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'third_id'},
        headers={'Authorization': 'Bearer third_token'},
    )
    assert r1.status == 200
    assert r1.json()['has_nep'] is True

    again = await service_client.post(
        '/v1/employee/keys/generate',
        headers={'Authorization': 'Bearer third_token'},
        params={'signature_password': '111111'},
    )
    assert again.status == 400
    assert 'Keys already exist' in again.json()['message']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_employee_keys_generate_already_in_db(service_client):
    """first_id уже имеет ключи в initial_data."""
    response = await service_client.post(
        '/v1/employee/keys/generate',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_password': '123456'},
    )
    assert response.status == 400
    assert 'Keys already exist' in response.json()['message']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_employee_keys_generate_missing_password(service_client):
    response = await service_client.post(
        '/v1/employee/keys/generate',
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 400
    assert 'signature_password is required' in response.json()['message']


@pytest.mark.parametrize(
    'signature_password,fragment',
    [
        ('12345', 'exactly 6 digits'),
        ('1234567', 'exactly 6 digits'),
        ('12ab34', 'exactly 6 digits'),
    ],
)
@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_employee_keys_generate_invalid_password(
    service_client, signature_password, fragment
):
    response = await service_client.post(
        '/v1/employee/keys/generate',
        headers={'Authorization': 'Bearer second_token'},
        params={'signature_password': signature_password},
    )
    assert response.status == 400
    assert fragment in response.json()['message']


# --- C++: /v1/documents/nep-sign, /v1/documents/nep-verify (mock → document/nep-sign|nep-verify) ---


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_sign_success_and_verify(service_client):
    """first_id: ключи и пароль 123456 в initial_data; pyservice замокан в conftest."""
    sign = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer first_token'},
        params={
            'document_id': 'doc_with_chain',
            'signature_password': '123456',
        },
        json={'reason': 'test', 'location': 'lab'},
    )
    assert sign.status == 200
    body = sign.json()
    assert body['signature_path'] == 'doc_with_chain_mock_nep.p7s'
    assert body['timestamp'] == '2026-03-25T12:00:00Z'
    ph = body['public_key_hash']
    assert len(ph) == 64
    assert all(c in '0123456789abcdef' for c in ph)
    assert len(body['signature_id']) >= 32

    ver = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': body['signature_id']},
    )
    assert ver.status == 200
    v = ver.json()
    assert v['valid'] is True
    assert v['integrity_ok'] is True
    assert v['signature_ok'] is True
    assert v['signer_name'] == 'A First'
    assert 'verified_at' in v


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_verify_existing_nep_signature(service_client):
    """sig_2 в initial_data — тип nep; user_name в JSON метаданных — «First A»."""
    ver = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': 'sig_2'},
    )
    assert ver.status == 200
    v = ver.json()
    assert v['valid'] is True
    assert v['signer_name'] == 'First A'
    assert v['signature_timestamp'] == '2025-06-01T12:00:00Z'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_verify_not_found(service_client):
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': '00000000000000000000000000000000'},
    )
    assert r.status == 404
    assert 'not found' in r.json()['message'].lower()


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_verify_kep_rejected(service_client):
    """sig_1 — КЭП, обработчик НЭП отклоняет."""
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': 'sig_1'},
    )
    assert r.status == 400
    assert 'NEP' in r.json()['message']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_sign_wrong_password(service_client):
    r = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer first_token'},
        params={
            'document_id': 'doc_with_chain',
            'signature_password': '000000',
        },
        json={},
    )
    assert r.status == 401
    assert 'Wrong signature_password' in r.json()['message']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_sign_missing_password(service_client):
    r = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'doc_with_chain'},
        json={},
    )
    assert r.status == 400
    assert 'signature_password is required' in r.json()['message']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_sign_document_not_found(service_client):
    r = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer first_token'},
        params={
            'document_id': 'no_such_document',
            'signature_password': '123456',
        },
        json={},
    )
    assert r.status == 404
    assert 'Document not found' in r.json()['message']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_sign_no_keys(service_client):
    """second_id без employee_keys в initial_data."""
    r = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer second_token'},
        params={
            'document_id': 'doc_with_chain',
            'signature_password': '123456',
        },
        json={},
    )
    assert r.status == 400
    assert 'generate keys' in r.json()['message'].lower()


@pytest.mark.pgsql(
    'db_1',
    files=['initial_data.sql', 'nep_remove_first_signature_password.sql'],
)
async def test_nep_documents_nep_sign_signature_password_row_missing(service_client):
    """Ключи есть, строка в employee_signature_passwords удалена."""
    r = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer first_token'},
        params={
            'document_id': 'doc_with_chain',
            'signature_password': '123456',
        },
        json={},
    )
    assert r.status == 400
    assert 'Signature password not found' in r.json()['message']


@pytest.mark.pgsql(
    'db_1',
    files=['initial_data.sql', 'nep_cpp_handlers_edge_cases.sql'],
)
async def test_nep_documents_nep_sign_pyservice_returns_500(service_client):
    """Мок pyservice отвечает 500 — HTTP-клиент бэкенда пробрасывает ошибку."""
    r = await service_client.post(
        '/v1/documents/nep-sign',
        headers={'Authorization': 'Bearer first_token'},
        params={
            'document_id': 'doc_nep_pyservice_sign_500',
            'signature_password': '123456',
        },
        json={},
    )
    assert r.status == 500


@pytest.mark.pgsql(
    'db_1',
    files=['initial_data.sql', 'nep_verify_stranger_nep.sql'],
)
async def test_nep_documents_nep_verify_public_key_not_found(service_client):
    """nep-подпись от stranger_id, у которого нет employee_keys."""
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': 'sig_nep_stranger'},
    )
    assert r.status == 400
    assert 'Public key not found' in r.json()['message']


@pytest.mark.pgsql(
    'db_1',
    files=['initial_data.sql', 'nep_cpp_handlers_edge_cases.sql'],
)
async def test_nep_documents_nep_verify_pyservice_invalid_signature(service_client):
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': 'sig_nep_verify_mock_invalid'},
    )
    assert r.status == 200
    v = r.json()
    assert v['valid'] is False
    assert v['integrity_ok'] is False
    assert v['signature_ok'] is True
    assert 'mock' in v['message'].lower()
    assert v['signer_name'] == 'Mock User'
    assert v['signature_timestamp'] == '2024-01-01T00:00:00Z'


@pytest.mark.pgsql(
    'db_1',
    files=['initial_data.sql', 'nep_cpp_handlers_edge_cases.sql'],
)
async def test_nep_documents_nep_verify_pyservice_http_502(service_client):
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': 'sig_nep_verify_mock_http502'},
    )
    assert r.status == 502


@pytest.mark.pgsql(
    'db_1',
    files=['initial_data.sql', 'nep_cpp_handlers_edge_cases.sql'],
)
async def test_nep_documents_nep_verify_metadata_non_object_500(service_client):
    """Метаданные — JSON-массив: nlohmann .value(key) бросает type_error.306."""
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
        params={'signature_id': 'sig_nep_verify_bad_metadata'},
    )
    assert r.status == 500
    assert '306' in r.json().get('details', '') or 'array' in r.json().get('details', '').lower()


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_nep_documents_nep_verify_missing_signature_id(service_client):
    r = await service_client.get(
        '/v1/documents/nep-verify',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert r.status == 404
    assert 'not found' in r.json()['message'].lower()
