import asyncio
from wsgiref import headers
import pytest
import json
from string import Template

from testsuite.databases import pgsql


# Start the tests via `make test-debug` or `make test-release`

def normalize_json(obj):
    """Recursively normalize the JSON object so that order does not matter."""
    if isinstance(obj, dict):
        return {k: normalize_json(v) for k, v in sorted(obj.items())}
    elif isinstance(obj, list):
        return sorted((normalize_json(i) for i in obj), key=lambda x: json.dumps(x, sort_keys=True))
    else:
        return obj


def are_json_equal(json_str1, json_str2):
    """Compare two JSON strings to see if they are equal regardless of order."""
    obj1 = json.loads(json_str1)
    obj2 = json.loads(json_str2)
    return normalize_json(obj1) == normalize_json(obj2)


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_db_initial_data(service_client):
    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"id":"first_id","inventory":[],"name":"First",'
                             '"phones":[],"surname":"A"}')


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_add_company(service_client):
    response = await service_client.post(
        '/v1/superuser/company/add',
        headers={'Authorization': 'Bearer zero_token'},
        json={'company_id': 'second', 'company_name': 'Second company'},
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer zero_token'},
        json={'name': 'Second', 'surname': 'B',
              'role': 'admin', 'company_id': 'second'},
    )

    assert response.status == 200
    # new_id = json.loads(response.text)['login']
    # new_password = json.loads(response.text)['password']


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_employees(service_client):
    response = await service_client.post(
        '/v1/employee/add-head',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
        json={'head_id': 'first_id'},
    )

    assert response.status == 200

    response = await service_client.get(
        '/v1/employees',
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}]}')


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
        '/v1/employee/info',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200

    response_body = response.text
    head_id = json.loads(response_body)['head_id']

    assert head_id == 'first_id'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_add(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Third', 'surname': 'C', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    new_password = json.loads(response.text)['password']
    assert new_id == 'tc'

    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': new_id},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"id":"' + new_id + '","inventory":[],"name":"Third",'
                             '"password":"' + new_password + '",'
                             '"phones":[],"surname":"C"}')

    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Александр', 'surname': 'Петров', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    assert new_id == 'apetrov'

    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Алексей', 'surname': 'Петров', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    assert new_id == 'apetrov1'

    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Антон', 'surname': 'Петров', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    assert new_id == 'apetrov2'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_remove(service_client):
    response = await service_client.post(
        '/v1/employee/remove',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Third', 'surname': 'C', 'role': 'user'},
    )

    assert response.status == 200

    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status != 200


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_edit(service_client):
    response = await service_client.post(
        '/v1/profile/edit',
        headers={'Authorization': 'Bearer second_token'},
        json={'phones': ['+7999999999']},
    )

    assert response.status == 200

    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200

    response_body = response.text
    phones = json.loads(response_body)['phones']

    assert phones == ['+7999999999']

    response = await service_client.post(
        '/v1/profile/edit',
        headers={'Authorization': 'Bearer second_token'},
        json={'email': 'second@mail.com'},
    )

    assert response.status == 200

    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200

    response_body = response.text
    email = json.loads(response_body)['email']

    assert email == 'second@mail.com'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_authorize(service_client):
    response = await service_client.post(
        '/v1/profile/edit',
        headers={'Authorization': 'Bearer first_token'},
        json={'password': 'first_password'},
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/authorize',
        json={'login': 'first_id', 'company_id': 'first',
              'password': 'first_password'},
    )

    assert response.status == 200

    # Wrong password
    response = await service_client.post(
        '/v1/authorize',
        json={'login': 'first_id', 'company_id': 'first',
              'password': 'wrong_password'},
    )

    assert response.status == 404

    # Wrong token
    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer wrong_token'},
    )

    assert response.status == 403

    # Authorization header without Bearer
    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'first_token'},
    )

    assert response.status == 401

    # No authorization header
    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'first_id'},
    )

    assert response.status == 401

    # No admin rights
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer second_token'},
        json={'name': 'Third', 'surname': 'C', 'role': 'user'},
    )

    assert response.status == 403


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_search_basic(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Fourth', 'surname': 'D', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    new_password = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/clear-tasks',
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/search/basic',
        json={'search_key': 'Fourth'},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Fourth","surname":"D"}]}')
    assert response.text == (response_required.substitute(id=new_id))


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_search_add(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Fourth', 'surname': 'D', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']

    response = await service_client.post(
        '/v1/clear-tasks',
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'FOURTH', 'limit': 1},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Fourth","surname":"D"}]}')
    assert response.text == (response_required.substitute(id=new_id))


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_search_remove(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Fifth', 'surname': 'E', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    new_password = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/employee/remove',
        params={'employee_id': new_id},
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Fifth', 'surname': 'E', 'role': 'user'},
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'Fifth', 'limit': 1},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'E', 'limit': 1},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[]}')


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_search_edit(service_client):
    # Add phone
    response = await service_client.post(
        '/v1/profile/edit',
        headers={'Authorization': 'Bearer second_token'},
        json={'phones': ['+1111']},
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/clear-tasks',
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '+1111', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}]}')

    # Add email
    response = await service_client.post(
        '/v1/profile/edit',
        headers={'Authorization': 'Bearer second_token'},
        json={'email': '2@mail.com'},
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/clear-tasks',
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '2@mail.com', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}]}')

    # Edit phone
    response = await service_client.post(
        '/v1/profile/edit',
        headers={'Authorization': 'Bearer second_token'},
        json={'phones': ['+2222']},
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/clear-tasks',
    )

    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '+1111', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '+2222', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '2@mail.com', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}]}')


def compare_employees(json_str1, json_str2):
    def json_to_set(json_str):
        data = json.loads(json_str)
        return set(tuple(employee.items()) for employee in data['employees'])

    set1 = json_to_set(json_str1)
    set2 = json_to_set(json_str2)

    return set1 == set2


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_search_full(service_client):
    # basic full
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Seventh', 'surname': 'F', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    new_password = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'SEVenth F user', 'limit': 1},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Seventh","surname":"F"}]}')
    assert compare_employees(response.text,
                             response_required.substitute(id=new_id))

    # two people
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Eight', 'surname': 'F', 'role': 'user'},
    )

    assert response.status == 200
    new_id2 = json.loads(response.text)['login']
    new_password2 = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'SEVenth F', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Seventh","surname":"F"},'
                                 '{"id":"${id2}",'
                                 '"name":"Eight","surname":"F"}]}')
    assert compare_employees(response.text,
                             response_required.substitute(id=new_id,
                                                          id2=new_id2))

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'F', 'limit': 10},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":['
                                 '{"id":"${id}",'
                                 '"name":"Seventh","surname":"F"},'
                                 '{"id":"${id2}",'
                                 '"name":"Eight","surname":"F"}]}')
    assert compare_employees(response.text,
                             response_required.substitute(id=new_id,
                                                          id2=new_id2))

    # three
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Seventh', 'surname': 'G', 'role': 'user'},
    )

    assert response.status == 200
    new_id3 = json.loads(response.text)['login']
    new_password3 = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'Seventh F', 'limit': 10},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Seventh","surname":"F"},'
                                 '{"id":"${id2}",'
                                 '"name":"Seventh","surname":"G"},'
                                 '{"id":"${id3}",'
                                 '"name":"Eight","surname":"F"}]}')
    assert compare_employees(response.text,
                             response_required.substitute(id=new_id,
                                                          id2=new_id3,
                                                          id3=new_id2))


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_attendance_list_all(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Third', 'surname': 'C', 'role': 'manager'},
    )
    assert response.status == 200
    employee_id = json.loads(response.text)['login']
    password = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/authorize',
        json={'login': employee_id, 'company_id': 'first', 'password': password},
    )
    assert response.status == 200
    token = json.loads(response.text)['token']

    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-21T10:00:00',
              'end_date': '2023-07-21T18:00:00'}
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/attendance/list-all',
        headers={'Authorization': 'Bearer ' + token},
        json={'from': '2023-07-21T00:00:00', 'to': '2023-07-22T00:00:00'}
    )

    assert response.status == 200
    assert are_json_equal(response.text, (
        '{"attendances":['
        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-21T18:00:00.000000",'
        '"start_date":"2023-07-21T10:00:00.000000"},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"}},'
        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}}'
        ']}')) == True
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T10:00:00',
              'end_date': '2023-07-22T18:00:00'}
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T9:00:00',
              'end_date': '2023-07-22T17:00:00'}
    )
    assert response.status == 200

    # Check overwrite the previous attendance
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T10:00:00',
              'end_date': '2023-07-22T18:00:00'}
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/attendance/list-all',
        headers={'Authorization': 'Bearer ' + token},
        json={'from': '2023-07-21T00:00:00', 'to': '2023-07-23T00:00:00'}
    )

    assert response.status == 200
    assert are_json_equal(response.text, (
        '{"attendances":['
        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-22T18:00:00.000000",'
        '"start_date":"2023-07-22T10:00:00.000000"},'
        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-21T18:00:00.000000",'
        '"start_date":"2023-07-21T10:00:00.000000"},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"},'
        '"end_date":"2023-07-22T18:00:00.000000",'
        '"start_date":"2023-07-22T10:00:00.000000"},'
        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}}'
        ']}')) == True
    response = await service_client.post(
        '/v1/attendance/list-all',
        headers={'Authorization': 'Bearer ' + token},
        json={'from': '2023-07-22T00:00:00', 'to': '2023-07-23T00:00:00'}
    )

    assert response.status == 200
    assert are_json_equal(response.text, (
        '{"attendances":['
        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-22T18:00:00.000000",'
        '"start_date":"2023-07-22T10:00:00.000000"},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"},'
        '"end_date":"2023-07-22T18:00:00.000000",'
        '"start_date":"2023-07-22T10:00:00.000000"},'
        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}}'
        ']}')) == True

    response = await service_client.post(
        '/v1/abscence/request',
        headers={'Authorization': 'Bearer first_token'},
        json={'type': 'sick_leave', 'start_date': '2023-07-22T00:00:00',
              'end_date': '2023-07-23T00:00:00'}
    )
    assert response.status == 200
    action_id = json.loads(response.text)['action_id']

    response = await service_client.post(
        '/v1/abscence/verdict',
        headers={'Authorization': 'Bearer first_token'},
        json={'action_id': action_id, 'approve': True}
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/abscence/request',
        headers={'Authorization': 'Bearer ' + token},
        json={'type': 'unpaid_vacation', 'start_date': '2023-07-01T00:00:00',
              'end_date': '2023-07-30T00:00:00'}
    )
    assert response.status == 200
    action_id = json.loads(response.text)['action_id']

    response = await service_client.post(
        '/v1/abscence/verdict',
        headers={'Authorization': 'Bearer ' + token},
        json={'action_id': action_id, 'approve': True}
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/attendance/list-all',
        headers={'Authorization': 'Bearer ' + token},
        json={'from': '2023-07-21T00:00:00', 'to': '2023-07-23T00:00:00'}
    )

    assert response.status == 200
    assert are_json_equal(response.text, (
        '{"attendances":'
        '[{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-21T18:00:00.000000","start_date":"2023-07-21T10:00:00.000000"},'
        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"},'
        '"end_date":"2023-07-22T18:00:00.000000","start_date":"2023-07-22T10:00:00.000000"},'
        '{"abscence_type":"sick_leave",'
        '"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-22T23:59:00.000000","start_date":"2023-07-22T00:00:00.000000"},'
        '{"abscence_type":"sick_leave",'
        '"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-23T23:59:00.000000","start_date":"2023-07-23T00:00:00.000000"},'
        '{"abscence_type":"unpaid_vacation",'
        '"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"},'
        '"end_date":"2023-07-23T23:59:00.000000","start_date":"2023-07-23T00:00:00.000000"},'
        '{"abscence_type":"unpaid_vacation",'
        '"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"},'
        '"end_date":"2023-07-22T23:59:00.000000","start_date":"2023-07-22T00:00:00.000000"},'
        '{"abscence_type":"unpaid_vacation",'
        '"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"},'
        '"end_date":"2023-07-21T23:59:00.000000","start_date":"2023-07-21T00:00:00.000000"}'
        ']}')) == True


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_actions(service_client):
    response = await service_client.post(
        '/v1/abscence/request',
        headers={'Authorization': 'Bearer first_token'},
        json={'type': 'vacation', 'start_date': '2023-07-10T00:00:00',
              'end_date': '2023-07-21T00:00:00'}
    )
    assert response.status == 200
    action_id = json.loads(response.text)['action_id']

    response = await service_client.post(
        '/v1/actions',
        headers={'Authorization': 'Bearer first_token'},
        json={'from': '2023-07-10T00:00:00', 'to': '2023-07-11T00:00:00'}
    )
    assert response.status == 200
    assert response.text == (
        '{"actions":[{"blocking_actions_ids":[],'
        '"end_date":"2023-07-21T23:59:00.000000","id":"' + action_id + '",'
        '"start_date":"2023-07-10T00:00:00.000000","status":"pending","type":"vacation"'
        '}]}')


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_documents_send(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Third', 'surname': 'C', 'role': 'admin'},
    )
    assert response.status == 200
    employee_id = json.loads(response.text)['login']
    password = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/authorize',
        json={'login': employee_id, 'company_id': 'first', 'password': password},
    )
    assert response.status == 200
    token = json.loads(response.text)['token']

    response = await service_client.post(
        '/v1/documents/send',
        headers={'Authorization': 'Bearer ' + token},
        json={'employee_ids': ['first_id', 'second_id'], 'document': {
            'id': 'id1', 'name': 'doc1',
            'description': 'text1', 'sign_required': True}}
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/documents/list',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    assert response.text == (
        '{"documents":[{"description":"text1","id":"id1",'
        '"name":"doc1","sign_required":true,"signed":false,"type":"admin_request"}]}')

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    created_time = json.loads(response.text)['notifications'][0]['created']
    id = json.loads(response.text)['notifications'][0]['id']
    assert response.text == (
        '{"notifications":[{"created":"' + created_time + '","id":"' + id + '","is_read":false,"sender":{"id":"tc","name":"Third","surname":"C"},"text":"Вам отправлен новый документ \\"doc1\\". Его можно просмотреть в разделе Документы.","type":"generic"}]}')

    response = await service_client.get(
        '/v1/documents/list',
        headers={'Authorization': 'Bearer second_token'}
    )
    assert response.status == 200
    assert response.text == (
        '{"documents":[{"description":"text1","id":"id1",'
        '"name":"doc1","sign_required":true,"signed":false,"type":"admin_request"}]}')

    response = await service_client.post(
        '/v1/documents/sign',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'id1'}
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/documents/list-all',
        headers={'Authorization': 'Bearer ' + token},
    )
    assert response.status == 200
    assert response.text == (
        '{"documents":[{"description":"text1","id":"id1",'
        '"name":"doc1","sign_required":true,"type":"admin_request"}]}')

    response = await service_client.get(
        '/v1/documents/get-signs',
        headers={'Authorization': 'Bearer ' + token},
        params={'document_id': 'id1'}
    )
    assert response.status == 200
    assert response.text == (
        '{"signs":['
        '{"employee":{"id":"second_id","name":"Second","surname":"B"},'
        '"signed":false},'
        '{"employee":{"id":"first_id","name":"First","surname":"A"},'
        '"signed":true}'
        ']}')


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_search_suggest(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Test1', 'surname': 'T1', 'role': 'user'},
    )

    assert response.status == 200
    new_id = json.loads(response.text)['login']
    new_password = json.loads(response.text)['password']

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': 'ZZZZZZZ', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == '{"employees":[]}'

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': 'T1 Test1 Bla', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == '{"employees":[]}'

    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'Test2', 'surname': 'T1', 'role': 'user'},
    )
    assert response.status == 200
    new_id2 = json.loads(response.text)['login']
    new_password2 = json.loads(response.text)['password']

    await asyncio.sleep(1)

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': 'T1 Test', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Test1","surname":"T1"},'
                                 '{"id":"${id2}",'
                                 '"name":"Test2","surname":"T1"}]}')
    assert compare_employees(
        response.text, response_required.substitute(id=new_id, id2=new_id2))

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': 'Test', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Test1","surname":"T1"},'
                                 '{"id":"${id2}",'
                                 '"name":"Test2","surname":"T1"}]}')
    assert compare_employees(
        response.text, response_required.substitute(id=new_id, id2=new_id2))

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': '', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"Test1","surname":"T1"},'
                                 '{"id":"${id2}",'
                                 '"name":"Test2","surname":"T1"}]}')
    assert compare_employees(
        response.text, response_required.substitute(id=new_id, id2=new_id2))


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_abscence_verdict(service_client):
    response = await service_client.post(
        '/v1/abscence/request',
        headers={'Authorization': 'Bearer first_token'},
        json={'type': 'vacation', 'start_date': '2023-07-10T00:00:00',
              'end_date': '2023-07-21T00:00:00'}
    )
    assert response.status == 200
    action_id = json.loads(response.text)['action_id']

    response = await service_client.post(
        '/v1/abscence/verdict',
        headers={'Authorization': 'Bearer first_token'},
        json={'action_id': action_id, 'approve': True}
    )

    assert response.status == 200

    await asyncio.sleep(1)

    response = await service_client.get(
        '/v1/documents/list',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    assert json.loads(response.text)[
        'documents'][0]['type'] == 'employee_request'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_upload_document(service_client):
    response = await service_client.post(
        '/v1/documents/upload',
        headers={'Authorization': 'Bearer first_token'},
        json={'extension': '.xlsx'}
    )

    assert response.status == 200
    response_json = json.loads(response.text)
    assert response_json["id"].endswith(".xlsx")
    assert response_json["url"].startswith(
        "https://working-day-documents.storage.yandexcloud.net")


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_inventory(service_client):
    response = await service_client.post(
        '/v1/inventory/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'employee_id': 'first_id', 'item': {
            'name': 'item1', 'description': 'desc1', 'id': 'id1'}}
    )

    assert response.status == 200

    response = await service_client.get(
        '/v1/employee/info',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert are_json_equal(response.text, (
        '{"id":"first_id","inventory":[{"description":"desc1","id":"id1","name":"item1"}],"name":"First","phones":[],"surname":"A"}'
    )) == True


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_end(service_client):
    response = await service_client.post(
        '/v1/clear-tasks',
    )

    assert response.status == 200
