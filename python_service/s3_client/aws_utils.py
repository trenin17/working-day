import boto3

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

def download_file(file_key, path_to_file):
    storage_client = get_storage_client()
    bucket = 'working-day-documents'
    storage_client.download_file(bucket, file_key, path_to_file)

def upload_file(file_path, object_name):
    storage_client = get_storage_client()
    bucket = 'working-day-documents'
    storage_client.upload_file(file_path, bucket, object_name)

def upload_and_presign(file_path, object_name):
    client = get_storage_client()
    bucket = 'working-day-documents'
    client.upload_file(file_path, bucket, object_name)
    return client.generate_presigned_url('get_object', Params={'Bucket': bucket, 'Key': object_name}, ExpiresIn=3600)
