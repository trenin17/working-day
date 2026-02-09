import asyncio
from wsgiref import headers
import pytest
import json
import re
import websockets
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
    assert response.text == ('{"employees":['
                             '{"id":"first_id","name":"First","surname":"A"},'
                             '{"id":"stranger_id","name":"Stranger","surname":"S"},'
                             '{"id":"second_id","name":"Second","surname":"B"}]}')


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
        json={'name': 'Third', 'surname': 'C', 'role': 'user', 'job_position': 'worker'},
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
    assert response.text == ('{"id":"' + new_id + '","inventory":[],"job_position":"worker","name":"Third",'
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
        json={'email': 'second@mail.com', 'job_position': 'worker'},
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
    assert json.loads(response_body)['job_position'] == 'worker'


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
                                 '"name":"Fourth","surname":"D"}],"tasks":[]}')
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
                                 '"name":"Fourth","surname":"D"}],'
                                 '"tasks":[]}')
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
    assert response.text == ('{"employees":[],"tasks":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'E', 'limit': 1},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[],"tasks":[]}')


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
                             ',"name":"Second","surname":"B"}],'
                             '"tasks":[]}')

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
                             ',"name":"Second","surname":"B"}],'
                             '"tasks":[]}')

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
    assert response.text == ('{"employees":[],"tasks":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '+2222', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}],'
                             '"tasks":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '2@mail.com', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}],'
                             '"tasks":[]}')


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
                                 '"name":"Seventh","surname":"F"}],"tasks":[]}')
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
                                 '"name":"Eight","surname":"F"}],"tasks":[]}')
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
                                 '"name":"Eight","surname":"F"}],"tasks":[]}')
    assert compare_employees(response.text,
                             response_required.substitute(id=new_id,
                                                          id2=new_id3,
                                                          id3=new_id2))

'''
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

    # 21 июля с 10 до 18 тип work_nighttime для First
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-21T10:00:00',
              'end_date': '2023-07-21T18:00:00',
              'attendance_type': 'work_nighttime'}
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
        '"start_date":"2023-07-21T10:00:00.000000",'
        '"attendance_type":"work_nighttime"},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"}},'
        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}}'
        ']}')) == True

    #  22 июля с 10 до 18 тип additional_paid_vacation для First
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T10:00:00',
              'end_date': '2023-07-22T18:00:00',
              'attendance_type': 'additional_paid_vacation'}
    )
    assert response.status == 200

    #  22 июля с 9 до 17 тип study_vacation_paid для Second
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T9:00:00',
              'end_date': '2023-07-22T17:00:00',
              'attendance_type': 'study_vacation_paid'}
    )
    assert response.status == 200

    # Check overwrite the previous attendance
    #  22 июля с 10 до 18 тип childcare_leave для Second
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'second_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T10:00:00',
              'end_date': '2023-07-22T18:00:00',
              'attendance_type': 'childcare_leave'}
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
        '"start_date":"2023-07-22T10:00:00.000000",'
        '"attendance_type":"additional_paid_vacation"},'
        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-21T18:00:00.000000",'
        '"start_date":"2023-07-21T10:00:00.000000",'
        '"attendance_type":"work_nighttime"},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"},'
        '"end_date":"2023-07-22T18:00:00.000000",'
        '"start_date":"2023-07-22T10:00:00.000000",'
        '"attendance_type":"childcare_leave"},'
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
        '"start_date":"2023-07-22T10:00:00.000000",'
        '"attendance_type":"additional_paid_vacation"},'
        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"},'
        '"end_date":"2023-07-22T18:00:00.000000",'
        '"start_date":"2023-07-22T10:00:00.000000",'
        '"attendance_type":"childcare_leave"},'
        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}}'
        ']}')) == True

    # 22-23 июля sick_leave для First
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
    # 1-30 июля unpaid_vacation для Third
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
        '{"attendances":['
        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-21T18:00:00.000000","start_date":"2023-07-21T10:00:00.000000",'
        '"attendance_type":"work_nighttime"},'

        '{"employee":{"id":"second_id","name":"Second","subcompany":"first","surname":"B"},'
        '"end_date":"2023-07-22T18:00:00.000000","start_date":"2023-07-22T10:00:00.000000",'
        '"attendance_type":"childcare_leave"},'

        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"}},'

        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-22T23:59:00.000000","start_date":"2023-07-22T00:00:00.000000",'
        '"abscence_type":"sick_leave"},'

        '{"employee":{"id":"first_id","name":"First","subcompany":"first","surname":"A"},'
        '"end_date":"2023-07-23T23:59:00.000000","start_date":"2023-07-23T00:00:00.000000",'
        '"abscence_type":"sick_leave"},'

        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"},'
        '"end_date":"2023-07-21T23:59:00.000000","start_date":"2023-07-21T00:00:00.000000",'
        '"abscence_type":"unpaid_vacation"},'

        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"},'
        '"end_date":"2023-07-22T23:59:00.000000","start_date":"2023-07-22T00:00:00.000000",'
        '"abscence_type":"unpaid_vacation"},'

        '{"employee":{"id":"tc","name":"Third","subcompany":"first","surname":"C"},'
        '"end_date":"2023-07-23T23:59:00.000000","start_date":"2023-07-23T00:00:00.000000",'
        '"abscence_type":"unpaid_vacation"}'
        ']}'
    )) == True
'''

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

    # 21 июля с 10 до 18 тип work_nighttime для First
    response = await service_client.post(
        '/v1/attendance/add',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer ' + token},
        json={'start_date': '2023-07-22T10:00:00',
              'end_date': '2023-07-22T18:00:00',
              'attendance_type': 'work_nighttime'}
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/actions',
        headers={'Authorization': 'Bearer first_token'},
        json={'from': '2023-07-22T00:00:00', 'to': '2023-07-23T00:00:00'}
    )
    assert response.status == 200

    action_id = json.loads(response.text)['actions'][0]['id']
    assert response.text == (
        '{"actions":[{"attendance_type":"work_nighttime","blocking_actions_ids":[],'
        '"end_date":"2023-07-22T18:00:00.000000","id":"' + action_id + '",'
        '"start_date":"2023-07-22T10:00:00.000000","type":"attendance"'
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
    response_data = json.loads(response.text)
    expected_response = {
        "documents": [
            {"chain_metadata_new":[],
             "created_ts": response_data["documents"][0]["created_ts"],
             "description": "text1",
             "id": "id1",
             "name": "doc1",
             "visibility_status": 0,
             "sign_required": True,
             "signed": False,
             "type": "admin_request"},
            {"chain_metadata_new":[
                {"employee_id":"first_id","requires_signature":1,"status":0},
                {"employee_id":"second_id","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][1]["created_ts"],
             "description": "Test document with approval chain",
             "id": "doc_with_chain",
             "name": "Document with chain",
             "sign_required": True,
             "signed": False,
             "visibility_status": 0,
             "type": "admin_request"},
            {"chain_metadata_new":[
                {"employee_id":"first_id","requires_signature":1,"status":2},
                {"employee_id":"second_id","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][2]["created_ts"],
             "description": "",
             "id": "rejected_doc",
             "name": "Rejected document",
             "sign_required": True,
             "signed": False,
             "visibility_status": 0,
             "type": "admin_request"}
        ]
    }
    assert response_data == expected_response

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    created_time = json.loads(response.text)['notifications'][0]['created']
    id = json.loads(response.text)['notifications'][0]['id']
    assert response.text == (
        '{"notifications":[{"created":"' + created_time + '","id":"' + id + '","is_read":false,"sender":{"id":"tc","name":"Third","surname":"C"},"text":"Вам отправлен новый документ \\"doc1\\". Его можно просмотреть в разделе Документы.","type":"generic"}]}')

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer second_token'}
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
    response_data = json.loads(response.text)
    assert response_data == expected_response

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
    response_data = json.loads(response.text)
    expected_response = {
        "documents": [
            {"chain_metadata_new":[
                {"employee_id": "first_id","requires_signature": 1,"status": 0},
                {"employee_id": "second_id","requires_signature": 0,"status": 0}],
             "created_ts": response_data["documents"][0]["created_ts"],
             "description": "Test document with approval chain",
             "id": "doc_with_chain",
             "name": "Document with chain",
             "sign_required": True,
             "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new":[
                {"employee_id": "first_id", "requires_signature": 1, "status": 2},
                {"employee_id": "second_id","requires_signature": 0,"status": 0}],
             "created_ts": response_data["documents"][1]["created_ts"], "description": "", "id": "rejected_doc", "name": "Rejected document", "sign_required": True, "type": "admin_request", "visibility_status": 0},
            {"chain_metadata_new":[],
             "created_ts": response_data["documents"][2]["created_ts"], "description": "Document without approval chain", "id": "empty_chain_doc", "name": "Empty chain doc", "sign_required": False, "type": "admin_request", "visibility_status": 0},
            {"chain_metadata_new":[],
             "created_ts": response_data["documents"][3]["created_ts"], "description": "text1", "id": "id1", "name": "doc1", "sign_required": True, "type": "admin_request", "visibility_status": 0}
       ]
    }
    assert response_data == expected_response

    response = await service_client.get(
        '/v1/documents/get-signs',
        headers={'Authorization': 'Bearer ' + token},
        params={'document_id': 'id1'}
    )
    assert response.status == 200
    response_json = json.loads(response.text)
    assert len(response_json["signs"]) == 2
    expected_sign = {
        "document_id": "id1",
        "employee": {
            "id": "second_id",
            "name": "Second",
            "surname": "B"
        },
        "signed": False
    }
    assert response_json["signs"][0] == expected_sign
    assert response_json["signs"][1]["document_id"].endswith(".pdf")
    expected_sign = {
        "document_id": response_json["signs"][1]["document_id"],
        "employee": {
            "id": "first_id",
            "name": "First",
            "surname": "A"
        },
        "signed": True
    }
    assert response_json["signs"][1] == expected_sign


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
    assert response.text == '{"employees":[],"tasks":[]}'

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': 'T1 Test1 Bla', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == '{"employees":[],"tasks":[]}'

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
                                 '"name":"Test2","surname":"T1"}],'
                                 '"tasks":[]}')
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
                                 '"name":"Test2","surname":"T1"}],'
                                 '"tasks":[]}')
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
                                 '"name":"Test2","surname":"T1"}],'
                                 '"tasks":[]}')
    assert compare_employees(
        response.text, response_required.substitute(id=new_id, id2=new_id2))

'''
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
'''

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
async def test_tracker_projects_add_and_list(service_client):
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'project_name': 'new project1'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'project_name': 'new project2'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    data = response.json()
    expected_projects = [
        {
            "project_name": "first",
            "tasks_count": 2,
        },
        {
            "project_name": "second",
            "tasks_count": 0,
        },
        {
            "project_name": "new project1",
            "tasks_count": 0,
        },
        {
            "project_name": "new project2",
            "tasks_count": 0,
        },
    ]
    assert data["projects"] == expected_projects

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_bad_add(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'second task',
              'project_name': 'unknown',
              'description': 'description of the first task'},
    )
    assert response.status == 404
    assert response.text ==  '{"message":"Wrong project name"}'
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'second task',
              'project_name': 'first',
              'description': 'description of the first task',
              'assignee': 'unknown_id'},
    )
    assert response.status == 404
    assert response.text ==  '{"message":"Wrong assignee"}'

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_add_and_list(service_client):

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'task 1',
              'project_name': 'first',
              'description': 'in first project'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'task 2',
              'project_name': 'first'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'task 3',
              'project_name': 'second',
              'assignee': 'second_id'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    data = response.json()
    expected_tasks = [
        {
            "title": "old task",
            "project_name": "first",
            "id": "first-1",
            "creator": "first_id",
            "assignee": "stranger_id",
        },
        {
            "title": "young task",
            "project_name": "first",
            "id": "first-2",
            "creator": "second_id",
            "assignee": "stranger_id",
        },
        {
            "title": "task 1",
            "project_name": "first",
            "id": "first-3",
            "creator": "first_id",
        },
        {
            "title": "task 2",
            "project_name": "first",
            "id": "first-4",
            "creator": "first_id",
        },
        {
            "title": "task 3",
            "project_name": "second",
            "id": "second-1",
            "creator": "first_id",
            "assignee": "second_id",
        },
    ]
    assert data["tasks"] == expected_tasks

    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    data = response.json()
    expected_projects = [
        {
            "project_name": "first",
            "tasks_count": 4,
        },
        {
            "project_name": "second",
            "tasks_count": 1,
        },
    ]
    assert data["projects"] == expected_projects

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chat_create_and_list(service_client):
    create_response = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'test_name', 'id_list': ['first_id']}
    )

    assert create_response.status == 200
    create_response_json = json.loads(create_response.text)

    list_response = await service_client.post(
        '/v1/messenger/list-chats',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer first_token'}
    )

    assert list_response.status == 200
    assert list_response.text.find('test_name') > -1
    assert list_response.text.find(create_response_json['chat_id'])

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_load_recent_messages(service_client):
    response = await service_client.post(
        '/v1/messenger/recent-messages',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_id': 'chat1'}
    )

    assert response.status == 200
    messages = response.json()
    assert isinstance(messages, list)
    assert len(messages) == 1
    assert messages[0]['chat_id'] == 'chat1'
    assert messages[0]['sender_id'] == 'user1'
    assert messages[0]['content']['content'] == 'Hello world!'
    assert messages[0]['timestamp'] == '2025-02-25T10:00:00.000000'


# ============== Comprehensive Messenger Tests ==============

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_create_chat_basic(service_client):
    """Create a chat, verify response contains chat_id."""
    response = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'Basic Chat', 'id_list': ['first_id', 'second_id']},
    )
    assert response.status == 200
    data = response.json()
    assert 'chat_id' in data
    assert len(data['chat_id']) > 0


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_create_chat_and_list(service_client):
    """Create a chat, list chats for member, verify it appears with correct name."""
    create_resp = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'Listed Chat', 'id_list': ['first_id', 'second_id']},
    )
    assert create_resp.status == 200
    chat_id = create_resp.json()['chat_id']

    list_resp = await service_client.post(
        '/v1/messenger/list-chats',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert list_resp.status == 200
    chats = list_resp.json()['chats']
    created = next((c for c in chats if c['chat_id'] == chat_id), None)
    assert created is not None
    assert created['chat_name'] == 'Listed Chat'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_create_chat_multiple_members(service_client):
    """Create a group chat with 3 members, verify all can see it."""
    create_resp = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'Group Chat', 'id_list': ['first_id', 'second_id', 'stranger_id']},
    )
    assert create_resp.status == 200
    chat_id = create_resp.json()['chat_id']

    for employee_id in ['first_id', 'second_id', 'stranger_id']:
        resp = await service_client.post(
            '/v1/messenger/list-chats',
            params={'employee_id': employee_id},
            headers={'Authorization': 'Bearer first_token'},
        )
        assert resp.status == 200
        assert chat_id in [c['chat_id'] for c in resp.json()['chats']], \
            f'{employee_id} should see the group chat'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_list_chats_empty(service_client):
    """List chats for user with no chats, verify empty response."""
    response = await service_client.post(
        '/v1/messenger/list-chats',
        params={'employee_id': 'stranger_id'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.json()['chats'] == []


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_list_chats_with_last_message(service_client):
    """Verify list-chats includes last_message for chats with messages."""
    response = await service_client.post(
        '/v1/messenger/list-chats',
        params={'employee_id': 'first_id'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    chats = response.json()['chats']
    chat1 = next(c for c in chats if c['chat_id'] == 'chat1')
    last_msg = chat1['last_message']
    assert last_msg['sender_id'] == 'user1'
    assert last_msg['content']['content'] == 'Hello world!'
    assert 'timestamp' in last_msg


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_recent_messages_returns_proper_json(service_client):
    """Verify recent-messages returns proper JSON objects (not double-serialized)."""
    response = await service_client.post(
        '/v1/messenger/recent-messages',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_id': 'chat1'},
    )
    assert response.status == 200
    messages = response.json()
    assert isinstance(messages, list)
    assert len(messages) == 1
    msg = messages[0]
    # Verify proper JSON object structure (not escaped strings)
    assert isinstance(msg, dict)
    assert msg['chat_id'] == 'chat1'
    assert msg['sender_id'] == 'user1'
    assert isinstance(msg['content'], dict)
    assert msg['content']['content'] == 'Hello world!'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_recent_messages_empty_chat(service_client):
    """Create a new chat, load messages, verify empty (no blank message after fix)."""
    create_resp = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'Empty Chat', 'id_list': ['first_id']},
    )
    assert create_resp.status == 200
    chat_id = create_resp.json()['chat_id']

    response = await service_client.post(
        '/v1/messenger/recent-messages',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_id': chat_id},
    )
    assert response.status == 200
    result = response.json()
    # After fix: no blank message inserted on chat creation
    assert result is None or result == []


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_recent_messages_ordering(service_client, service_port):
    """Send multiple messages via WebSocket, verify DESC timestamp order."""
    create_resp = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'Order Test', 'id_list': ['first_id']},
    )
    assert create_resp.status == 200
    chat_id = create_resp.json()['chat_id']

    # Send 3 messages via WebSocket with small delays for distinct timestamps
    ws_url = f'ws://localhost:{service_port}/chat?token=first_token'
    async with websockets.connect(ws_url) as ws:
        for i in range(3):
            await ws.send(json.dumps({
                'chat_id': chat_id,
                'content': {'content': f'Message {i}'},
            }))
            await asyncio.wait_for(ws.recv(), timeout=2.0)
            await asyncio.sleep(0.05)

    # Verify DESC timestamp order
    response = await service_client.post(
        '/v1/messenger/recent-messages',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_id': chat_id},
    )
    assert response.status == 200
    messages = response.json()
    assert len(messages) == 3
    # Messages should be in DESC timestamp order (newest first)
    for i in range(len(messages) - 1):
        assert messages[i]['timestamp'] >= messages[i + 1]['timestamp']
    # Newest message should be "Message 2"
    assert messages[0]['content']['content'] == 'Message 2'
    assert messages[2]['content']['content'] == 'Message 0'


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_create_chat_unauthorized(service_client):
    """All messenger REST endpoints return 401 without auth token."""
    # create-chat
    resp = await service_client.post(
        '/v1/messenger/create-chat',
        json={'chat_name': 'No Auth', 'id_list': ['first_id']},
    )
    assert resp.status == 401

    # list-chats
    resp = await service_client.post(
        '/v1/messenger/list-chats',
        params={'employee_id': 'first_id'},
    )
    assert resp.status == 401

    # recent-messages
    resp = await service_client.post(
        '/v1/messenger/recent-messages',
        json={'chat_id': 'chat1'},
    )
    assert resp.status == 401


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_websocket_send_and_receive(service_client, service_port):
    """Connect via WebSocket, send a message, verify it's persisted via REST."""
    create_resp = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'WS Send Test', 'id_list': ['first_id', 'second_id']},
    )
    assert create_resp.status == 200
    chat_id = create_resp.json()['chat_id']

    # Send message via WebSocket
    ws_url = f'ws://localhost:{service_port}/chat?token=first_token'
    async with websockets.connect(ws_url) as ws:
        await ws.send(json.dumps({
            'chat_id': chat_id,
            'content': {'content': 'WebSocket hello!'},
        }))
        # Server echoes the message back (sender is also a chat member)
        echo = await asyncio.wait_for(ws.recv(), timeout=2.0)
        echo_data = json.loads(echo)
        assert echo_data['sender_id'] == 'first_id'
        assert echo_data['chat_id'] == chat_id

    # Verify message was persisted via REST
    response = await service_client.post(
        '/v1/messenger/recent-messages',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_id': chat_id},
    )
    assert response.status == 200
    messages = response.json()
    assert len(messages) == 1
    assert messages[0]['content']['content'] == 'WebSocket hello!'
    assert messages[0]['sender_id'] == 'first_id'
    assert messages[0]['chat_id'] == chat_id


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_websocket_broadcast(service_client, service_port):
    """Two users connect via WebSocket, one sends message, other receives it."""
    create_resp = await service_client.post(
        '/v1/messenger/create-chat',
        headers={'Authorization': 'Bearer first_token'},
        json={'chat_name': 'Broadcast Test', 'id_list': ['first_id', 'second_id']},
    )
    assert create_resp.status == 200
    chat_id = create_resp.json()['chat_id']

    ws_url_1 = f'ws://localhost:{service_port}/chat?token=first_token'
    ws_url_2 = f'ws://localhost:{service_port}/chat?token=second_token'

    async with websockets.connect(ws_url_1) as ws1, \
               websockets.connect(ws_url_2) as ws2:
        # User 1 sends a message
        await ws1.send(json.dumps({
            'chat_id': chat_id,
            'content': {'content': 'Hello from user 1!'},
        }))

        # User 1 gets echo back (they are a member too)
        echo = await asyncio.wait_for(ws1.recv(), timeout=2.0)
        echo_data = json.loads(echo)
        assert echo_data['sender_id'] == 'first_id'
        assert echo_data['content']['content'] == 'Hello from user 1!'

        # User 2 receives the broadcast
        broadcast = await asyncio.wait_for(ws2.recv(), timeout=2.0)
        broadcast_data = json.loads(broadcast)
        assert broadcast_data['sender_id'] == 'first_id'
        assert broadcast_data['content']['content'] == 'Hello from user 1!'
        assert broadcast_data['chat_id'] == chat_id


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_messenger_websocket_no_auth(service_port):
    """WebSocket connection without auth should be rejected."""
    ws_url = f'ws://localhost:{service_port}/chat'
    try:
        async with websockets.connect(ws_url) as ws:
            await ws.recv()
        pytest.fail("Connection should have been rejected without auth")
    except Exception:
        pass  # Expected: server rejects unauthenticated WebSocket


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_assigned_tasks_to_user (service_client):

    response = await service_client.post(
        '/v1/tracker/tasks/assigned-tasks-to-user',
        headers={'Authorization': 'Bearer first_token'},
        json={'employee_id': 'bad_user_id'},
    )
    # not found
    assert response.status == 404

    for i in range(1, 6):
        if i % 2:
            new_id = 'first_id'
        else:
            new_id = 'second_id'
        add_task_response = await service_client.post(
            '/v1/tracker/tasks/add',
            headers={'Authorization': 'Bearer first_token'},
            json={
                'title': f'Task{i}',
                'description': f'Description for task {i}',
                'project_name': 'second',
                'assignee': new_id,
            },
        )
        assert add_task_response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/assigned-to-user',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'second_id'},
    )
    assert response.status == 200

    data = response.json()

    tasks = data["tasks"]
    assert len(tasks) == 2
    assert tasks[0]["title"] == 'Task2'
    assert tasks[1]["title"] == 'Task4'

    response = await service_client.post(
        '/v1/tracker/tasks/assigned-to-user',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    data = response.json()

    tasks = data["tasks"]
    assert len(tasks) == 3
    assert tasks[0]["title"] == 'Task1'
    assert tasks[1]["title"] == 'Task3'
    assert tasks[2]["title"] == 'Task5'

    response = await service_client.post(
        '/v1/tracker/tasks/assigned-to-user',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'stranger_id'},
    )
    assert response.status == 200

    data = response.json()

    tasks = data["tasks"]
    assert len(tasks) == 2
    assert tasks[0]["title"] == 'old task'
    assert tasks[1]["title"] == 'young task'

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_info_and_edit(service_client):

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.text == (
        '{"assignee":"stranger_id",'
        '"created_ts":"2025-04-12T10:00:00.000000",'
        '"creator":"first_id",'
        '"deadline":"2025-04-12T10:00:00.000000",'
        '"description":"description for old task",'
        '"id":"first-1",'
        '"media_links":["s3 download test link","s3 download test link"],'
        '"project_name":"first",'
        '"status":"Open",'
        '"title":"old task"}')

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'first-1'},
        json={'description': 'new description for old task'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'first-1'},
        json={'status':'InProgress', 'title':'not old task'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    assert response.text == (
        '{"assignee":"stranger_id",'
        '"created_ts":"2025-04-12T10:00:00.000000",'
        '"creator":"first_id",'
        '"deadline":"2025-04-12T10:00:00.000000",'
        '"description":"new description for old task",'
        '"id":"first-1",'
        '"media_links":["s3 download test link","s3 download test link"],'
        '"project_name":"first",'
        '"status":"InProgress",'
        '"title":"not old task"}')

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_bad_params_info_edit(service_client):

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 400

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-3'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 404

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'first-3'},
        json={'description': 'new description'},
    )
    assert response.status == 404

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        json={'description': 'new description'},
    )
    assert response.status == 400

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_media_upload(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/media/upload',
        params={'task_id': 'first-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_json = json.loads(response.text)

    assert response_json["url"] == "s3 upload test link"

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response_json = json.loads(response.text)
    media_links = response_json["media_links"]
    assert len(media_links) == 3
    for i in range(3):
        assert media_links[i] == "s3 download test link"

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_search_add(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
                'title': 'task for searching',
                'project_name': 'first',
                'assignee': 'second_id',
             },
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={
                'search_key': 'task for searching',
                'limit': 1,
                'tags': ["tasks"],
             },
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[],"tasks":['
                                 '{"assignee":"second_id",'
                                 '"creator":"first_id",'
                                 '"id":"first-3",'
                                 '"project_name":"first",'
                                 '"title":"task for searching"}'
                                 ']}')
    assert response.text == response_required.substitute(id='first-3')

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_search_edit(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'title': 'Edit task',
                'project_name': 'second',
                'assignee': 'first_id',
             },
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        json={
                'search_key': 'edit',
                'limit': 1,
                'tags': ["tasks"],
             },
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[],"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"id":"second-1",'
                                 '"project_name":"second",'
                                 '"title":"Edit task"}'
                                 ']}')
    assert response.text == response_required.substitute(id='second-1')

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'second-1'},
        json={
                'title': 'updated',
             },
    )

    assert response.status == 200
    response = await service_client.post(
        '/v1/search/full',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'search_key': 'Edit task',
                'limit': 1,
                'tags': ["tasks"],
             },
    )
    assert response.status == 200
    assert response.text == '{"employees":[],"tasks":[]}'

    response = await service_client.post(
        '/v1/search/full',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'search_key': 'Updated task',
                'limit': 1,
                'tags': ["tasks"],
             },
    )

    assert response.status == 200
    response_required = Template('{"employees":[],"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"id":"second-1",'
                                 '"project_name":"second",'
                                 '"title":"updated"}'
                                 ']}')
    assert response.text == response_required.substitute(id='second-1')

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'title': 'task',
                'project_name': 'second',
                'assignee': 'first_id',
             },
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'search_key': 'Updated task',
                'limit': 2,
                'tags': ["tasks"],
             },
    )

    assert response.status == 200

    response_required = Template('{"employees":[],'
                                 '"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"id":"second-2",'
                                 '"project_name":"second",'
                                 '"title":"task"},'
                                 ''
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"id":"second-1",'
                                 '"project_name":"second",'
                                 '"title":"updated"}'
                                 ']}')
    assert response.text == response_required.substitute(id='second-1')

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_and_employees_search(service_client):
    response = await service_client.post(
        '/v1/employee/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'name': 'new', 'surname': 'D', 'role': 'user'},
    )
    assert response.status == 200
    new_id = json.loads(response.text)['login']

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
                'title': 'new',
                'project_name': 'second',
                'assignee': 'second_id',
             },
    )
    assert response.status == 200

    await asyncio.sleep(1)
    # basic search (tasks)
    response = await service_client.post(
        '/v1/search/basic',
        json={'search_key': 'new', 'tags':["tasks"]},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_required = Template('{"employees":[],'
                                 '"tasks":[{'
                                 '"assignee":"second_id",'
                                 '"creator":"first_id",'
                                 '"id":"second-1",'
                                 '"project_name":"second",'
                                 '"title":"new"}]}')
    assert response.text == response_required.substitute()
    # basic search (employees)
    response = await service_client.post(
        '/v1/search/basic',
        json={'search_key': 'new', 'tags':["employees"]},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"new","surname":"D"}],'
                                 '"tasks":[]}')
    assert compare_employees(
        response.text, response_required.substitute(id=new_id))
    # full search (all)
    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'new','limit':2, 'tags':["employees", "tasks"]},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_required = Template('{"employees":[{"id":"${id}",'
                                 '"name":"new","surname":"D"}],'
                                 '"tasks":[{'
                                 '"assignee":"second_id",'
                                 '"creator":"first_id",'
                                 '"id":"second-1",'
                                 '"project_name":"second",'
                                 '"title":"new"}]}')
    assert response.text == response_required.substitute(id=new_id)

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_bad_tag_search(service_client):
    response = await service_client.post(
        '/v1/search/full',
        json={
                'search_key': 'new',
                'limit': 2,
                'tags': "not array tag",
             },
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 500

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_add_with_deadline(service_client):

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'task 1',
              'project_name': 'first',
              'deadline': '2025-08-24T14:00:00.000000'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-3'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    expected_response = {
        "creator":"first_id",
        "deadline":"2025-08-24T14:00:00.000000",
        "id":"first-3",
        "media_links":[],
        "project_name":"first",
        "status":"Open",
        "title":"task 1"
    }
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}",  response_data["created_ts"])

    response_data.pop("created_ts")
    assert response_data == expected_response

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_edit_deadline(service_client):

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.text == (
        '{"assignee":"stranger_id",'
        '"created_ts":"2025-04-12T10:00:00.000000",'
        '"creator":"first_id",'
        '"deadline":"2025-04-12T10:00:00.000000",'
        '"description":"description for old task",'
        '"id":"first-1",'
        '"media_links":["s3 download test link","s3 download test link"],'
        '"project_name":"first",'
        '"status":"Open",'
        '"title":"old task"}')

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'first-1'},
        json={'deadline': '2025-08-24T14:00:00.000000'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'first-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    assert response.text == (
        '{"assignee":"stranger_id",'
        '"created_ts":"2025-04-12T10:00:00.000000",'
        '"creator":"first_id",'
        '"deadline":"2025-08-24T14:00:00.000000",'
        '"description":"description for old task",'
        '"id":"first-1",'
        '"media_links":["s3 download test link","s3 download test link"],'
        '"project_name":"first",'
        '"status":"Open",'
        '"title":"old task"}')

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_send_docx_document(service_client):
    response = await service_client.post(
        '/v1/documents/send',
        headers={'Authorization': 'Bearer first_token'},
        json={'employee_ids': ['first_id', 'second_id'], 'document': {
            'id': 'id1.docx', 'name': 'doc1',
            'description': 'text1', 'sign_required': True}}
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/documents/list',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    expected_response = {
        "documents": [
            {"chain_metadata_new":[],
             "created_ts": response_data["documents"][0]["created_ts"],
             "description": "text1",
             "id": "id1.pdf",
             "name": "doc1",
             "sign_required": True,
             "signed": False,
             "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new":[
                {"employee_id":"first_id","requires_signature":1,"status":0},
                {"employee_id":"second_id","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][1]["created_ts"],
             "description": "Test document with approval chain",
             "id": "doc_with_chain",
             "name": "Document with chain",
             "sign_required": True,
             "signed": False,
             "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new":[
                {"employee_id":"first_id","requires_signature":1,"status":2},
                {"employee_id":"second_id","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][2]["created_ts"],
             "description": "",
             "id": "rejected_doc",
             "name": "Rejected document",
             "sign_required": True,
             "signed": False,
             "type": "admin_request",
             "visibility_status": 0}
        ]
    }
    assert response_data == expected_response

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_approve(service_client):

    auth = 'Bearer first_token'
    appr_status = 0
    status = 0
    for _ in range(2):
        response = await service_client.post(
            '/v1/documents/chain/update',
            headers={'Authorization': auth},
            params={'document_id': 'doc_with_chain'},
            json={'approval_status': appr_status}
        )
        assert response.status == 200
        expected_response = {
            "chain_metadata_new": [
                {"employee_id": "first_id", "requires_signature": 1, "status": 1},
                {"employee_id": "second_id", "requires_signature": 0, "status": status}
            ]
        }
        response_data = json.loads(response.text)
        assert response_data == expected_response

        response = await service_client.get(
            '/v1/documents/get-signs',
            headers={'Authorization': 'Bearer first_token'},
            params={'document_id': 'doc_with_chain'}
        )
        assert response.status == 200
        response_data = json.loads(response.text)

        assert len(response_data["signs"]) == 2
        first_sign = next(s for s in response_data["signs"] if s["employee"]["id"] == "first_id")
        second_sign = next(s for s in response_data["signs"] if s["employee"]["id"] == "second_id")
        assert first_sign["signed"] is True
        assert second_sign["signed"] is False
        auth = 'Bearer second_token'
        appr_status = 2
        status = 1

    auth = 'Bearer first_token'
    for _ in range(2):

        response = await service_client.post(
            '/v1/notifications',
            headers={'Authorization': auth}
        )
        assert response.status == 200

        created_time0 = json.loads(response.text)['notifications'][0]['created']
        created_time1 = json.loads(response.text)['notifications'][1]['created']
        id0 = json.loads(response.text)['notifications'][0]['id']
        id1 = json.loads(response.text)['notifications'][1]['id']

        assert response.text == (
            '{"notifications":[{"created":"' + created_time0 + '","id":"' + id0 + '","is_read":false,"text":"Документ \'Document with chain\' был утвержден пользователем Second B.","type":"generic"}'
            ',{"created":"' + created_time1 + '","id":"' + id1 + '","is_read":false,"text":"Документ \'Document with chain\' был подписан и утвержден пользователем First A.","type":"generic"}]}')
        auth = 'Bearer second_token'

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_reject(service_client):
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'doc_with_chain'},
        json={'approval_status': 3}
    )
    assert response.status == 200

    expected_response = {
        "chain_metadata_new": [
            {"employee_id": "first_id", "requires_signature": 1, "status": 2},
            {"employee_id": "second_id", "requires_signature": 0, "status": 0}
        ]
    }
    assert json.loads(response.text) == expected_response

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    assert "был отклонен" in response.text

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_try_sign_non_signable(service_client):
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'doc_with_chain'},
        json={'approval_status': 0}
    )
    assert response.status == 200
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer second_token'},
        params={'document_id': 'doc_with_chain'},
        json={'approval_status': 0}
    )
    assert response.status == 400
    assert "Document doesn't require a signature" in response.text

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_wrong_order(service_client):
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer second_token'},
        params={'document_id': 'doc_with_chain'},
        json={'approval_status': 2}
    )
    assert response.status == 400
    assert "Not your turn to approve" in response.text

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_already_rejected(service_client):
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'rejected_doc'},
        json={'approval_status': 0}
    )
    assert response.status == 409
    assert "Document already rejected" in response.text
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer second_token'},
        params={'document_id': 'rejected_doc'},
        json={'approval_status': 2}
    )
    assert response.status == 409
    assert "Document already rejected" in response.text

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_empty_chain(service_client):
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'empty_chain_doc'},
        json={'approval_status': 2}
    )
    assert response.status == 409
    assert "No pending approval steps" in response.text

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_update_nonexistent_document(service_client):
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'nonexistent_doc'},
        json={'approval_status': 0}
    )
    assert response.status == 404
    assert "Document not found" in response.text

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_add(service_client):
    response = await service_client.post(
        '/v1/documents/chain/add',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'empty_chain_doc'},
        json={
            'chain_metadata_new': [
                {
                    'employee_id': 'first_id',
                    'requires_signature': 1,
                    'status': 0
                },
                {
                    'employee_id': 'second_id',
                    'requires_signature': 0,
                    'status': 0
                }
            ]
        }
    )
    assert response.status == 200
    response = await service_client.post(
        '/v1/documents/chain/update',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'empty_chain_doc'},
        json={'approval_status': 0}
    )
    assert response.status == 200
    expected_response = {
        "chain_metadata_new": [
            {"employee_id": "first_id", "requires_signature": 1, "status": 1},
            {"employee_id": "second_id", "requires_signature": 0, "status": 0},
        ]
    }
    response_data = json.loads(response.text)
    assert response_data == expected_response

    response = await service_client.get(
        '/v1/documents/get-signs',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'empty_chain_doc'}
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    assert len(response_data["signs"]) == 2
    first_sign = next(s for s in response_data["signs"] if s["employee"]["id"] == "first_id")
    second_sign = next(s for s in response_data["signs"] if s["employee"]["id"] == "second_id")
    assert first_sign["signed"] is True
    assert second_sign["signed"] is False

    auth = 'Bearer first_token'
    for _ in range(2):
        response = await service_client.post(
            '/v1/notifications',
            headers={'Authorization': auth}
        )
        assert response.status == 200

        created_time0 = json.loads(response.text)['notifications'][0]['created']
        id0 = json.loads(response.text)['notifications'][0]['id']

        created_time1 = json.loads(response.text)['notifications'][1]['created']
        id1 = json.loads(response.text)['notifications'][1]['id']

        assert response.text == (
            '{"notifications":[{"created":"'+ created_time0 + '","id":"' + id0 + '",'
            '"is_read":false,"text":"Документ \'Empty chain doc\' был подписан и утвержден пользователем First A.'
            '","type":"generic"},{"created":"'+ created_time1 +'","id":"' + id1 + '","is_read":false,'
            '"text":"Документ \'Empty chain doc\' был добавлен пользователем First A.","type":"generic"}]}')
        auth = 'Bearer second_token'

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_add_empty_metadata(service_client):
    response = await service_client.post(
        '/v1/documents/chain/add',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'empty_chain_doc'},
        json={
            'chain_metadata_new': []
        }
    )
    assert response.status == 400

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_add_nonexistent_document(service_client):
    response = await service_client.post(
        '/v1/documents/chain/add',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'nonexistent_doc'},
        json={
            'chain_metadata_new': [
                {
                    'employee_id': 'first_id',
                    'requires_signature': 1,
                    'status': 0
                }
            ]
        }
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_add_to_document_with_chain(service_client):
    response = await service_client.post(
        '/v1/documents/chain/add',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'doc_with_chain'},
        json={
            'chain_metadata_new': [
                {
                    'employee_id': 'first_id',
                    'requires_signature': 1,
                    'status': 0
                }
            ]
        }
    )
    assert response.status == 409

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_chain_add_with_invalid_employees(service_client):
    response = await service_client.post(
        '/v1/documents/chain/add',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'empty_chain_doc'},
        json={
            'chain_metadata_new': [
                {
                    'employee_id': 'invalid_id',
                    'requires_signature': 1,
                    'status': 0
                }
            ]
        }
    )
    assert response.status == 400


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_permissions_list(service_client):
    response = await service_client.post(
        '/v1/employee/permissions/list',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'first_id'},
    )
    assert response.status == 200
    assert response.json() == {
        'permissions': [
            {'permission_type': 'can_remove_documents', 'permission_value': 1},
        ]
    }
    response = await service_client.post(
        '/v1/employee/permissions/list',
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 400
    response = await service_client.post(
        '/v1/employee/permissions/list',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'unknown_id'},
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_permissions_set(service_client):
    response = await service_client.post(
        '/v1/employee/permissions/set',
        headers={'Authorization': 'Bearer second_token'},
        params={'employee_id': 'second_id'},
        json={
            'permissions': [
                {'permission_type': 'can_remove_documents', 'permission_value': 1},
            ]
        },
    )
    assert response.status == 403

    response = await service_client.post(
        '/v1/employee/permissions/set',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'second_id'},
        json={
            'permissions': [
                {'permission_type': 'can_remove_documents', 'permission_value': 1},
            ]
        },
    )
    assert response.status == 200
    assert response.json() == {
        'permissions': [
            {'permission_type': 'can_remove_documents', 'permission_value': 1},
        ]
    }
    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer second_token'}
    )
    assert response.status == 200

    created_time = json.loads(response.text)['notifications'][0]['created']
    id = json.loads(response.text)['notifications'][0]['id']
    assert response.text == (
        '{"notifications":[{"created":"'+ created_time + '","id":"' + id + '",'
        '"is_read":false,"text":"Ваши права были изменены пользователем First A.",'
        '"type":"generic"}]}')

    response = await service_client.post(
        '/v1/employee/permissions/list',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'second_id'},
    )
    assert response.status == 200
    assert response.json() == {
        'permissions': [
            {'permission_type': 'can_remove_documents', 'permission_value': 1},
        ]
    }
    response = await service_client.post(
        '/v1/employee/permissions/set',
        headers={'Authorization': 'Bearer first_token'},
        params={'employee_id': 'unknown'},
        json={
            'permissions': [
                {'permission_type': 'can_remove_documents', 'permission_value': 0},
            ]
        },
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_documents_archiving(service_client):
    response = await service_client.post(
        '/v1/documents/remove',
        headers={'Authorization': 'Bearer first_token'},
        json={'document_id': 'doc_with_chain', 'comment': 'archive for testing'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/documents/history',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'doc_with_chain'},
    )
    hist = response.json()['history']
    assert hist[-1]['action_type'] == 'archived'
    assert hist[-1]['comment'] == 'archive for testing'

    response = await service_client.post(
        '/v1/documents/restore',
        headers={'Authorization': 'Bearer second_token'},
        json={'document_id': 'doc_with_chain', 'comment': 'restoring for testing'},
    )
    assert response.status == 403

    response = await service_client.post(
        '/v1/documents/restore',
        headers={'Authorization': 'Bearer first_token'},
        json={'document_id': 'doc_with_chain', 'comment': 'restoring'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/documents/history',
        headers={'Authorization': 'Bearer first_token'},
        params={'document_id': 'doc_with_chain'},
    )
    hist = response.json()['history']
    assert hist[-1]['action_type'] == 'restored'
    assert hist[-1]['comment'] == 'restoring'


    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200

    created_time0 = json.loads(response.text)['notifications'][0]['created']
    id0 = json.loads(response.text)['notifications'][0]['id']
    created_time1 = json.loads(response.text)['notifications'][1]['created']
    id1 = json.loads(response.text)['notifications'][1]['id']

    assert response.text == (
        '{"notifications":[{"created":"'+ created_time0 + '","id":"' + id0 + '",'
        '"is_read":false,"text":"Документ \'Document with chain\' был восстановлен пользователем First A.",'
        '"type":"generic"},{"created":"'+ created_time1 + '","id":"' + id1 + '",'
        '"is_read":false,"text":"Документ \'Document with chain\' был удален пользователем First A.",'
        '"type":"generic"}]}')

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer second_token'}
    )
    assert response.status == 200

    created_time0 = json.loads(response.text)['notifications'][0]['created']
    id0 = json.loads(response.text)['notifications'][0]['id']
    created_time1 = json.loads(response.text)['notifications'][1]['created']
    id1 = json.loads(response.text)['notifications'][1]['id']

    assert response.text == (
        '{"notifications":[{"created":"'+ created_time0 + '","id":"' + id0 + '",'
        '"is_read":false,"text":"Документ \'Document with chain\' был восстановлен пользователем First A.",'
        '"type":"generic"},{"created":"'+ created_time1 + '","id":"' + id1 + '",'
        '"is_read":false,"text":"Документ \'Document with chain\' был удален пользователем First A.",'
        '"type":"generic"}]}')

    response = await service_client.post(
        '/v1/documents/remove',
        headers={'Authorization': 'Bearer second_token'},
        json={'document_id': 'doc_with_chain'},
    )
    assert response.status == 403

    response = await service_client.post(
        '/v1/documents/remove',
        headers={'Authorization': 'Bearer first_token'},
        json={'document_id': 'unknown'},
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_end(service_client):
    response = await service_client.post(
        '/v1/clear-tasks',
    )
    assert response.status == 200
