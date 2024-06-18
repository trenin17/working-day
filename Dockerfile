FROM ghcr.io/userver-framework/ubuntu-22.04-userver-pg:latest

RUN apt-get install -y wget git libcurl4-openssl-dev libssl-dev uuid-dev zlib1g-dev libpulse-dev vim

RUN mkdir -p /app/src /app/configs /app/third_party /app/tests /app/scripts /app/postgresql/migrations

RUN git clone --recurse-submodules https://github.com/aws/aws-sdk-cpp
RUN mkdir sdk_build
WORKDIR "/sdk_build"
RUN cmake ../aws-sdk-cpp -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local/ -DCMAKE_INSTALL_PREFIX=/usr/local/ -DBUILD_ONLY="s3;sts"
RUN make
RUN make install

WORKDIR "/app"

RUN mkdir --parents ~/.postgresql ~/.aws /aws
RUN wget "https://storage.yandexcloud.net/cloud-certs/CA.pem" --output-document ~/.postgresql/root.crt
RUN chmod 0600 ~/.postgresql/root.crt

COPY aws /aws/ 
RUN cp -r /aws/* ~/.aws
COPY CMakeLists.txt Makefile Makefile.local /app
COPY configs /app/configs/
COPY src /app/src/
COPY third_party /app/third_party/
COPY tests /app/tests/
COPY scripts /app/scripts
COPY postgresql/migrations /app/postgresql/migrations

RUN make build-release

CMD build_release/working_day -c configs/static_config.yaml --config_vars configs/config_vars.yaml
