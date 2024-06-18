from aiohttp import web
import uuid
import subprocess
from datetime import datetime
from .stamp import create_stamp, StampData
from s3_client.aws_utils import download_file, upload_file

async def sign_document(request):
    try:
        data = await request.json()
        
        path_to_pdf = '/tmp/' + str(uuid.uuid4()) + '.pdf'
        file_key = data['file_key']
        download_file(file_key, path_to_pdf)

        path_to_new_pdf = '/tmp/' + str(uuid.uuid4()) + '.pdf'

        date = datetime.today().strftime('%d.%m.%Y')
        employee_id = data['employee_id']
        employee_name = data['employee_name']
        employee_surname = data['employee_surname']
        employee_patronymic = data.get('employee_patronymic', "")
        employee_initials = f"{employee_surname} {employee_name[0]}."
        if employee_patronymic:
            employee_initials += f"{employee_patronymic[0]}."
        organization = 'ЕСВ.ТЕХНОЛОДЖИ-ПЛЮС' # TODO: change company name
        sign_type = 'Простая ЭП'
        signed_file_key = data['signed_file_key']

        stamp_data = StampData(date, employee_initials, organization, employee_id, signed_file_key[0:-4], sign_type)
        create_stamp(path_to_pdf, path_to_new_pdf, stamp_data)

        upload_file(path_to_new_pdf, signed_file_key)

        subprocess.run(['rm', path_to_pdf])
        subprocess.run(['rm', path_to_new_pdf])

        return web.Response(status=200, content_type='text/plain', text=signed_file_key)
    except Exception as e:
        return web.Response(status=500, text=str(e))    
    