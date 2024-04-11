import ssl

import aiohttp
import asyncio
from wsgiref import headers

ssl_ctx = ssl.create_default_context(
    cafile=str('/home/trenin/project/working_day/configs/cert.pem'),
)

ssl_ctx.check_hostname = False
ssl_ctx.verify_mode = ssl.CERT_NONE

conn = aiohttp.TCPConnector(ssl=ssl_ctx)

async def test_https():
    async with aiohttp.ClientSession(connector=conn) as session:
        async with session.get(
                f'https://localhost:8080/v1/documents/vacation?action_id=1230da1c73514c96afa68c31f0e94bb8',
                headers={'Authorization': 'Bearer 7c4db9ff92e745d39b870c35c18e01a9'},
        ) as response:
            print(response.status)
            print(await response.text())

loop = asyncio.get_event_loop()
loop.run_until_complete(test_https())
