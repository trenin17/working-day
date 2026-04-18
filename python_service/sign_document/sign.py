import subprocess
import uuid
from datetime import datetime

from aiohttp import web

from .stamp import StampData, create_stamp
from s3_client.aws_utils import download_file, upload_file


async def create_stamp_for_nep(request):
    data = await request.json()

    path_to_pdf = '/tmp/' + str(uuid.uuid4()) + '.pdf'
    file_key = data['file_key']
    download_file(file_key, path_to_pdf)

    path_to_new_pdf = '/tmp/' + str(uuid.uuid4()) + '.pdf'

    date = datetime.today().strftime('%d.%m.%Y')
    raw_signers = data.get('signers')
    if not isinstance(raw_signers, list):
        return web.json_response(
            {'error': 'signers must be a JSON array of objects'},
            status=400,
        )
    signers = [s for s in raw_signers if s and isinstance(s, dict)]
    if not signers:
        return web.json_response(
            {'error': 'signers must be a non-empty list of objects'},
            status=400,
        )
    organization = data.get('organization') or ''
    is_first_signature = data.get('is_first_signature', True)
    signed_file_key = data['signed_file_key']
    document_stem = (
        signed_file_key[:-4] if signed_file_key.endswith('.pdf') else signed_file_key
    )

    stamp_data = StampData(
        date,
        '',
        organization,
        '',
        document_stem,
        sign_type='УНЭП',
        nep_signers=signers,
    )
    create_stamp(path_to_pdf, path_to_new_pdf, stamp_data, is_first_signature)

    upload_file(path_to_new_pdf, signed_file_key)

    subprocess.run(['rm', path_to_pdf])
    subprocess.run(['rm', path_to_new_pdf])

    return web.Response(status=200, content_type='text/plain', text=signed_file_key)
