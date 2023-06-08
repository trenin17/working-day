from wsgiref import headers
import pytest

from testsuite.databases import pgsql


# Start the tests via `make test-debug` or `make test-release`


'''
async def test_first_time_users(service_client):
    response = await service_client.post(
        '/v1/hello',
        params={'name': 'userver'},
    )
    assert response.status == 200
    assert response.text == 'Hello, userver!\n'


async def test_db_updates(service_client):
    response = await service_client.post('/v1/hello', params={'name': 'World'})
    assert response.status == 200
    assert response.text == 'Hello, World!\n'

    response = await service_client.post('/v1/hello', params={'name': 'World'})
    assert response.status == 200
    assert response.text == 'Hi again, World!\n'

    response = await service_client.post('/v1/hello', params={'name': 'World'})
    assert response.status == 200
    assert response.text == 'Hi again, World!\n'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_db_initial_data(service_client):
    response = await service_client.post(
        '/v1/hello',
        params={'name': 'user-from-initial_data.sql'},
    )
    assert response.status == 200
    assert response.text == 'Hi again, user-from-initial_data.sql!\n'
'''


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_db_initial_data(service_client):
    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == '{"id":"first_id","name":"First","surname":"A"}'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_add_head(service_client):
    response = await service_client.post(
        '/v1/employee/add-head',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
        json={'head_id': 'first_id'},
    )
    
    assert response.status == 200

    response = await service_client.get(
        '/v1/employees',
        params={'employee_id': '123'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == '{"employees":[{"id":"second_id","name":"Second","surname":"B"}]}'
