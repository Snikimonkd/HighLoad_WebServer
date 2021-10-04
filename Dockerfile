FROM ubuntu:latest
COPY httptest /home/sniki/http-test-suite/httptest/
RUN apt update -y && \
    apt upgrade -y && \
    apt install -y gcc
WORKDIR /app
RUN gcc -pthread -o main.o main.c
ENTRYPOINT [main.o]