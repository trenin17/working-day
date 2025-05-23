from aiohttp import web
from generate_document.generate import generate_document
from sign_document.sign import sign_document
from convert_document.convert import convert_document
import subprocess
import logging

@web.middleware
async def error_logging_middleware(request, handler):
    try:
        response = await handler(request)
        if response.status >= 500:
            # Log server errors explicitly if returned by handler
            logging.error(f"500-level response for {request.method} {request.path}")
        return response
    except Exception:
        logging.exception(f"Exception occurred while handling request: {request.method} {request.path}")
        raise  # Let aiohttp generate the 500 response


cmd = [
    'libreoffice', '--headless', '--convert-to', 'pdf', '--outdir',
    '/tmp', 'generate_document/templates/esv_create.docx'
]
subprocess.run(cmd, check=True)
subprocess.run(['rm', '/tmp/esv_create.pdf'])

logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s %(levelname)s %(name)s: %(message)s',
    filename='/tmp/logs/log.txt',
    filemode='a'  # Use 'a' to append instead
)
logging.getLogger('aiohttp.server').setLevel(logging.ERROR)


app = web.Application(middlewares=[error_logging_middleware])
app.add_routes([web.post('/document/generate', generate_document)])
app.add_routes([web.post('/document/sign', sign_document)])
app.add_routes([web.post('/document/convert', convert_document)])

web.run_app(app, host='0.0.0.0', port=3000)
