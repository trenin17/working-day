"""
Обработчик для подписания PDF документов с использованием НЭП
"""
import json
import logging
from aiohttp import web
from .signer import NEPSigner
from s3_client.aws_utils import download_file, upload_file


async def nep_sign_document(request):
    """
    Подписание PDF документа с использованием НЭП

    Ожидает JSON с полями:
    - document_id: ID документа (используется для формирования пути в S3)
    - employee_id: ID сотрудника
    - employee_name: Имя сотрудника
    - private_key: Приватный ключ в формате PEM
    - public_key: Публичный ключ в формате PEM
    - reason: (опционально) Причина подписания
    - location: (опционально) Место подписания
    """
    try:
        data = await request.json()

        document_id = data.get('document_id')
        employee_id = data.get('employee_id')
        employee_name = data.get('employee_name')
        private_key_pem = data.get('private_key')
        public_key_pem = data.get('public_key')
        reason = data.get('reason')
        location = data.get('location')

        if not all([document_id, employee_id, employee_name, private_key_pem, public_key_pem]):
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

        # Создаем подписанта
        signer = NEPSigner(user_name=employee_name, user_id=employee_id)

        # Загружаем ключи
        try:
            signer.load_key_pair(
                private_key_pem=private_key_pem.encode('utf-8') if isinstance(private_key_pem, str) else private_key_pem,
                public_key_pem=public_key_pem.encode('utf-8') if isinstance(public_key_pem, str) else public_key_pem
            )
        except Exception as e:
            logging.error(f"Failed to load keys: {str(e)}")
            return web.json_response(
                {'error': 'Failed to load keys. Check logs for more details.'},
                status=400
            )

        # Путь для файла подписи
        signature_local_path = f'/tmp/{document_id}_{employee_id}.p7s'

        # Подписываем документ
        try:
            signature_metadata = signer.sign_pdf(
                input_pdf_path=pdf_local_path,
                signature_path=signature_local_path,
                reason=reason,
                location=location
            )
        except Exception as e:
            logging.error(f"Failed to sign document: {str(e)}")
            return web.json_response(
                {'error': 'Failed to sign document. Check logs for more details.'},
                status=500
            )

        # Загружаем файл подписи в S3
        signature_s3_key = f'{document_id}_{employee_id}.p7s'
        try:
            upload_file(signature_local_path, signature_s3_key)
        except Exception as e:
            logging.error(f"Failed to upload signature: {str(e)}")
            return web.json_response(
                {'error': 'Failed to upload signature. Check logs for more details.'},
                status=500
            )

        # Возвращаем метаданные подписи
        response_data = {
            'signature_path': signature_s3_key,
            'timestamp': signature_metadata['timestamp'],
            'public_key_hash': signature_metadata['public_key_hash'],
            'user_name': signature_metadata['user_name'],
            'user_id': signature_metadata['user_id'],
            'reason': signature_metadata.get('reason'),
            'location': signature_metadata.get('location'),
        }

        return web.json_response(response_data)

    except json.JSONDecodeError:
        return web.json_response(
            {'error': 'Invalid JSON'},
            status=400
        )
    except Exception as e:
        logging.error(f"Unexpected error in nep_sign_document: {str(e)}")
        return web.json_response(
            {'error': 'Internal server error. Check logs for more details.'},
            status=500
        )
