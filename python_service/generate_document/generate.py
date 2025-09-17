from datetime import datetime
import subprocess
from aiohttp import web
from sign_document.stamp import create_stamp, StampData
from s3_client.aws_utils import upload_and_presign, download_file

import asyncio

def delete_tmp_files(file_key):
    subprocess.run(['rm', '-rf', '/tmp/' + file_key + '*'])


async def process_document(file_key, data):
    employee_id = data['employee_id']
    employee_name = data['employee_name']
    employee_surname = data['employee_surname']
    employee_patronymic = data.get('employee_patronymic', "")
    company_name = data.get('subcompany', "")

    now_date = datetime.today().strftime('%d.%m.%Y')

    output_path_pdf = '/tmp/' + file_key + '.pdf'
    download_file(file_key=file_key + '.pdf', path_to_file=output_path_pdf)

    output_path_pdf_signed = '/tmp/' + file_key + '_signed.pdf'

    stamp_data = StampData(now_date, employee_name + " " + employee_surname + " " + employee_patronymic,
                            company_name, employee_id, file_key)
    create_stamp(output_path_pdf, output_path_pdf_signed, stamp_data)

    url = upload_and_presign(output_path_pdf_signed, file_key + '_signed.pdf')
    delete_tmp_files(file_key)
    return url

tasks = {}
async def worker(file_key, data):
    try:
        result = await process_document(file_key, data)
        return result
    except Exception as e:
        return None
    finally:
        tasks.pop(file_key, None)

async def generate_document(request):
    # Retrieve `file_key` from the query string
    file_key = request.rel_url.query['file_key']

    # Get JSON data from the body
    data = await request.json()

    if file_key not in tasks:
        # запускаем worker в фоне
        tasks[file_key] = asyncio.create_task(worker(file_key, data))

    # возвращаем сразу
    return web.Response(status=200, content_type='text/plain', text=file_key)
