import asyncio
import uuid
from aiohttp import web
import subprocess
from s3_client.aws_utils import download_file, upload_file

tasks = {}

def convert_docx_to_pdf(docx_path, output_pdf_path):
    cmd = [
        ### скачать бинарь libreoffice
        'libreoffice', '--headless', '--convert-to', 'pdf', '--outdir',
        output_pdf_path, docx_path
    ]
    subprocess.run(cmd, check=True)


async def worker(file_key, converted_file_key):
    input_path = f'/tmp/{file_key}'
    output_path = f'/tmp/{converted_file_key}'
    try:
        download_file(file_key, input_path)
        convert_docx_to_pdf(input_path, '/tmp')
        upload_file(output_path, converted_file_key)
    finally:
        subprocess.run(['rm', '-f', input_path])
        subprocess.run(['rm', '-f', output_path])

async def convert_document(request):
    data = await request.json()
    file_key = data['file_key']
    converted_file_key = data.get('converted_file_key') or str(uuid.uuid4()) + ".pdf"

    task = asyncio.create_task(worker(file_key, converted_file_key))
    tasks[converted_file_key] = task


    return web.json_response({"converted_file_key": converted_file_key})
