"""
Модуль для работы с усиленной неквалифицированной электронной подписью (НЭП)
в PDF документах на основе pyHanko.

Соответствует требованиям ФЗ-63 и ст. 22.3 ТК РФ для внутреннего документооборота.
"""
import tempfile
import os
import hashlib
import traceback
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional, Dict, Any
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.backends import default_backend
from pyhanko.sign.validation.errors import SignatureValidationError

import asn1crypto.cms
import asn1crypto.algos
import asn1crypto.core


class NEPSigner:
    """
    Класс для создания и проверки усиленной неквалифицированной электронной подписи
    в PDF документах.

    Использует RSA-2048 для криптографического преобразования.
    Не требует УЦ, корневых сертификатов и CryptoPro.
    """

    def __init__(self, user_name: str, user_id: Optional[str] = None):
        """
        Инициализация подписанта.

        Args:
            user_name: Имя пользователя (для идентификации)
            user_id: Опциональный ID пользователя в системе
        """
        self.user_name = user_name
        self.user_id = user_id or user_name

        self.private_key: Optional[rsa.RSAPrivateKey] = None
        self.public_key: Optional[rsa.RSAPublicKey] = None

    def load_key_pair(self, private_key_pem: bytes, public_key_pem: Optional[bytes] = None):
        """
        Загрузка существующей пары ключей.

        Args:
            private_key_pem: Приватный ключ в формате PEM
            public_key_pem: Опциональный публичный ключ (если не указан, извлекается из приватного)
        """
        self.private_key = serialization.load_pem_private_key(
            private_key_pem,
            password=None,
            backend=default_backend()
        )

        if public_key_pem:
            self.public_key = serialization.load_pem_public_key(
                public_key_pem,
                backend=default_backend()
            )
        else:
            self.public_key = self.private_key.public_key()

    def sign_pdf(
        self,
        input_pdf_path: str,
        signature_path: Optional[str] = None,
        reason: Optional[str] = None,
        location: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Подписание PDF документа НЭП с отсоединенной подписью.

        Args:
            input_pdf_path: Путь к исходному PDF файлу
            signature_path: Путь для сохранения файла отсоединенной подписи (если None, используется input_pdf_path + .p7s)
            reason: Причина подписания (опционально)
            location: Место подписания (опционально)

        Returns:
            Словарь с метаданными о подписи
        """
        if not self.private_key:
            raise ValueError("Ключи не инициализированы. Вызовите load_key_pair()")

        input_path = Path(input_pdf_path)

        if not input_path.exists():
            raise FileNotFoundError(f"Файл не найден: {input_pdf_path}")

        # Определяем путь для файла подписи
        if signature_path is None:
            signature_path = str(input_path) + '.p7s'
        sig_path = Path(signature_path)

        # Создаем временный сертификат для pyHanko (требуется для формата PDF)
        # Это не настоящий сертификат УЦ, а технический формат для встраивания подписи
        cert = self._create_self_signed_certificate()

        # Сохраняем сертификат и ключ во временные файлы для SimpleSigner

        with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='.pem') as cert_file:
            cert_file.write(cert.public_bytes(serialization.Encoding.PEM))
            cert_path = cert_file.name

        with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='.pem') as key_file:
            key_file.write(self.private_key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption()
            ))
            key_path = key_file.name

        try:

            # Читаем содержимое PDF для подписания
            with open(input_path, 'rb') as inf:
                pdf_content = inf.read()

            # Создаем отсоединенную подпись вручную через asn1crypto
            # Вычисляем хэш PDF
            digest_algorithm = hashes.SHA256()
            hasher = hashes.Hash(digest_algorithm, default_backend())
            hasher.update(pdf_content)
            pdf_hash = hasher.finalize()

            # Создаем signed attributes
            signing_time = datetime.now(timezone.utc)

            signed_attrs = asn1crypto.cms.CMSAttributes([
                asn1crypto.cms.CMSAttribute({
                    'type': asn1crypto.cms.CMSAttributeType('1.2.840.113549.1.9.3'),  # contentType
                    'values': [asn1crypto.cms.ContentType('1.2.840.113549.1.7.1')]  # data
                }),
                asn1crypto.cms.CMSAttribute({
                    'type': asn1crypto.cms.CMSAttributeType('1.2.840.113549.1.9.5'),  # signingTime
                    'values': [asn1crypto.cms.Time({'utc_time': asn1crypto.core.UTCTime(signing_time)})]
                }),
                asn1crypto.cms.CMSAttribute({
                    'type': asn1crypto.cms.CMSAttributeType('1.2.840.113549.1.9.4'),  # messageDigest
                    'values': [asn1crypto.core.OctetString(pdf_hash)]
                }),
            ])

            # Подписываем signed attributes (с тегом SET OF)
            signed_attrs_der = signed_attrs.dump()
            # Заменяем тег 0xA0 на 0x31 (SET OF)
            signed_attrs_for_signing = b'\x31' + signed_attrs_der[1:]

            # Вычисляем хэш от signed attributes
            hasher = hashes.Hash(digest_algorithm, default_backend())
            hasher.update(signed_attrs_for_signing)
            signed_attrs_hash = hasher.finalize()

            # Подписываем хэш
            signature_bytes = self.private_key.sign(
                signed_attrs_hash,
                padding.PKCS1v15(),
                hashes.SHA256()
            )

            # Создаем CMS структуру
            # Конвертируем issuer из cryptography в asn1crypto
            cert_der = cert.public_bytes(serialization.Encoding.DER)
            cert_asn1 = asn1crypto.x509.Certificate.load(cert_der)

            signer_info = asn1crypto.cms.SignerInfo({
                'version': 'v1',
                'sid': asn1crypto.cms.SignerIdentifier({
                    'issuer_and_serial_number': asn1crypto.cms.IssuerAndSerialNumber({
                        'issuer': cert_asn1.issuer,
                        'serial_number': cert_asn1.serial_number
                    })
                }),
                'digest_algorithm': asn1crypto.algos.DigestAlgorithm({'algorithm': '2.16.840.1.101.3.4.2.1'}),  # SHA-256
                'signed_attrs': signed_attrs,
                'signature_algorithm': asn1crypto.algos.SignedDigestAlgorithm({'algorithm': '1.2.840.113549.1.1.1'}),  # RSA
                'signature': asn1crypto.core.OctetString(signature_bytes)
            })

            signed_data = asn1crypto.cms.SignedData({
                'version': 'v1',
                'digest_algorithms': asn1crypto.cms.DigestAlgorithms([
                    asn1crypto.algos.DigestAlgorithm({'algorithm': '2.16.840.1.101.3.4.2.1'})  # SHA-256
                ]),
                'encap_content_info': asn1crypto.cms.ContentInfo({
                    'content_type': '1.2.840.113549.1.7.1',  # data
                    'content': None  # detached signature
                }),
                'certificates': asn1crypto.cms.CertificateSet([
                    asn1crypto.x509.Certificate.load(cert.public_bytes(serialization.Encoding.DER))
                ]),
                'signer_infos': asn1crypto.cms.SignerInfos([signer_info])
            })

            content_info = asn1crypto.cms.ContentInfo({
                'content_type': '1.2.840.113549.1.7.2',  # signedData
                'content': signed_data
            })

            detached_signature = content_info.dump()

            # Сохраняем отсоединенную подпись в файл
            with open(sig_path, 'wb') as sig_file:
                sig_file.write(detached_signature)

        finally:
            # Удаляем временные файлы
            try:
                os.unlink(cert_path)
                os.unlink(key_path)
            except:
                pass

        # Метаданные подписи
        signature_metadata = {
            'user_name': self.user_name,
            'user_id': self.user_id,
            'timestamp': datetime.now().isoformat(),
            'reason': reason,
            'location': location,
            'public_key_hash': hashlib.sha256(
                self.public_key.public_bytes(
                    encoding=serialization.Encoding.PEM,
                    format=serialization.PublicFormat.SubjectPublicKeyInfo
                )
            ).hexdigest(),
            'signature_path': str(sig_path),
            'detached': True
        }

        return signature_metadata

    def verify_pdf(self, pdf_path: str, signature_path: Optional[str] = None) -> Dict[str, Any]:
        """
        Проверка отсоединенной подписи PDF документа.

        Args:
            pdf_path: Путь к PDF файлу
            signature_path: Путь к файлу отсоединенной подписи (если None, используется pdf_path + .p7s)

        Returns:
            Словарь с результатами проверки
        """
        pdf_path_obj = Path(pdf_path)

        if not pdf_path_obj.exists():
            raise FileNotFoundError(f"Файл не найден: {pdf_path}")

        # Определяем путь к файлу подписи
        if signature_path is None:
            signature_path = str(pdf_path_obj) + '.p7s'
        sig_path_obj = Path(signature_path)

        if not sig_path_obj.exists():
            result = {
                'valid': False,
                'integrity_ok': False,
                'signature_ok': False,
                'message': f'Файл подписи не найден: {signature_path}',
            }
            return result

        try:
            # Проверяем отсоединенную подпись через cryptography

            # Читаем PDF файл
            with open(pdf_path_obj, 'rb') as pdf_file:
                pdf_content = pdf_file.read()

            # Читаем файл подписи
            with open(sig_path_obj, 'rb') as sig_file:
                signature_data = sig_file.read()

            # Проверяем отсоединенную подпись напрямую через парсинг PKCS#7 структуры
            # pyHanko не имеет прямого API для отсоединенных подписей, поэтому используем
            # стандартный способ проверки PKCS#7 через asn1crypto и cryptography

            # Инициализируем флаги проверки
            integrity_ok = False
            signature_ok = False

            # Парсим PKCS#7 структуру
            cms_obj = asn1crypto.cms.ContentInfo.load(signature_data)

            # Проверяем, что это SignedData
            if cms_obj['content_type'].dotted != '1.2.840.113549.1.7.2':
                raise ValueError("Неверный тип PKCS#7 структуры")

            signed_data = cms_obj['content']

            # Проверяем наличие подписей
            if not signed_data['signer_infos']:
                raise ValueError("Подпись не содержит информации о подписанте")

            # Извлекаем сертификат из подписи (он должен быть там)
            if 'certificates' not in signed_data or not signed_data['certificates'] or len(signed_data['certificates']) == 0:
                raise ValueError("Сертификат не найден в подписи")

            # Получаем сертификат из подписи
            cert_der = signed_data['certificates'][0].dump()
            cert = x509.load_der_x509_certificate(cert_der, default_backend())

            # Проверяем, что публичный ключ сертификата соответствует нашему
            # (для НЭП это допустимо, так как мы используем самоподписанные сертификаты)
            if cert.public_key().public_numbers() != self.public_key.public_numbers():
                raise ValueError("Публичный ключ сертификата не соответствует ожидаемому")

            # Получаем первую подпись
            signer_info = signed_data['signer_infos'][0]

            # Извлекаем подпись (в байтах)
            signature_bytes = signer_info['signature'].contents

            # Вычисляем хэш PDF документа для проверки целостности
            digest_algorithm = hashes.SHA256()
            hasher = hashes.Hash(digest_algorithm, default_backend())
            hasher.update(pdf_content)
            pdf_hash = hasher.finalize()

            # 1. ПРОВЕРКА ЦЕЛОСТНОСТИ ДОКУМЕНТА (integrity_ok)
            try:
                if 'signed_attrs' not in signer_info or not signer_info['signed_attrs']:
                    # Если нет signed attributes, подпись подписывает сам документ
                    # Целостность проверяется через криптографическую проверку подписи
                    integrity_ok = True
                else:
                    # Если есть signed attributes, проверяем message digest
                    signed_attrs = signer_info['signed_attrs']

                    # Ищем message digest в signed attributes
                    message_digest_found = False
                    for attr in signed_attrs:
                        if attr['type'].dotted == '1.2.840.113549.1.9.4':  # messageDigest
                            message_digest = attr['values'][0].contents
                            if message_digest != pdf_hash:
                                raise ValueError("Message digest не соответствует документу")
                            message_digest_found = True
                            break

                    if not message_digest_found:
                        raise ValueError("Message digest не найден в signed attributes")

                    integrity_ok = True
            except Exception:
                integrity_ok = False

            # 2. ПРОВЕРКА КРИПТОГРАФИЧЕСКОЙ ПОДПИСИ (signature_ok)
            try:
                if 'signed_attrs' not in signer_info or not signer_info['signed_attrs']:
                    # Если нет signed attributes, подпись подписывает сам документ
                    cert.public_key().verify(
                        signature_bytes,
                        pdf_hash,
                        padding.PKCS1v15(),
                        hashes.SHA256()
                    )
                else:
                    # Подпись подписывает DER-кодированные signed attributes
                    signed_attrs_der = signer_info['signed_attrs'].dump()

                    # ВАЖНО: В PKCS#7 подпись создается от SET OF (тег 0x31),
                    # но asn1crypto кодирует signed_attrs с тегом IMPLICIT [0] (0xA0)
                    # Нужно заменить тег для проверки подписи
                    if signed_attrs_der[0] == 0xa0:
                        signed_attrs_for_verify = b'\x31' + signed_attrs_der[1:]
                    else:
                        signed_attrs_for_verify = signed_attrs_der

                    # Вычисляем хэш от signed attributes
                    hasher = hashes.Hash(digest_algorithm, default_backend())
                    hasher.update(signed_attrs_for_verify)
                    signed_attrs_hash = hasher.finalize()

                    # Проверяем подпись
                    cert.public_key().verify(
                        signature_bytes,
                        signed_attrs_hash,
                        padding.PKCS1v15(),
                        hashes.SHA256()
                    )

                signature_ok = True
            except Exception:
                signature_ok = False

            # Итоговая проверка: подпись действительна если:
            # 1. Документ не был изменен (integrity_ok)
            # 2. Подпись криптографически корректна (signature_ok)
            final_valid = integrity_ok and signature_ok

            # Формируем сообщение в зависимости от результата проверки
            if signature_ok and integrity_ok:
                message = 'Подпись действительна'
            elif not signature_ok and not integrity_ok:
                message = 'Подпись недействительна: криптографическая проверка не пройдена и документ был изменен'
            elif not signature_ok:
                message = 'Подпись недействительна: криптографическая проверка не пройдена'
            else:  # not integrity_ok
                message = 'Подпись недействительна: документ был изменен после подписания'

            result = {
                'valid': final_valid,
                'integrity_ok': integrity_ok,
                'signature_ok': signature_ok,
                'message': message,
            }

        except SignatureValidationError as e:
            result = {
                'valid': False,
                'integrity_ok': False,
                'signature_ok': False,
                'message': f'Ошибка проверки подписи: {str(e)}',
            }
        except Exception as e:
            # Если PDF поврежден (например, после модификации), это тоже означает недействительность подписи

            error_msg = str(e) if str(e) else repr(e)
            error_traceback = traceback.format_exc()

            if 'hex string' in error_msg.lower() or 'unexpected token' in error_msg.lower():
                result = {
                    'valid': False,
                    'integrity_ok': False,
                    'signature_ok': False,
                    'message': 'Документ был изменен или поврежден после подписания',
                    'error': error_msg,
                    'document_corrupted': True
                }
            else:
                # Для отладки: выводим более подробную информацию
                result = {
                    'valid': False,
                    'integrity_ok': False,
                    'signature_ok': False,
                    'message': f'Неожиданная ошибка: {error_msg}' if error_msg else 'Неожиданная ошибка при проверке подписи',
                }

        return result

    def _create_self_signed_certificate(self):
        """
        Создание самоподписанного сертификата для технических целей pyHanko.

        Это НЕ сертификат УЦ, а технический формат для встраивания подписи в PDF.
        """

        subject = issuer = x509.Name([
            x509.NameAttribute(NameOID.COUNTRY_NAME, "RU"),
            x509.NameAttribute(NameOID.ORGANIZATION_NAME, "CRM System"),
            x509.NameAttribute(NameOID.COMMON_NAME, self.user_name),
        ])

        cert = x509.CertificateBuilder().subject_name(
            subject
        ).issuer_name(
            issuer
        ).public_key(
            self.public_key
        ).serial_number(
            x509.random_serial_number()
        ).not_valid_before(
            datetime.now()
        ).not_valid_after(
            datetime.now().replace(year=datetime.now().year + 10)
        ).add_extension(
            x509.SubjectAlternativeName([
                x509.DNSName(f"user-{self.user_id}")
            ]),
            critical=False,
        ).sign(self.private_key, hashes.SHA256(), default_backend())

        return cert

