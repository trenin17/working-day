from aiohttp import web
import uuid
from .stamp import create_stamp, StampData

async def sign_document(request):
    try:
        data = await request.json()
        path_to_pdf = data['path_to_pdf']
        path_to_new_pdf = '/tmp/' + str(uuid.uuid4())
        date = data['date']
        name = data['name']
        organization = data['organization']
        user_id = data['user_id']
        document_id = data['document_id']
        sign_type = data.get('sign_type', 'Простая ЭП')
        stamp_data = StampData(date, name, organization, user_id, document_id, sign_type)
        create_stamp(path_to_pdf, path_to_new_pdf, stamp_data)
        # TODO: upload signed pdf and delete temporary files
        return web.Response(status=200, content_type='text/plain', text='OK')
    except Exception as e:
        return web.Response(status=500, text=str(e))    
    