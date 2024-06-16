FROM python:3.10

# set the working directory in the container
WORKDIR /app

# copy the dependencies file to the working directory
COPY requirements.txt .

# install dependencies
RUN pip install -r requirements.txt

RUN apt-get update && apt-get install -y libreoffice

RUN mkdir ./generate_document
COPY generate_document ./generate_document/
COPY main.py .

CMD [ "python", "./main.py" ]
