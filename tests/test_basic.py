import asyncio
from calendar import c
from wsgiref import headers
import pytest
import json
import re
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
    assert response.text == ('{"has_nep":true,"id":"first_id","inventory":[],"name":"First",'
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
                             '{"id":"second_id","name":"Second","surname":"B"},'
                             '{"id":"third_id","name":"Third","surname":"C"},'
                             '{"id":"stranger_id","name":"Stranger","surname":"S"}]}')


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
    assert response.text == ('{"has_nep":false,"id":"' + new_id + '","inventory":[],"job_position":"worker","name":"Third",'
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
                                 '"name":"Fourth","surname":"D"}],"projects":[],"tasks":[]}')
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
                                 '"name":"Fourth","surname":"D"}],"projects":[],'
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
    assert response.text == ('{"employees":[],"projects":[],"tasks":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': 'E', 'limit': 1},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[],"projects":[],"tasks":[]}')


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
                             ',"name":"Second","surname":"B"}],"projects":[],'
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
                             ',"name":"Second","surname":"B"}],"projects":[],'
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
    assert response.text == ('{"employees":[],"projects":[],"tasks":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '+2222', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}],"projects":[],'
                             '"tasks":[]}')

    response = await service_client.post(
        '/v1/search/full',
        json={'search_key': '2@mail.com', 'limit': 1},
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    assert response.text == ('{"employees":[{"id":"second_id"'
                             ',"name":"Second","surname":"B"}],"projects":[],'
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
            'description': 'text1', 'sign_required': 1}}
    )
    assert response.status == 200

    # Автор (tc) тоже видит отправленный документ в списке с author_id = tc
    response = await service_client.get(
        '/v1/documents/list',
        headers={'Authorization': 'Bearer ' + token},
    )
    assert response.status == 200
    author_docs = json.loads(response.text)['documents']
    doc_id1_for_author = next((d for d in author_docs if d['id'] == 'id1'), None)
    assert doc_id1_for_author is not None, 'Автор должен видеть отправленный документ в списке'
    assert doc_id1_for_author['author_id'] == 'tc', 'У отправленного документа у автора author_id должен быть tc'

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
             "sign_required": 1,
             "signed": False,
             "author_id": "tc",
             "type": "admin_request"},
            {"chain_metadata_new":[
                {"employee_id":"first_id","employee_name":"A First","requires_signature":1,"signature_id":"sig_1","signature_path":"doc_with_chain_first_id_kep.p7s","signature_type":"kep","signed_at":response_data["documents"][1]["chain_metadata_new"][0].get("signed_at"),"status":0},
                {"employee_id":"second_id","employee_name":"B Second","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][1]["created_ts"],
             "description": "Test document with approval chain",
             "id": "doc_with_chain",
             "name": "Document with chain",
             "sign_required": 1,
             "signed": False,
             "visibility_status": 0,
             "type": "admin_request"},
            {"chain_metadata_new":[
                {"employee_id":"first_id","employee_name":"A First","requires_signature":1,"status":2},
                {"employee_id":"second_id","employee_name":"B Second","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][2]["created_ts"],
             "description": "",
             "id": "rejected_doc",
             "name": "Rejected document",
             "sign_required": 1,
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
    # Update signed_at from the actual response for second user
    expected_response["documents"][1]["chain_metadata_new"][0]["signed_at"] = response_data["documents"][1]["chain_metadata_new"][0].get("signed_at")
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
            {"author_id": "tc", "chain_metadata_new": [],
             "created_ts": response_data["documents"][0]["created_ts"],
             "description": "text1", "id": "id1", "name": "doc1", "sign_required": 1,
             "type": "admin_request", "visibility_status": 0},
            {"chain_metadata_new": [
                {"employee_id": "first_id", "requires_signature": 1, "status": 0},
                {"employee_id": "second_id", "requires_signature": 0, "status": 0}],
             "created_ts": response_data["documents"][1]["created_ts"],
             "description": "Test document with approval chain", "id": "doc_with_chain",
             "name": "Document with chain", "sign_required": 1, "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new": [],
             "created_ts": response_data["documents"][2]["created_ts"],
             "description": "Document without approval chain", "id": "empty_chain_doc",
             "name": "Empty chain doc", "sign_required": 0, "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new": [
                {"employee_id": "first_id", "requires_signature": 1, "status": 2},
                {"employee_id": "second_id", "requires_signature": 0, "status": 0}],
             "created_ts": response_data["documents"][3]["created_ts"],
             "description": "", "id": "rejected_doc", "name": "Rejected document",
             "sign_required": 1, "type": "admin_request", "visibility_status": 0},
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
    assert response_json["signs"][1] == expected_sign
    assert response_json["signs"][0]["document_id"].endswith(".pdf")
    expected_sign = {
        "document_id": response_json["signs"][0]["document_id"],
        "employee": {
            "id": "first_id",
            "name": "First",
            "surname": "A"
        },
        "signed": True
    }
    assert response_json["signs"][0] == expected_sign


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
    assert response.text == '{"employees":[],"projects":[],"tasks":[]}'

    response = await service_client.post(
        '/v1/search/suggest',
        json={'search_key': 'T1 Test1 Bla', 'limit': 5},
        headers={'Authorization': 'Bearer first_token'},
    )

    assert response.status == 200
    assert response.text == '{"employees":[],"projects":[],"tasks":[]}'

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
                                 '"name":"Test2","surname":"T1"}],"projects":[],'
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
                                 '"name":"Test2","surname":"T1"}],"projects":[],'
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
                                 '"name":"Test2","surname":"T1"}],"projects":[],'
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
        '{"has_nep":true,"id":"first_id","inventory":[{"description":"desc1","id":"id1","name":"item1"}],"name":"First","phones":[],"surname":"A"}'
    )) == True

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_projects_add_list_and_info(service_client):
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'new project1'},
    )
    assert response.status == 500

    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'new project1', 'project_key': 'MYPROJECTONE incorrect'},
    )
    assert response.status == 400
    assert response.text == '{"message":"Project key must contain only uppercase Latin letters"}'

    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'new project1', 'project_key': 'MYPROJECTONE'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    assert response_data["project_id"] == 'MYPROJECTONE', f"Expected MYPROJECTONE, got {response_data['project_id']}"


    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'MYPROJECTTWO',
            'title': 'new project2',
            'status': 'Closed',
            'description': 'desc proj2'
        },
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    assert response_data["project_id"] == 'MYPROJECTTWO', f"Expected MYPROJECTTWO, got {response_data["project_id"]}"


    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response_data = json.loads(response.text)

    assert len(response_data["projects"]) == 3
    expected_projects = [
        {
            "creator": 'first_id',
            "project_id": 'MYPROJECTTWO',
            "title": "new project2",
        },
        {
            "creator": 'first_id',
            "project_id": 'MYPROJECTONE',
            "title": "new project1",
        },
        {
            "creator": 'first_id',
            "project_id": "FIRST",
            "title": "first project name",
        },
    ]
    assert response_data["projects"] == expected_projects

    response = await service_client.get(
        '/v1/tracker/projects/info',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': 'MYPROJECTTWO'},
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    expected_project = {
        "assigned_users_ids": [],
        "creator": 'first_id',
        "project_id": 'MYPROJECTTWO',
        "title": "new project2",
        "description": "desc proj2",
        "tasks_count": 0,
        "created_ts": response_data["created_ts"],
        "last_updated_ts": response_data["last_updated_ts"],
        "status": "Closed",
    }
    assert response_data == expected_project

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_projects_add_and_edit(service_client):
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'MYPROJECTONE',
            'title': 'project1',
            'assigned_users_ids': ['first_id', 'stranger_id']
        },
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    project_id = response_data["project_id"]
    assert project_id == 'MYPROJECTONE', f"Expected MYPROJECTONE, got {project_id}"

    response = await service_client.get(
        '/v1/tracker/projects/info',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': project_id}
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    expected_project = {
        "assigned_users_ids": ['first_id', 'stranger_id'],
        "creator": 'first_id',
        "project_id": project_id,
        "title": "project1",
        "tasks_count": 0,
        "created_ts": response_data["created_ts"],
        "last_updated_ts": response_data["last_updated_ts"],
        "status": "Open",
    }
    assert response_data == expected_project


    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': project_id},
        json={
            'assigned_users_ids': ['second_id'],
            'title': 'edit_title',
            'description': 'description',
            'status': 'Pause',
        }
    )
    assert response.status == 200
    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    expected_projects = [
        {
            "creator": 'first_id',
            "project_id": project_id,
            "title": "edit_title",
        },
        {
            "creator": 'first_id',
            "project_id": "FIRST",
            "title": "first project name",
        }
    ]
    assert response_data["projects"] == expected_projects

    response = await service_client.get(
        '/v1/tracker/projects/info',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': project_id}
    )
    assert response.status == 200

    response_data = json.loads(response.text)

    expected_project = {
        "assigned_users_ids": ['second_id'],
        "description": "description",
        "creator": 'first_id',
        "project_id": project_id,
        "title": "edit_title",
        "tasks_count": 0,
        "created_ts": response_data["created_ts"],
        "last_updated_ts": response_data["last_updated_ts"],
        "status": "Pause",
    }

    assert response_data == expected_project


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_projects_list_no_duplicates_with_multiple_assigned_users(service_client):
    """Test that a project with multiple assigned users appears only once in the list for each user."""
    # Create a project with multiple assigned users
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'MULTIPROJ',
            'title': 'Project with multiple assigned users',
            'assigned_users_ids': ['second_id', 'stranger_id']
        },
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    project_id = response_data["project_id"]
    assert project_id == 'MULTIPROJ'

    # Verify project info shows both assigned users
    response = await service_client.get(
        '/v1/tracker/projects/info',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': project_id}
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    assert set(response_data["assigned_users_ids"]) == {'second_id', 'stranger_id'}

    # Check list for creator (first_id) - should see project once
    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    # Count occurrences of the project in the list
    project_ids = [p["project_id"] for p in response_data["projects"]]
    assert project_ids.count(project_id) == 1, f"Project {project_id} appears {project_ids.count(project_id)} times, expected 1"

    # Verify project is in the list
    project_found = next((p for p in response_data["projects"] if p["project_id"] == project_id), None)
    assert project_found is not None
    assert project_found["title"] == "Project with multiple assigned users"
    assert project_found["creator"] == "first_id"

    # Check list for first assigned user (second_id) - should see project once
    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    project_ids = [p["project_id"] for p in response_data["projects"]]
    assert project_ids.count(project_id) == 1, f"Project {project_id} appears {project_ids.count(project_id)} times for second_id, expected 1"

    # Verify project is in the list for assigned user
    project_found = next((p for p in response_data["projects"] if p["project_id"] == project_id), None)
    assert project_found is not None
    assert project_found["title"] == "Project with multiple assigned users"

    # Note: stranger_id doesn't have a token in test data, so we only test with second_id
    # Testing with one assigned user is sufficient to verify the fix for duplicate projects


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_projects_media_upload(service_client):
    response = await service_client.post(
        '/v1/tracker/projects/media/upload',
        params={'project_id': 'FIRST'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_json = json.loads(response.text)

    assert response_json["url"] == "s3 upload test link"

    response = await service_client.get(
        '/v1/tracker/projects/info',
        params={'project_id': 'FIRST'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response_json = json.loads(response.text)
    image_url = response_json["image_url"]
    assert image_url == "s3 download test link"

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_bad_add(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'title': 'second task',
            'project_id': 'unknown',
            'description': 'description of the first task'
        },
    )
    assert response.status == 404
    assert response.text ==  '{"message":"Project not found"}'
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'second task',
              'project_id': 'FIRST',
              'description': 'description of the first task',
              'assignee': 'unknown_id'},
    )
    assert response.status == 404
    assert response.text ==  '{"message":"Wrong assignee"}'

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_add_and_list(service_client):
    # Add third_id to FIRST project so they can be observer
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': 'FIRST'},
        json={'assigned_users_ids': ['third_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'title': 'task 1',
            'project_id': 'FIRST',
            'description': 'in first project',
        },
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'task 2',
              'project_id': 'FIRST',
              'observers': ['third_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer third_token'}
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    created_time0 = response_data['notifications'][0]['created']
    id0 = response_data['notifications'][0]['id']
    sender = {
        "id": "first_id",
        "name": "First",
        "surname": "A"
    }

    expected_data = {
        "notifications": [{
            "task_id": "FIRST-4",
            "created": created_time0,
            "id": id0,
            "is_read":False,
            "sender": sender,
            "text":"Вам доступна к наблюдению новая задача \"task 2\" в проекте \"first project name\".",
            "type":"generic",
        }],
    }
    assert response_data == expected_data

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'title': 'task 3',
              'project_id': 'SECOND',
              'assignee': 'second_id'},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer second_token'}
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    created_time0 = response_data['notifications'][0]['created']
    id0 = response_data['notifications'][0]['id']
    sender = {
        "id": "first_id",
        "name": "First",
        "surname": "A"
    }
    expected_data = {
        "notifications": [{
            "task_id": "SECOND-1",
            "created": created_time0,
            "id": id0,
            "is_read":False,
            "sender": sender,
            "text":"Вам назначена новая задача \"task 3\" в проекте \"second project name\".",
            "type":"generic",
        }],
    }
    assert response_data == expected_data

    response = await service_client.get(
        '/v1/tracker/tasks/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    data = response.json()
    expected_tasks = [
        {
            "title": "task 3",
            "project_id": "SECOND",
            "task_id": "SECOND-1",
            "creator": "first_id",
            "assignee": "second_id",
        },
        {
            "title": "task 2",
            "project_id": "FIRST",
            "task_id": "FIRST-4",
            "creator": "first_id",
        },
        {
            "title": "task 1",
            "project_id": "FIRST",
            "task_id": "FIRST-3",
            "creator": "first_id",
        },
        {
            "title": "old task",
            "project_id": "FIRST",
            "task_id": "FIRST-1",
            "creator": "first_id",
            "assignee": "stranger_id",
        },
        {
            "title": "young task",
            "project_id": "FIRST",
            "task_id": "FIRST-2",
            "creator": "second_id",
            "assignee": "stranger_id",
        }
    ]
    assert data["tasks"] == expected_tasks

    response = await service_client.get(
        '/v1/tracker/tasks/list',
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200
    data = response.json()
    expected_tasks = [
        {
            "title": "task 3",
            "project_id": "SECOND",
            "task_id": "SECOND-1",
            "creator": "first_id",
            "assignee": "second_id",
        },
        {
            "title": "young task",
            "project_id": "FIRST",
            "task_id": "FIRST-2",
            "creator": "second_id",
            "assignee": "stranger_id",
        },
    ]
    assert data["tasks"] == expected_tasks

    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    expected_projects = [
        {
            "creator": 'first_id',
            "project_id": "FIRST",
            "title": "first project name",
        },
    ]
    assert response_data["projects"] == expected_projects

    response = await service_client.get(
        '/v1/tracker/projects/list',
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    expected_projects = [
        {
            "creator": 'second_id',
            "project_id": "SECOND",
            "title": "second project name",
        },
    ]
    assert response_data["projects"] == expected_projects

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
    assert response.text == ('[\"{\\\"chat_id\\\":\\\"chat1\\\",\\\"content\\\":{\\\"content\\\":\\\"Hello world!\\\"},'
                             '\\\"sender_id\\\":\\\"user1\\\",\\\"timestamp\\\":\\\"2025-02-25T10:00:00.000000\\\"}\"]')

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_assigned_tasks_to_user (service_client):
    # Add first_id to SECOND project so they can be assigned tasks
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'project_id': 'SECOND'},
        json={'assigned_users_ids': ['first_id']},
    )
    assert response.status == 200

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
                'project_id': 'SECOND',
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
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_json = json.loads(response.text)
    expected_json = {
        "assignee": "stranger_id",
        "created_ts":"2025-04-12T10:00:00.000000",
        "creator":"first_id",
        "deadline":"2025-04-12T10:00:00.000000",
        "description":"description for old task",
        "media_links":["s3 download test link","s3 download test link"],
        "last_updated_ts": response_json['last_updated_ts'],
        "observers":[],
        "priority":"Low",
        "project_id":"FIRST",
        "status":"Open",
        "task_id":"FIRST-1",
        "title":"old task",
        'related_tasks_ids': [],
        'document_ids': [],
        'comments_ids': [],
    }
    assert response_json == expected_json

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'FIRST-1'},
        json={'description': 'new description for old task'},
    )
    assert response.status == 200

    # Add second_id to FIRST project so they can be observer
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': 'FIRST'},
        json={'assigned_users_ids': ['second_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'FIRST-1'},
        json={'status':'InProgress', 'title':'not old task', 'observers': ['second_id']},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response_json = json.loads(response.text)
    expected_json = {
        "assignee":"stranger_id",
        "created_ts":"2025-04-12T10:00:00.000000",
        "creator":"first_id",
        "deadline":"2025-04-12T10:00:00.000000",
        "description":"new description for old task",
        "media_links":["s3 download test link","s3 download test link"],
        "last_updated_ts": response_json['last_updated_ts'],
        "observers":['second_id'],
        "priority":"Low",
        "project_id":"FIRST",
        "status":"InProgress",
        "task_id":"FIRST-1",
        "title":"not old task",
        'related_tasks_ids': [],
        'document_ids': [],
        'comments_ids': [],
    }
    assert response_json == expected_json


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_bad_params_info_edit(service_client):

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 400

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-3'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 404

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'FIRST-3'},
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
async def test_tracker_projects_search_edit(service_client):
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer second_token'},
        json={'title': 'Edit project', 'project_key': 'MYPROJECTONE'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    project_id = response_data["project_id"]
    assert project_id == 'MYPROJECTONE', f"Expected MYPROJECTONE, got {project_id}"

    response = await service_client.post(
        '/v1/search/full',
        json={
                'search_key': 'edit',
                'limit': 1,
                'tags': ["projects"],
             },
        headers={'Authorization': 'Bearer second_token'},
    )

    assert response.status == 200
    response_required = Template('{"employees":[],"projects":['
                                 '{"creator":"second_id",'
                                 '"project_id":"${project_id}",'
                                 '"title":"Edit project"}],'
                                 '"tasks":[]}')
    assert response.text == response_required.substitute(project_id=project_id)

    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'project_id': project_id},
        json={'title': 'updated project'},
    )
    assert response.status == 200
    # Add first_id to SECOND project so they can be assigned
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'project_id': 'SECOND'},
        json={'assigned_users_ids': ['first_id']},
    )
    assert response.status == 200
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'title': 'Updated task',
                'project_id': 'SECOND',
                'assignee': 'first_id',
             },
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/search/full',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'search_key': 'Edit',
                'limit': 1,
                'tags': ["tasks", "projects"],
             },
    )
    assert response.status == 200
    assert response.text == '{"employees":[],"projects":[],"tasks":[]}'

    response = await service_client.post(
        '/v1/search/full',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'search_key': 'Updated',
                'limit': 2,
                'tags': ["tasks", "projects"],
             },
    )

    assert response.status == 200
    response_required = Template('{"employees":[],'
                                 '"projects":['
                                 '{"creator":"second_id",'
                                 '"project_id":"${project_id}",'
                                 '"title":"updated project"}],'
                                 '"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-1",'
                                 '"title":"Updated task"}'
                                 ']}')
    assert response.text == response_required.substitute(project_id=project_id)


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_media_upload(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/media/upload',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_json = json.loads(response.text)

    assert response_json["url"] == "s3 upload test link"

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
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
    # Add second_id to FIRST project so they can be assigned
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': 'FIRST'},
        json={'assigned_users_ids': ['second_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
                'title': 'task for searching',
                'project_id': 'FIRST',
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
    response_required = Template('{"employees":[],"projects":[],"tasks":['
                                 '{"assignee":"second_id",'
                                 '"creator":"first_id",'
                                 '"project_id":"FIRST",'
                                 '"task_id":"FIRST-3",'
                                 '"title":"task for searching"}'
                                 ']}')
    assert response.text == response_required.substitute(task_id='FIRST-3')

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_search_edit(service_client):
    # Add first_id to SECOND project so they can be assigned
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'project_id': 'SECOND'},
        json={'assigned_users_ids': ['first_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'title': 'Edit task',
                'project_id': 'SECOND',
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
    response_required = Template('{"employees":[],"projects":[],"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-1",'
                                 '"title":"Edit task"}'
                                 ']}')
    assert response.text == response_required.substitute(task_id='SECOND-1')

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'SECOND-1'},
        json={'title': 'updated'},
    )

    # проверяем уведомление о добавлении задачи
    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    created_time0 = response_data['notifications'][0]['created']
    id0 = response_data['notifications'][0]['id']

    created_time1 = response_data['notifications'][1]['created']
    id1 = response_data['notifications'][1]['id']
    sender = {
        "id": "second_id",
        "name": "Second",
        "surname": "B"
    }

    expected_data = {
        "notifications": [{
            "task_id": "SECOND-1",
            "created": created_time0,
            "id": id0,
            "is_read":False,
            "sender": sender,
            "text":"Изменена информация о задаче \"updated\" в проекте \"second project name\".",
            "type":"generic",
        },{
            "task_id": "SECOND-1",
            "created": created_time1,
            "id": id1,
            "is_read":False,
            "sender": sender,
            "text":"Вам назначена новая задача \"Edit task\" в проекте \"second project name\".",
            "type":"generic",
        }
        ],
    }
    assert response_data == expected_data

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
    assert response.text == '{"employees":[],"projects":[],"tasks":[]}'

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
    response_required = Template('{"employees":[],"projects":[],"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-1",'
                                 '"title":"updated"}'
                                 ']}')
    assert response.text == response_required.substitute(task_id='SECOND-1')

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
                'title': 'task',
                'project_id': 'SECOND',
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

    response_required = Template('{"employees":[],"projects":[],'
                                 '"tasks":['
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-2",'
                                 '"title":"task"},'
                                 ''
                                 '{"assignee":"first_id",'
                                 '"creator":"second_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-1",'
                                 '"title":"updated"}'
                                 ']}')
    assert response.text == response_required.substitute(task_id='SECOND-1')

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
                'project_id': 'SECOND',
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
    response_required = Template('{"employees":[],"projects":[],'
                                 '"tasks":[{'
                                 '"assignee":"second_id",'
                                 '"creator":"first_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-1",'
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
                                 '"name":"new","surname":"D"}],"projects":[],'
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
                                 '"name":"new","surname":"D"}],"projects":[],'
                                 '"tasks":[{'
                                 '"assignee":"second_id",'
                                 '"creator":"first_id",'
                                 '"project_id":"SECOND",'
                                 '"task_id":"SECOND-1",'
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
async def test_tracker_tasks_add_with_extra_fields(service_client):
    # Add second_id and third_id to FIRST project so they can be assigned/observers
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': 'FIRST'},
        json={'assigned_users_ids': ['second_id', 'third_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'title': 'task 1',
            'project_id': 'FIRST',
            'deadline': '2025-08-24T14:00:00.000000',
            'observers': ['second_id'],
            'related_tasks_ids': ['FIRST-1'],
            'assignee': 'third_id',
        },
    )
    assert response.status == 200

    # проверяем календарь
    response = await service_client.post(
        '/v1/actions',
        headers={'Authorization': 'Bearer third_token'},
        json={'from': '2025-08-20T14:00:00', 'to': '2025-08-26T14:00:00'}
    )
    assert response.status == 200

    action_id = json.loads(response.text)['actions'][0]['id']
    assert response.text == (
        '{"actions":[{"attendance_type":"tracker_task_deadline","blocking_actions_ids":[],'
        '"end_date":"2025-08-24T14:00:00.000000","id":"' + action_id + '",'
        '"start_date":"2025-08-24T14:00:00.000000","type":"attendance"'
        '}]}')

    # проверяем уведомление о добавлении задачи
    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer second_token'}
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    created_time0 = response_data['notifications'][0]['created']
    id0 = response_data['notifications'][0]['id']
    sender = {
        "id": "first_id",
        "name": "First",
        "surname": "A"
    }

    expected_data = {
        "notifications": [{
            "task_id": "FIRST-3",
            "created": created_time0,
            "id": id0,
            "is_read":False,
            "sender": sender,
            "text":"Вам доступна к наблюдению новая задача \"task 1\" в проекте \"first project name\", назначенная пользователю \"C Third\".",
            "type":"generic",
        }],
    }
    assert response_data == expected_data

    response = await service_client.post(
        '/v1/notifications',
        headers={'Authorization': 'Bearer third_token'}
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    created_time0 = response_data['notifications'][0]['created']
    id0 = response_data['notifications'][0]['id']
    sender = {
        "id": "first_id",
        "name": "First",
        "surname": "A"
    }

    expected_data = {
        "notifications": [{
            "task_id": "FIRST-3",
            "created": created_time0,
            "id": id0,
            "is_read":False,
            "sender": sender,
            "text":"Вам назначена новая задача \"task 1\" в проекте \"first project name\".",
            "type":"generic",
        }],
    }
    assert response_data == expected_data

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-3'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    expected_response = {
        "action_id": response_data["action_id"],
        "creator":"first_id",
        "deadline":"2025-08-24T14:00:00.000000",
        "task_id":"FIRST-3",
        "media_links":[],
        "project_id":"FIRST",
        "status":"Open",
        "title":"task 1",
        "last_updated_ts": response_data['last_updated_ts'],
        "observers": ['second_id'],
        "priority": 'Low',
        "related_tasks_ids": ['FIRST-1'],
        'assignee': 'third_id',
        'document_ids': [],
        'comments_ids': [],
    }
    assert re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}",  response_data["created_ts"])

    response_data.pop("created_ts")
    assert response_data == expected_response

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_tasks_edit_deadline(service_client):

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    expected_response = {
        "assignee":"stranger_id",
        "created_ts":"2025-04-12T10:00:00.000000",
        "creator":"first_id",
        "deadline":"2025-04-12T10:00:00.000000",
        "description":"description for old task",
        "last_updated_ts": response_data['last_updated_ts'],
        "media_links":["s3 download test link","s3 download test link"],
        "observers":[],
        "priority": 'Low',
        "project_id":"FIRST",
        'related_tasks_ids': [],
        "status":"Open",
        "task_id":"FIRST-1",
        "title":"old task",
        'document_ids': [],
        'comments_ids': [],
    }
    assert response_data == expected_response

    # Add second_id to FIRST project so they can be assigned
    response = await service_client.post(
        '/v1/tracker/projects/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'project_id': 'FIRST'},
        json={'assigned_users_ids': ['second_id']},
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/tracker/tasks/edit',
        headers={'Authorization': 'Bearer second_token'},
        params={'task_id': 'FIRST-1'},
        json={
            'deadline': '2025-09-24T14:00:00.000000',
            "related_tasks_ids": ['FIRST-2'],
            'assignee': 'second_id'
        },
    )
    assert response.status == 200

    response = await service_client.post(
        '/v1/actions',
        headers={'Authorization': 'Bearer second_token'},
        json={'from': '2025-08-20T14:00:00', 'to': '2025-08-26T14:00:00'}
    )
    assert response.status == 200
    assert response.text == ('{"actions":[]}')

    response = await service_client.post(
        '/v1/actions',
        headers={'Authorization': 'Bearer second_token'},
        json={'from': '2025-09-20T14:00:00', 'to': '2025-09-26T14:00:00'}
    )
    assert response.status == 200
    action_id = json.loads(response.text)['actions'][0]['id']
    assert response.text == (
        '{"actions":[{"attendance_type":"tracker_task_deadline","blocking_actions_ids":[],'
        '"end_date":"2025-09-24T14:00:00.000000","id":"' + action_id + '",'
        '"start_date":"2025-09-24T14:00:00.000000","type":"attendance"'
        '}]}')

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response_data = json.loads(response.text)
    expected_response = {
        "action_id": response_data["action_id"],
        "assignee":"second_id",
        "created_ts":"2025-04-12T10:00:00.000000",
        "creator":"first_id",
        "deadline":"2025-09-24T14:00:00.000000",
        "description":"description for old task",
        "last_updated_ts": response_data['last_updated_ts'],
        "media_links":["s3 download test link","s3 download test link"],
        "observers":[],
        "priority": 'Low',
        "project_id":"FIRST",
        "status":"Open",
        "task_id":"FIRST-1",
        "title":"old task",
        "related_tasks_ids": ['FIRST-2'],
        'document_ids': [],
        'comments_ids': [],
    }
    assert response_data == expected_response
    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-2'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    assert response_data['related_tasks_ids'] == ['FIRST-1']

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tracker_task_send_and_remove_docx_document(service_client):
    response = await service_client.post(
        '/v1/tracker/tasks/documents/send',
        headers={'Authorization': 'Bearer first_token'},
        params={'task_id': 'FIRST-1'},
        json={
            'document_id': 'id1.docx',
            'name': 'doc1',
            'description': 'text1'
        }
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    expected_response = {
        "document_ids": ['id1.pdf'],
        'comments_ids': [],
        "related_tasks_ids": [],
        "observers": [],
        "priority": 'Low',
        "project_id": "FIRST",
        "status": "Open",
        "task_id": "FIRST-1",
        "title": "old task",
        "last_updated_ts": response_data['last_updated_ts'],
        "created_ts": response_data['created_ts'],
        "description": "description for old task",
        "media_links": ["s3 download test link", "s3 download test link"],
        "deadline": "2025-04-12T10:00:00.000000",
        "assignee": "stranger_id",
        "creator": "first_id",
    }
    assert response_data == expected_response

    response = await service_client.post(
        '/v1/tracker/tasks/documents/remove',
        headers={'Authorization': 'Bearer first_token'},
        params={
            'task_id': 'FIRST-1',
            'document_id': 'id1.pdf'
        }
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    response_data = json.loads(response.text)
    assert response_data['document_ids'] == []

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_send_docx_document(service_client):
    response = await service_client.post(
        '/v1/documents/send',
        headers={'Authorization': 'Bearer first_token'},
        json={'employee_ids': ['first_id', 'second_id'], 'document': {
            'id': 'id1.docx', 'name': 'doc1',
            'description': 'text1', 'sign_required': 1}}
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
             "sign_required": 1,
             "signed": False,
             "author_id": "first_id",
             "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new":[
                {"employee_id":"first_id","employee_name":"A First","requires_signature":1,"signature_id":"sig_1","signature_path":"doc_with_chain_first_id_kep.p7s","signature_type":"kep","signed_at":response_data["documents"][1]["chain_metadata_new"][0].get("signed_at"),"status":0},
                {"employee_id":"second_id","employee_name":"B Second","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][1]["created_ts"],
             "description": "Test document with approval chain",
             "id": "doc_with_chain",
             "name": "Document with chain",
             "sign_required": 1,
             "signed": False,
             "type": "admin_request",
             "visibility_status": 0},
            {"chain_metadata_new":[
                {"employee_id":"first_id","employee_name":"A First","requires_signature":1,"status":2},
                {"employee_id":"second_id","employee_name":"B Second","requires_signature":0,"status":0}],
             "created_ts": response_data["documents"][2]["created_ts"],
             "description": "",
             "id": "rejected_doc",
             "name": "Rejected document",
             "sign_required": 1,
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
        json_data = {
            'approval_status': appr_status,
        }
        if appr_status == 0:
            json_data['signature_id'] = 'sig_1'

        response = await service_client.post(
            '/v1/documents/chain/update',
            headers={'Authorization': auth},
            params={'document_id': 'doc_with_chain'},
            json=json_data,
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
        json={
            'approval_status': 0,
            'signature_id': 'sig_1',
        }
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
                    'requires_signature': 2,
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
        json={
            'approval_status': 1,
            'signature_password': '123456',
        }
    )
    assert response.status == 200
    expected_response = {
        "chain_metadata_new": [
            {"employee_id": "first_id", "requires_signature": 2, "status": 1},
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
async def test_comments_add_remove(service_client):
    response = await service_client.post(
        '/v1/comments/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'data': 'test comment'},
    )
    assert response.status == 200
    comments_id = response.json()['comment_id']
    assert response.json() == {'comment_id': comments_id}

    response = await service_client.post(
        '/v1/comments/remove',
        headers={'Authorization': 'Bearer first_token'},
        params={'comment_id': comments_id},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': comments_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_comments_add_to_task(service_client):
    response = await service_client.post(
        '/v1/comments/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'data': 'test comment', 'task_id': 'FIRST-1'},
    )
    assert response.status == 200
    comment_id = response.json()['comment_id']
    assert response.json() == {'comment_id': comment_id}

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.json()['comments_ids'] == [comment_id]

    response = await service_client.post(
        '/v1/comments/remove',
        headers={'Authorization': 'Bearer first_token'},
        params={'comment_id': comment_id},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': 'FIRST-1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.json()['comments_ids'] == []

    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': comment_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_comments_edit_info(service_client):
    response = await service_client.post(
        '/v1/comments/edit',
        headers={'Authorization': 'Bearer first_token'},
        params={'comment_id': 'comment1'},
        json={'data': 'test comment 2'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': 'comment1'},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.json() == {
        'comment_id': 'comment1',
        'author_id': 'first_id',
        'data': 'test comment 2',
        'created_ts': response.json()['created_ts'],
        'last_updated_ts': response.json()['last_updated_ts'],
        'documents_ids': [],
    }

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_comments_access_author(service_client):
    """Test that comment author can access their own comment"""
    # Create a comment by first_id
    response = await service_client.post(
        '/v1/comments/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'data': 'comment by first_id', 'task_id': 'FIRST-1'},
    )
    assert response.status == 200
    comment_id = response.json()['comment_id']

    # Author should be able to access the comment
    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': comment_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.json()['author_id'] == 'first_id'
    assert response.json()['data'] == 'comment by first_id'

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_comments_access_assigned_user(service_client):
    """Test that assigned user can access comments in project tasks"""
    # Create a project with assigned user
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'TESTPROJ',
            'title': 'Test Project',
            'assigned_users_ids': ['second_id']
        },
    )
    assert response.status == 200
    project_id = response.json()['project_id']

    # Create a task in this project
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'title': 'Test Task',
            'project_id': project_id,
        },
    )
    assert response.status == 200
    task_id = response.json()['task_id']

    # Create a comment by first_id (not second_id)
    response = await service_client.post(
        '/v1/comments/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'data': 'comment in assigned project', 'task_id': task_id},
    )
    assert response.status == 200
    comment_id = response.json()['comment_id']

    # Assigned user (second_id) should be able to access the comment
    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': comment_id},
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200
    assert response.json()['comment_id'] == comment_id

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_comments_access_project_creator(service_client):
    """Test that project creator can access comments in project tasks"""
    # Create a project (first_id is creator)
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'CREATORPROJ',
            'title': 'Creator Project',
        },
    )
    assert response.status == 200
    project_id = response.json()['project_id']

    # Create a task in this project
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
            'title': 'Task by second',
            'project_id': project_id,
        },
    )
    assert response.status == 200
    task_id = response.json()['task_id']

    # Create a comment by second_id
    response = await service_client.post(
        '/v1/comments/add',
        headers={'Authorization': 'Bearer second_token'},
        json={'data': 'comment by second', 'task_id': task_id},
    )
    assert response.status == 200
    comment_id = response.json()['comment_id']

    # Project creator (first_id) should be able to access the comment
    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': comment_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200
    assert response.json()['comment_id'] == comment_id

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_comments_access_denied(service_client):
    """Test that user without access cannot see comments"""
    # Create a project without third_id in assigned_users
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'PRIVATEPROJ',
            'title': 'Private Project',
        },
    )
    assert response.status == 200
    project_id = response.json()['project_id']

    # Create a task in this project
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'title': 'Private Task',
            'project_id': project_id,
        },
    )
    assert response.status == 200
    task_id = response.json()['task_id']

    # Create a comment by first_id
    response = await service_client.post(
        '/v1/comments/add',
        headers={'Authorization': 'Bearer first_token'},
        json={'data': 'private comment', 'task_id': task_id},
    )
    assert response.status == 200
    comment_id = response.json()['comment_id']

    # third_id (not creator, not assigned, not author) should NOT be able to access
    response = await service_client.get(
        '/v1/comments/info',
        params={'comment_id': comment_id},
        headers={'Authorization': 'Bearer third_token'},
    )
    assert response.status == 404

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_tasks_access_within_project(service_client):
    """Test that users can access all tasks within their project"""
    # Create a project with assigned user
    response = await service_client.post(
        '/v1/tracker/projects/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'project_key': 'TASKPROJ',
            'title': 'Task Project',
            'assigned_users_ids': ['second_id']
        },
    )
    assert response.status == 200
    project_id = response.json()['project_id']

    # Create task 1 by first_id (creator)
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer first_token'},
        json={
            'title': 'Task 1',
            'project_id': project_id,
        },
    )
    assert response.status == 200
    task1_id = response.json()['task_id']

    # Create task 2 by second_id (assigned user)
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer second_token'},
        json={
            'title': 'Task 2',
            'project_id': project_id,
        },
    )
    assert response.status == 200
    task2_id = response.json()['task_id']

    # Create task 3 by third_id (not assigned, not creator)
    response = await service_client.post(
        '/v1/tracker/tasks/add',
        headers={'Authorization': 'Bearer third_token'},
        json={
            'title': 'Task 3',
            'project_id': project_id,
        },
    )
    assert response.status == 200
    task3_id = response.json()['task_id']

    # Project creator (first_id) should see all tasks
    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task1_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task2_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task3_id},
        headers={'Authorization': 'Bearer first_token'},
    )
    assert response.status == 200

    # Assigned user (second_id) should see all tasks
    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task1_id},
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task2_id},
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task3_id},
        headers={'Authorization': 'Bearer second_token'},
    )
    assert response.status == 200

    # third_id (not assigned, not creator) should NOT see tasks they didn't create
    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task1_id},
        headers={'Authorization': 'Bearer third_token'},
    )
    assert response.status == 404

    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task2_id},
        headers={'Authorization': 'Bearer third_token'},
    )
    assert response.status == 404

    # But third_id should see their own task
    response = await service_client.get(
        '/v1/tracker/tasks/info',
        params={'task_id': task3_id},
        headers={'Authorization': 'Bearer third_token'},
    )
    assert response.status == 200

@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_documents_list_signature_enrichment(service_client):
    response = await service_client.get(
        '/v1/documents/list',
        headers={'Authorization': 'Bearer first_token'}
    )
    assert response.status == 200
    response_data = json.loads(response.text)

    # doc_with_chain has a signature for first_id
    doc_with_chain = next(d for d in response_data["documents"] if d["id"] == "doc_with_chain")
    chain = doc_with_chain["chain_metadata_new"]

    # first_id has both employee_name and signature info
    first_item = next(c for c in chain if c["employee_id"] == "first_id")
    assert first_item["employee_name"] == "A First"
    assert first_item["signature_id"] == "sig_1"
    assert first_item["signature_path"] == "doc_with_chain_first_id_kep.p7s"
    assert "signed_at" in first_item

    # second_id has employee_name but no signature
    second_item = next(c for c in chain if c["employee_id"] == "second_id")
    assert second_item["employee_name"] == "B Second"
    assert "signature_id" not in second_item
    assert "signature_path" not in second_item
    assert "signed_at" not in second_item

    # rejected_doc has employee_names but no signatures
    rejected_doc = next(d for d in response_data["documents"] if d["id"] == "rejected_doc")
    for item in rejected_doc["chain_metadata_new"]:
        assert "employee_name" in item
        assert "signature_id" not in item

    # empty chain docs have no chain_metadata to enrich
    for doc in response_data["documents"]:
        if doc["chain_metadata_new"] == []:
            continue
        for item in doc["chain_metadata_new"]:
            assert "employee_name" in item


@pytest.mark.pgsql('db_1', files=['initial_data.sql'])
async def test_end(service_client):
    response = await service_client.post(
        '/v1/clear-tasks',
    )
    assert response.status == 200
