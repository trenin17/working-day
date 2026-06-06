"""
Unit-тесты HTTP-хендлеров nep_sign / nep_verify (без реального S3): моки download_file / upload_file.

Дополнение к чеклисту 3.6 отчёта (слой HTTP микросервиса):

- Успешное подписание через API: test_nep_sign_success
- Ошибки ввода / JSON / ключей / S3: test_nep_sign_missing_required_fields,
  test_nep_sign_invalid_json, test_nep_sign_load_keys_invalid_pem,
  test_nep_sign_download_fails, test_nep_sign_sign_pdf_raises, test_nep_sign_upload_fails
- Успешная проверка подписи через API: test_nep_verify_success
- Недействительная подпись (изменён PDF после подписи): test_nep_verify_returns_invalid_result
- Ошибки verify: test_nep_verify_missing_fields, test_nep_verify_invalid_json,
  test_nep_verify_pdf_download_fails, test_nep_verify_sig_download_fails,
  test_nep_verify_bad_public_key_pem, test_nep_verify_inner_exception_returns_500

Криптография и PDF разного размера — в python_service/tests/test_nep_signer.py (NEPSigner).
Генерация ключей (C++) — tests/test_nep_basic.py.
"""
import json
import shutil
from pathlib import Path
from unittest.mock import MagicMock, patch

import fitz
import pytest

from nep_signer.sign_handler import nep_sign_document
from nep_signer.verify_handler import nep_verify_document


def _rsa_pem_pair():
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.hazmat.primitives import serialization

    pk = rsa.generate_private_key(65537, 2048, default_backend())
    priv = pk.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.NoEncryption(),
    )
    pub = pk.public_key().public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    return priv.decode(), pub.decode()


def _minimal_pdf(path):
    doc = fitz.open()
    doc.new_page().insert_text((72, 72), 'h')
    doc.save(str(path))
    doc.close()


class MockJsonRequest:
    def __init__(self, payload=None, json_error=None):
        self._payload = payload
        self._json_error = json_error

    async def json(self):
        if self._json_error:
            raise self._json_error
        return self._payload


def _resp_json(resp):
    """Тело JSON-ответа aiohttp.web (body уже materialized у json_response)."""
    return json.loads(resp.body.decode())


@pytest.mark.asyncio
async def test_nep_sign_missing_required_fields():
    resp = await nep_sign_document(MockJsonRequest({}))
    assert resp.status == 400
    body = _resp_json(resp)
    assert body['error'] == 'Missing required fields'


@pytest.mark.asyncio
async def test_nep_sign_invalid_json():
    import json as json_lib

    resp = await nep_sign_document(
        MockJsonRequest(json_error=json_lib.JSONDecodeError('x', '', 0))
    )
    assert resp.status == 400
    body = _resp_json(resp)
    assert body['error'] == 'Invalid JSON'


@pytest.mark.asyncio
@patch('nep_signer.sign_handler.download_file')
async def test_nep_sign_download_fails(mock_dl):
    mock_dl.side_effect = RuntimeError('s3 down')
    priv, pub = _rsa_pem_pair()
    resp = await nep_sign_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'employee_id': 'e1',
                'employee_name': 'N',
                'private_key': priv,
                'public_key': pub,
            }
        )
    )
    assert resp.status == 502
    body = _resp_json(resp)
    assert 'download' in body['error'].lower()


@pytest.mark.asyncio
@patch('nep_signer.sign_handler.download_file')
async def test_nep_sign_load_keys_invalid_pem(mock_dl):
    mock_dl.side_effect = lambda *_a, **_k: None
    resp = await nep_sign_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'employee_id': 'e1',
                'employee_name': 'N',
                'private_key': 'not pem',
                'public_key': 'not pem',
            }
        )
    )
    assert resp.status == 400
    body = _resp_json(resp)
    assert 'keys' in body['error'].lower()


@pytest.mark.asyncio
@patch('nep_signer.sign_handler.upload_file')
@patch('nep_signer.sign_handler.download_file')
async def test_nep_sign_success(mock_dl, mock_up, tmp_path):
    pdf_src = tmp_path / 'src.pdf'
    _minimal_pdf(pdf_src)

    def copy_to_tmp(doc_id, dst):
        shutil.copy(pdf_src, dst)

    mock_dl.side_effect = copy_to_tmp
    priv, pub = _rsa_pem_pair()
    resp = await nep_sign_document(
        MockJsonRequest(
            {
                'document_id': 'doc-x',
                'employee_id': 'emp-y',
                'employee_name': 'Имя',
                'private_key': priv,
                'public_key': pub,
                'reason': 'тест',
                'location': 'офис',
            }
        )
    )
    assert resp.status == 200
    body = _resp_json(resp)
    assert body['signature_path'] == 'doc-x_emp-y_nep.p7s'
    assert body['user_name'] == 'Имя'
    assert body['reason'] == 'тест'
    assert body['location'] == 'офис'
    assert len(body['public_key_hash']) == 64
    mock_up.assert_called_once()


@pytest.mark.asyncio
@patch('nep_signer.sign_handler.upload_file')
@patch('nep_signer.sign_handler.download_file')
@patch('nep_signer.sign_handler.NEPSigner')
async def test_nep_sign_sign_pdf_raises(mock_nepsigner, mock_dl, mock_up, tmp_path):
    pdf_src = tmp_path / 'src.pdf'
    _minimal_pdf(pdf_src)
    mock_dl.side_effect = lambda doc_id, dst: shutil.copy(pdf_src, dst)
    inst = MagicMock()
    inst.sign_pdf.side_effect = OSError('disk full')
    mock_nepsigner.return_value = inst

    priv, pub = _rsa_pem_pair()
    resp = await nep_sign_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'employee_id': 'e1',
                'employee_name': 'N',
                'private_key': priv,
                'public_key': pub,
            }
        )
    )
    assert resp.status == 500
    body = _resp_json(resp)
    assert 'sign' in body['error'].lower()


@pytest.mark.asyncio
@patch('nep_signer.sign_handler.upload_file')
@patch('nep_signer.sign_handler.download_file')
async def test_nep_sign_upload_fails(mock_dl, mock_up, tmp_path):
    pdf_src = tmp_path / 'src.pdf'
    _minimal_pdf(pdf_src)
    mock_dl.side_effect = lambda *_a, **_k: shutil.copy(pdf_src, '/tmp/doc-x')
    mock_up.side_effect = RuntimeError('upload')

    priv, pub = _rsa_pem_pair()
    resp = await nep_sign_document(
        MockJsonRequest(
            {
                'document_id': 'doc-x',
                'employee_id': 'e1',
                'employee_name': 'N',
                'private_key': priv,
                'public_key': pub,
            }
        )
    )
    assert resp.status == 502
    body = _resp_json(resp)
    assert 'upload' in body['error'].lower()


# --- verify ---


@pytest.mark.asyncio
async def test_nep_verify_missing_fields():
    resp = await nep_verify_document(MockJsonRequest({}))
    assert resp.status == 400
    body = _resp_json(resp)
    assert body['error'] == 'Missing required fields'


@pytest.mark.asyncio
async def test_nep_verify_invalid_json():
    import json as json_lib

    resp = await nep_verify_document(
        MockJsonRequest(json_error=json_lib.JSONDecodeError('x', '', 0))
    )
    assert resp.status == 400


@pytest.mark.asyncio
@patch('nep_signer.verify_handler.download_file')
async def test_nep_verify_pdf_download_fails(mock_dl):
    mock_dl.side_effect = RuntimeError('fail')
    resp = await nep_verify_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'signature_path': 's.p7s',
                'public_key': _rsa_pem_pair()[1],
            }
        )
    )
    assert resp.status == 502


@pytest.mark.asyncio
@patch('nep_signer.verify_handler.download_file')
async def test_nep_verify_sig_download_fails(mock_dl):
    def dl(doc_or_sig, dst):
        if doc_or_sig == 'd1':
            Path(dst).write_bytes(b'pdf')
        else:
            raise RuntimeError('no sig')

    mock_dl.side_effect = dl

    resp = await nep_verify_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'signature_path': 'sig.p7s',
                'public_key': _rsa_pem_pair()[1],
            }
        )
    )
    assert resp.status == 502


@pytest.mark.asyncio
@patch('nep_signer.verify_handler.download_file')
async def test_nep_verify_bad_public_key_pem(mock_dl):
    mock_dl.side_effect = lambda *_a, **_k: None
    resp = await nep_verify_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'signature_path': 's.p7s',
                'public_key': 'not a key',
            }
        )
    )
    assert resp.status == 400
    body = _resp_json(resp)
    assert 'public key' in body['error'].lower()


@pytest.mark.asyncio
@patch('nep_signer.verify_handler.download_file')
async def test_nep_verify_success(mock_dl, tmp_path):
    from nep_signer.signer import NEPSigner

    pdf = tmp_path / 'f.pdf'
    _minimal_pdf(pdf)
    priv, pub = _rsa_pem_pair()
    s = NEPSigner('v')
    s.load_key_pair(priv.encode(), pub.encode())
    s.sign_pdf(str(pdf))
    p7s = Path(str(pdf) + '.p7s')
    sig_bytes = p7s.read_bytes()

    def dl(key, dst):
        if key == 'doc-v':
            shutil.copy(pdf, dst)
        else:
            Path(dst).write_bytes(sig_bytes)

    mock_dl.side_effect = dl

    resp = await nep_verify_document(
        MockJsonRequest(
            {
                'document_id': 'doc-v',
                'signature_path': 'any-sig.p7s',
                'public_key': pub,
            }
        )
    )
    assert resp.status == 200
    body = _resp_json(resp)
    assert body['valid'] is True


@pytest.mark.asyncio
@patch('nep_signer.verify_handler.NEPSigner.verify_pdf')
@patch('nep_signer.verify_handler.download_file')
async def test_nep_verify_inner_exception_returns_500(mock_dl, mock_verify, tmp_path):
    mock_dl.side_effect = lambda *_a, **_k: None
    mock_verify.side_effect = RuntimeError('unexpected')

    priv, pub = _rsa_pem_pair()
    resp = await nep_verify_document(
        MockJsonRequest(
            {
                'document_id': 'd1',
                'signature_path': 's.p7s',
                'public_key': pub,
            }
        )
    )
    assert resp.status == 500
    body = _resp_json(resp)
    assert 'verify' in body['error'].lower()


@pytest.mark.asyncio
@patch('nep_signer.verify_handler.download_file')
async def test_nep_verify_returns_invalid_result(mock_dl, tmp_path):
    pdf = tmp_path / 'f.pdf'
    _minimal_pdf(pdf)
    priv, pub = _rsa_pem_pair()
    from nep_signer.signer import NEPSigner

    s = NEPSigner('v')
    s.load_key_pair(priv.encode(), pub.encode())
    s.sign_pdf(str(pdf))
    b = bytearray(pdf.read_bytes())
    b[10] ^= 0xFF
    pdf.write_bytes(bytes(b))

    p7s_path = Path(str(pdf) + '.p7s')
    sig_bytes = p7s_path.read_bytes()

    def dl(key, dst):
        if key == 'doc-v2':
            shutil.copy(pdf, dst)
        else:
            Path(dst).write_bytes(sig_bytes)

    mock_dl.side_effect = dl

    resp = await nep_verify_document(
        MockJsonRequest(
            {
                'document_id': 'doc-v2',
                'signature_path': 'sig.p7s',
                'public_key': pub,
            }
        )
    )
    assert resp.status == 200
    body = _resp_json(resp)
    assert body['valid'] is False
