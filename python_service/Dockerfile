FROM python:3.10

# set the working directory in the container
WORKDIR /app

# copy the dependencies file to the working directory
COPY requirements.txt .

# install dependencies
RUN pip install -r requirements.txt

RUN apt-get update && apt-get install -y libreoffice

RUN mkdir -p /app/generate_document /app/sign_document /app/generate_from_template /tmp/logs

COPY generate_document ./generate_document/
COPY main.py .
COPY sign_document ./sign_document
COPY convert_document ./convert_document
COPY attendance_export_to_excel ./attendance_export_to_excel
COPY generate_from_template ./generate_from_template
COPY s3_client ./s3_client
COPY start_service.sh .

CMD [ "./start_service.sh" ]
