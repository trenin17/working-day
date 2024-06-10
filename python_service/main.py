from aiohttp import web
from generate_document.generate import generate_document
from sign_document.sign import sign_document

app = web.Application()
app.add_routes([web.post('/document/generate', generate_document)])
app.add_routes([web.get('/documents/sign', sign_document)])

web.run_app(app, host='0.0.0.0', port=3000)
