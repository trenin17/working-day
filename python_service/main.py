from aiohttp import web
from generate_document.generate import generate_document
from sign_document.sign import sign_document
import subprocess


cmd = [
    'libreoffice', '--headless', '--convert-to', 'pdf', '--outdir',
    '/tmp', 'generate_document/templates/esv_create.docx'
]
subprocess.run(cmd, check=True)
subprocess.run(['rm', '/tmp/esv_create.pdf'])

app = web.Application()
app.add_routes([web.post('/document/generate', generate_document)])
app.add_routes([web.post('/document/sign', sign_document)])

web.run_app(app, host='0.0.0.0', port=3000)
