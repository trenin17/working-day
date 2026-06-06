import asyncio
import subprocess
from aiohttp import web
from s3_client.aws_utils import download_file


def delete_tmp_files(file_key):
    subprocess.run(['rm', '-rf', '/tmp/' + file_key + '*'])


async def process_document(file_key, _data):
    """Source PDF is already in S3 (template/upload). PEP stamp and NEP use other endpoints."""
    output_path_pdf = '/tmp/' + file_key + '.pdf'
    download_file(file_key=file_key + '.pdf', path_to_file=output_path_pdf)
    delete_tmp_files(file_key)
    return file_key


tasks = {}


async def worker(file_key, data):
    try:
        return await process_document(file_key, data)
    finally:
        tasks.pop(file_key, None)


async def generate_document(request):
    file_key = request.rel_url.query['file_key']
    data = await request.json()
    if file_key not in tasks:
        tasks[file_key] = asyncio.create_task(worker(file_key, data))
    return web.Response(status=200, content_type='text/plain', text=file_key)
