from aiohttp import web
from generate_document.generate import generate_document

app = web.Application()
app.add_routes([web.post('/document/generate', generate_document)])

web.run_app(app, host='127.0.0.1', port=3000)