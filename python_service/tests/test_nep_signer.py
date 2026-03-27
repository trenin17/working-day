"""
Unit-тесты для nep_signer.signer.NEPSigner (без HTTP/S3/БД).
Покрывают основные ветки sign_pdf / verify_pdf и сообщения результата.

Соответствие разделу 3.6 отчёта (функциональное тестирование НЭП):

1) Генерация ключей для тестовых пользователей
   — только интеграция C++: tests/test_nep_basic.py (POST /v1/employee/keys/generate).
   Здесь _rsa_key_pair_pem() — вспомогательная загрузка ключей в NEPSigner для тестов
   подписи/верификации, не дублирует продовую генерацию (OpenSSL в бэкенде).

2) Подписание PDF различного размера
   — test_kt1_sign_verify_various_pdf_sizes

3) Проверка действительных подписей
   — test_sign_verify_roundtrip, test_load_key_pair_public_key_inferred_from_private,
     test_sign_verify_custom_signature_path

4) Обнаружение модификации документа
   — test_verify_tampered_pdf_fails, test_message_integrity_fail_signature_ok,
     test_message_both_integrity_and_signature_fail

5) Неверные / повреждённые подписи
   — test_verify_garbage_p7s_returns_invalid, test_verify_wrong_pkcs7_content_type,
     test_verify_tampered_p7s_fails, test_verify_with_mismatched_public_key_fails,
     test_message_signature_fail_integrity_ok, test_verify_no_certificates_in_signed_data
     (порча RSA-октетов в CMS — см. _flip_signature_octets_in_cms)

6) Граничные случаи и ошибки
   — test_sign_without_loaded_keys_raises, test_verify_missing_pdf_raises,
     test_verify_missing_p7s, test_verify_pdf_without_public_key_returns_error_dict,
     test_verify_signature_validation_error_mapped, и др.

Производительность (дымовой порог, не бенчмарк): test_kt1_sign_verify_performance_smoke.
"""
from pathlib import Path
from unittest.mock import patch

import pytest
import fitz
import asn1crypto.cms as asn1cms
import asn1crypto.algos as asn1algos
import asn1crypto.core as asn1core
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from pyhanko.sign.validation.errors import SignatureValidationError

from nep_signer.signer import NEPSigner


def _rsa_key_pair_pem():
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=2048,
        backend=default_backend(),
    )
    priv_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    pub_pem = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    return priv_pem, pub_pem


def _write_minimal_pdf(path):
    doc = fitz.open()
    page = doc.new_page()
    page.insert_text((72, 72), 'NEP unit test')
    doc.save(str(path))
    doc.close()


def _write_pdf_approx_size(path, min_bytes: int, max_pages: int = 200) -> int:
    """
    PDF не меньше min_bytes (по размеру файла после сохранения).
    Возвращает фактический размер файла.
    """
    path = Path(path)
    doc = fitz.open()
    line = 'Lorem ipsum dolor sit amet, consectetur adipiscing elit. ' * 4
    try:
        while doc.page_count < max_pages:
            page = doc.new_page()
            y = 50
            while y < 760:
                page.insert_text((50, y), line)
                y += 14
            doc.save(str(path))
            size = path.stat().st_size
            if size >= min_bytes:
                return size
        return path.stat().st_size
    finally:
        doc.close()


def _cms_data_content_info_bytes():
    """PKCS#7 не SignedData — ветка «Неверный тип PKCS#7 структуры»."""
    return asn1cms.ContentInfo({
        'content_type': '1.2.840.113549.1.7.1',
        'content': asn1core.OctetString(b'x'),
    }).dump()


def _cms_empty_signer_infos_bytes():
    """SignedData без signer_infos — ошибка до проверки сертификата."""
    signed_data = asn1cms.SignedData({
        'version': 'v1',
        'digest_algorithms': asn1cms.DigestAlgorithms([
            asn1algos.DigestAlgorithm({'algorithm': '2.16.840.1.101.3.4.2.1'}),
        ]),
        'encap_content_info': asn1cms.ContentInfo({
            'content_type': '1.2.840.113549.1.7.1',
            'content': None,
        }),
        'signer_infos': asn1cms.SignerInfos([]),
    })
    return asn1cms.ContentInfo({
        'content_type': '1.2.840.113549.1.7.2',
        'content': signed_data,
    }).dump()


def _strip_certificates_from_valid_p7s(p7s_path):
    """Убирает сертификаты из валидной подписи (ветка «Сертификат не найден»)."""
    ci = asn1cms.ContentInfo.load(p7s_path.read_bytes())
    sd = ci['content']
    sd['certificates'] = None
    return asn1cms.ContentInfo({
        'content_type': ci['content_type'],
        'content': sd,
    }).dump()


def _flip_signature_octets_in_cms(p7s_path):
    """Портит только RSA-подпись в SignerInfo; digest в signed_attrs и PDF без изменений."""
    ci = asn1cms.ContentInfo.load(p7s_path.read_bytes())
    sd = ci['content']
    si = sd['signer_infos'][0]
    raw = si['signature'].contents
    si['signature'] = asn1core.OctetString(bytes(b ^ 0xFF for b in raw))
    return asn1cms.ContentInfo({
        'content_type': ci['content_type'],
        'content': sd,
    }).dump()


# --- sign_pdf: ошибки и метаданные ---


def test_sign_without_loaded_keys_raises():
    signer = NEPSigner('User')
    with pytest.raises(ValueError, match='Ключи не инициализированы'):
        signer.sign_pdf('/nonexistent/path.pdf')


def test_sign_missing_input_file_raises(tmp_path):
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('User')
    signer.load_key_pair(priv_pem)
    missing = tmp_path / 'nope.pdf'
    with pytest.raises(FileNotFoundError, match='Файл не найден'):
        signer.sign_pdf(str(missing))


def test_init_user_id_defaults_to_user_name():
    s = NEPSigner('OnlyName')
    assert s.user_id == 'OnlyName'
    s2 = NEPSigner('N', user_id='explicit')
    assert s2.user_id == 'explicit'


def test_sign_metadata_optional_reason_location_none(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Meta')
    signer.load_key_pair(priv_pem)
    meta = signer.sign_pdf(str(pdf))
    assert meta['reason'] is None
    assert meta['location'] is None


def test_load_key_pair_invalid_pem_raises():
    signer = NEPSigner('Bad')
    with pytest.raises(ValueError):
        signer.load_key_pair(b'not valid pem')


# --- sign + verify: успех ---


def test_sign_verify_roundtrip(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)

    priv_pem, pub_pem = _rsa_key_pair_pem()
    signer = NEPSigner('Alice', user_id='u1')
    signer.load_key_pair(priv_pem, pub_pem)

    meta = signer.sign_pdf(str(pdf), reason='Согласование', location='Москва')
    assert meta['detached'] is True
    assert meta['user_name'] == 'Alice'
    assert meta['reason'] == 'Согласование'
    assert meta['location'] == 'Москва'
    assert len(meta['public_key_hash']) == 64

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is True
    assert result['integrity_ok'] is True
    assert result['signature_ok'] is True
    assert result['message'] == 'Подпись действительна'


def test_load_key_pair_public_key_inferred_from_private(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)

    priv_pem, pub_pem = _rsa_key_pair_pem()
    signer = NEPSigner('Dave')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))

    assert signer.public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    ) == pub_pem

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is True


def test_sign_verify_custom_signature_path(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    sig = tmp_path / 'custom.p7s'
    _write_minimal_pdf(pdf)

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Frank')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf), signature_path=str(sig))

    assert sig.is_file()
    result = signer.verify_pdf(str(pdf), signature_path=str(sig))
    assert result['valid'] is True
    assert result['message'] == 'Подпись действительна'


# --- verify_pdf: файлы ---


def test_verify_missing_pdf_raises(tmp_path):
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Eve')
    signer.load_key_pair(priv_pem)
    missing = tmp_path / 'absent.pdf'
    with pytest.raises(FileNotFoundError, match='Файл не найден'):
        signer.verify_pdf(str(missing))


def test_verify_missing_p7s(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Carol')
    signer.load_key_pair(priv_pem)

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert result['integrity_ok'] is False
    assert result['signature_ok'] is False
    assert 'не найден' in result['message'].lower()


def test_verify_pdf_without_public_key_returns_error_dict(tmp_path):
    """Без public_key сравнение с сертификатом падает; исключение ловится в verify_pdf."""
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('NoKey')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))
    signer.public_key = None
    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert 'Неожиданная ошибка' in result['message']
    assert 'NoneType' in result['message']


# --- Неверный / битый PKCS#7 ---


def test_verify_garbage_p7s_returns_invalid(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    p7s = tmp_path / 'doc.pdf.p7s'
    _write_minimal_pdf(pdf)
    p7s.write_bytes(b'not a cms blob')

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Grace')
    signer.load_key_pair(priv_pem)

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert result['integrity_ok'] is False
    assert result['signature_ok'] is False


def test_verify_wrong_pkcs7_content_type(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    p7s = tmp_path / 'doc.pdf.p7s'
    _write_minimal_pdf(pdf)
    p7s.write_bytes(_cms_data_content_info_bytes())

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('WrongOid')
    signer.load_key_pair(priv_pem)

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert 'Неожиданная ошибка' in result['message']
    assert 'Неверный тип PKCS#7' in result['message']


def test_verify_empty_signer_infos(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    p7s = tmp_path / 'doc.pdf.p7s'
    _write_minimal_pdf(pdf)
    p7s.write_bytes(_cms_empty_signer_infos_bytes())

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('EmptySigners')
    signer.load_key_pair(priv_pem)

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert 'Неожиданная ошибка' in result['message']
    assert 'не содержит информации о подписанте' in result['message']


def test_verify_no_certificates_in_signed_data(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('NoCert')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))
    p7s = tmp_path / 'doc.pdf.p7s'
    p7s.write_bytes(_strip_certificates_from_valid_p7s(p7s))

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert 'Сертификат не найден' in result['message']


def test_verify_tampered_p7s_fails(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Henry')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))

    p7s = tmp_path / 'doc.pdf.p7s'
    buf = bytearray(p7s.read_bytes())
    buf[-1] ^= 0xFF
    p7s.write_bytes(bytes(buf))

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False


# --- Ключи и целостность документа ---


def test_verify_with_mismatched_public_key_fails(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)

    priv_a, _ = _rsa_key_pair_pem()
    signer_sign = NEPSigner('Signer')
    signer_sign.load_key_pair(priv_a)
    signer_sign.sign_pdf(str(pdf))

    priv_b, pub_b = _rsa_key_pair_pem()
    signer_wrong = NEPSigner('Wrong')
    signer_wrong.load_key_pair(priv_b, pub_b)

    result = signer_wrong.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert 'Неожиданная ошибка' in result['message']


def test_verify_tampered_pdf_fails(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Bob')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))

    data = pdf.read_bytes()
    b = bytearray(data)
    b[len(b) // 2] ^= 0xFF
    pdf.write_bytes(bytes(b))

    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert result['integrity_ok'] is False


def test_message_both_integrity_and_signature_fail(tmp_path):
    """Порча PDF и RSA-подписи: integrity_ok и signature_ok оба false."""
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Both')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))
    b = bytearray(pdf.read_bytes())
    b[len(b) // 2] ^= 0xFF
    pdf.write_bytes(bytes(b))
    p7s = tmp_path / 'doc.pdf.p7s'
    p7s.write_bytes(_flip_signature_octets_in_cms(p7s))
    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert result['integrity_ok'] is False
    assert result['signature_ok'] is False
    assert result['message'] == (
        'Подпись недействительна: криптографическая проверка не пройдена и документ был изменен'
    )


def test_message_integrity_fail_signature_ok(tmp_path):
    """Документ изменён; подпись по signed_attrs криптографически верна."""
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Iris')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))
    b = bytearray(pdf.read_bytes())
    b[len(b) // 3] ^= 0x55
    pdf.write_bytes(bytes(b))
    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert result['integrity_ok'] is False
    assert result['signature_ok'] is True
    assert result['message'] == (
        'Подпись недействительна: документ был изменен после подписания'
    )


def test_message_signature_fail_integrity_ok(tmp_path):
    """PDF не трогали; испортили только RSA-подпись в CMS."""
    pdf = tmp_path / 'doc.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Jack')
    signer.load_key_pair(priv_pem)
    signer.sign_pdf(str(pdf))
    p7s = tmp_path / 'doc.pdf.p7s'
    p7s.write_bytes(_flip_signature_octets_in_cms(p7s))
    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is False
    assert result['integrity_ok'] is True
    assert result['signature_ok'] is False
    assert result['message'] == (
        'Подпись недействительна: криптографическая проверка не пройдена'
    )


# --- Исключения из парсера / pyhanko ---


def test_verify_signature_validation_error_mapped(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    p7s = tmp_path / 'doc.pdf.p7s'
    _write_minimal_pdf(pdf)
    p7s.write_bytes(b'placeholder')

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('SVE')
    signer.load_key_pair(priv_pem)

    with patch(
        'nep_signer.signer.asn1crypto.cms.ContentInfo.load',
        side_effect=SignatureValidationError('fail'),
    ):
        result = signer.verify_pdf(str(pdf))

    assert result['valid'] is False
    assert result['message'].startswith('Ошибка проверки подписи:')


def test_verify_hex_string_error_document_corrupted(tmp_path):
    pdf = tmp_path / 'doc.pdf'
    p7s = tmp_path / 'doc.pdf.p7s'
    _write_minimal_pdf(pdf)
    p7s.write_bytes(b'x')

    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Hex')
    signer.load_key_pair(priv_pem)

    with patch(
        'nep_signer.signer.asn1crypto.cms.ContentInfo.load',
        side_effect=ValueError('bad hex string in input'),
    ):
        result = signer.verify_pdf(str(pdf))

    assert result['valid'] is False
    assert result.get('document_corrupted') is True
    assert 'поврежден' in result['message'].lower()


def test_verify_unexpected_token_document_corrupted(tmp_path):
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Tok')
    signer.load_key_pair(priv_pem)
    pdf = tmp_path / 'd.pdf'
    _write_minimal_pdf(pdf)
    (tmp_path / 'd.pdf.p7s').write_bytes(b'z')

    with patch(
        'nep_signer.signer.asn1crypto.cms.ContentInfo.load',
        side_effect=ValueError('unexpected token at line 1'),
    ):
        result = signer.verify_pdf(str(pdf))

    assert result.get('document_corrupted') is True


# --- Чеклист 3.6 (отчёт этап 1): PDF-размер, перф — генерация ключей см. tests/test_nep_basic.py ---


@pytest.mark.parametrize(
    'min_size',
    [2_000, 50_000, 200_000],
    ids=['~2KiB', '~50KiB', '~200KiB'],
)
def test_kt1_sign_verify_various_pdf_sizes(tmp_path, min_size):
    """Подпись и проверка PDF, нарастающего размера файла (не один минимальный документ)."""
    pdf = tmp_path / f'sized_{min_size}.pdf'
    actual = _write_pdf_approx_size(pdf, min_size)
    if actual < min_size:
        pytest.skip(f'не удалось набрать PDF >= {min_size} B (получилось {actual} B)')

    priv_pem, pub_pem = _rsa_key_pair_pem()
    signer = NEPSigner('SizedUser')
    signer.load_key_pair(priv_pem, pub_pem)
    signer.sign_pdf(str(pdf))
    result = signer.verify_pdf(str(pdf))
    assert result['valid'] is True
    assert result['integrity_ok'] is True
    assert result['signature_ok'] is True


def test_kt1_sign_verify_performance_smoke(tmp_path):
    """Дымовой порог: один цикл sign+verify на маленьком PDF не «зависает» (не нагрузочный тест)."""
    import time

    pdf = tmp_path / 'perf.pdf'
    _write_minimal_pdf(pdf)
    priv_pem, _ = _rsa_key_pair_pem()
    signer = NEPSigner('Perf')
    signer.load_key_pair(priv_pem)

    t0 = time.perf_counter()
    signer.sign_pdf(str(pdf))
    signer.verify_pdf(str(pdf))
    elapsed = time.perf_counter() - t0

    assert elapsed < 120.0
