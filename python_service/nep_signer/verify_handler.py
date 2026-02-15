"""
Обработчик для проверки подписей PDF документов
"""
import json
import logging
from aiohttp import web
from .signer import NEPSigner
from s3_client.aws_utils import download_file

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend


async def nep_verify_document(request):
    """
    Проверка подписи PDF документа

    Ожидает JSON с полями:
    - document_id: ID документа (используется для формирования пути в S3)
    - signature_path: Путь к файлу подписи в S3
    - public_key: Публичный ключ в формате PEM
    """
    try:
        data = await request.json()

        document_id = data.get('document_id')
        signature_path = data.get('signature_path')
        public_key_pem = data.get('public_key')

        if not all([document_id, signature_path, public_key_pem]):
            return web.json_response(
                {'error': 'Missing required fields'},
                status=400
            )

        # Загружаем PDF из S3
        pdf_local_path = f'/tmp/{document_id}'
        try:
            download_file(document_id, pdf_local_path)
        except Exception as e:
            logging.error(f"Failed to download document {document_id}: {str(e)}")
            return web.json_response(
                {'error': 'Failed to download document. Check logs for more details.'},
                status=500
            )

        # Загружаем файл подписи из S3
        signature_local_path = f'/tmp/{signature_path}'
        try:
            download_file(signature_path, signature_local_path)
        except Exception as e:
            logging.error(f"Failed to download signature {signature_path}: {str(e)}")
            return web.json_response(
                {'error': 'Failed to download signature. Check logs for more details.'},
                status=500
            )

        # Создаем верификатор (имя не важно для проверки)
        verifier = NEPSigner(user_name="Verifier", user_id="verifier")

        # Загружаем публичный ключ
        try:
            # Для проверки нужен только публичный ключ, приватный не нужен
            # Но load_key_pair требует приватный ключ, поэтому загрузим только публичный ключ напрямую

            verifier.public_key = serialization.load_pem_public_key(
                public_key_pem.encode('utf-8') if isinstance(public_key_pem, str) else public_key_pem,
                backend=default_backend()
            )
        except Exception as e:
            logging.error(f"Failed to load public key: {str(e)}")
            return web.json_response(
                {'error': 'Failed to load public key. Check logs for more details.'},
                status=400
            )

        # Проверяем подпись
        try:
            verification_result = verifier.verify_pdf(
                pdf_path=pdf_local_path,
                signature_path=signature_local_path
            )
        except Exception as e:
            logging.error(f"Failed to verify signature: {str(e)}")
            return web.json_response(
                {'error': 'Failed to verify signature. Check logs for more details.'},
                status=500
            )

        return web.json_response(verification_result)

    except json.JSONDecodeError:
        return web.json_response(
            {'error': 'Invalid JSON'},
            status=400
        )
    except Exception as e:
        logging.exception(f"Unexpected error in nep_verify_document: {str(e)}")
        return web.json_response(
            {'error': 'Internal server error. Check logs for more details.'},
            status=500
        )
