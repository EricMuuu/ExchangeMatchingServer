FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC
WORKDIR /code

RUN apt-get update && apt-get install -y \
    build-essential \
    libpqxx-dev \
    postgresql \
    libboost-all-dev \
    libtinyxml2-dev

ADD . /code/

RUN chmod u+r+x run.sh

RUN make clean && make

CMD ["./run.sh"]
