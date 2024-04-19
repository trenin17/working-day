from fpdf import FPDF
import boto3
import json
import base64
from datetime import datetime

storage_client = None

def get_boto_session():
    boto_session = boto3.session.Session(
        aws_access_key_id='YCAJESGfHPZvOGYgqBjDkrLCZ',
        aws_secret_access_key='YCOFwp-4AxDaEEKeDBPx6YVuvU8Bqx-Z3hcQdIR8'
    )
    return boto_session

def get_storage_client():
    global storage_client
    if storage_client is not None:
        return storage_client

    storage_client = get_boto_session().client(
        service_name='s3',
        endpoint_url='https://storage.yandexcloud.net',
        region_name='ru-central1'
    )
    return storage_client

def upload_and_presign(file_path, object_name):
    client = get_storage_client()
    bucket = 'trenin17-results'
    client.upload_file(file_path, bucket, object_name)
    return client.generate_presigned_url('get_object', Params={'Bucket': bucket, 'Key': object_name}, ExpiresIn=3600)

def handler(event, context):
    file_key = event['queryStringParameters']['file_key']
    body = json.loads(base64.b64decode(event['body']))
    employee_name = body['employee_name']
    employee_surname = body['employee_surname']
    employee_patronymic = ''
    if 'employee_patronymic' in body:
        employee_patronymic = body['employee_patronymic']
    employee_position = ''
    if 'employee_position' in body:
        employee_position = body['employee_position']
    
    head_name = body['head_name']
    head_surname = body['head_surname']
    head_patronymic = ''
    if 'head_patronymic' in body:
        head_patronymic = body['head_patronymic']
    head_position = ''
    if 'head_position' in body:
        head_position = body['head_position']

    pdf = FPDF(orientation = 'P', unit = 'mm', format = 'A4')
    pdf.add_page()
    pdf.add_font('DejaVu', '', 'font/DejaVuSansCondensed.ttf', uni=True)
    pdf.set_font('DejaVu', '', 12)

    pdf.set_xy(120, 15)
    pdf.multi_cell(80, 7, "Кому: " + head_position + " " + head_surname + " " + head_name + " " + head_patronymic + "\nОт: " + employee_position + " " + employee_surname + " " + employee_name + " " + employee_patronymic)

    pdf.set_xy(90, 70)
    pdf.cell(15, 10, "ЗАЯВЛЕНИЕ")

    start_date = body['start_date']
    end_date = body['end_date']
    pdf.set_xy(10, 90)
    pdf.multi_cell(190, 7, "Прошу предоставить мне ежегодный оплачиваемый отпуск с " + start_date + " по " + end_date + ".")

    date = datetime.today().strftime('%d.%m.%Y')
    pdf.set_xy(10, 120)
    pdf.cell(20, 7, date)

    initials = ""
    if employee_patronymic == "":
        initials = employee_surname + " " + employee_name[0] + "."
    else:
        initials = employee_surname + " " + employee_name[0] + "." + employee_patronymic[0] + "."
    pdf.set_xy(130, 120)
    pdf.cell(70, 7, "______________________ / " + initials, align='R')

    file_key += '.pdf'
    file_name = '/tmp/' + file_key
    pdf.output(file_name)

    result = upload_and_presign(file_name, file_key)
    return {
        'statusCode': 200,
        'headers': {
            'Content-Type': 'text/plain'
        },
        'isBase64Encoded': False,
        'body': result
    }
