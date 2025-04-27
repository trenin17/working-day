from threading import Lock
from aiohttp import web
import subprocess
from s3_client.aws_utils import download_file, upload_file

mutex = Lock()

def convert_docx_to_pdf(docx_path, output_pdf_path):
    with mutex:
        cmd = [
            'libreoffice', '--headless', '--convert-to', 'pdf', '--outdir',
            output_pdf_path, docx_path
        ]
        subprocess.run(cmd, check=True)

async def convert_document(request):
    try:
        data = await request.json()
        file_key = data['file_key']
        converted_file_key = data['converted_file_key']
        input_path = '/tmp/' + file_key
        output_path = '/tmp/' + converted_file_key

        download_file(file_key, input_path)

        convert_docx_to_pdf(input_path, output_path)

        upload_file(output_path, converted_file_key)
        
        subprocess.run(['rm', input_path])
        subprocess.run(['rm', output_path])

        return web.Response(status=200, content_type='text/plain', text=file_key + '.pdf')
    except Exception as e:
        return web.Response(status=500, text=str(e))