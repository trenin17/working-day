FROM ubuntu:latest

RUN mkdir -p /app/src /app/configs /app/third_party /app/tests

COPY CMakeLists.txt Makefile Makefile.local /app
COPY configs /app/configs/
COPY src /app/src/
COPY third_party /app/third_party/
COPY tests /app/tests/

WORKDIR "/app"

RUN apt-get update 
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y $(cat third_party/userver/scripts/docs/en/deps/ubuntu-22.04.md | tr '\n' ' ')
RUN apt-get install -y wget git

RUN mkdir --parents ~/.postgresql
RUN wget "https://storage.yandexcloud.net/cloud-certs/CA.pem" --output-document ~/.postgresql/root.crt
RUN chmod 0600 ~/.postgresql/root.crt

RUN make build-release

CMD build_release/working_day -c configs/static_config.yaml 
