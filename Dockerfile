FROM debian:stable-slim

RUN apt-get update
RUN apt-get install -y make
RUN apt-get install -y gcc
RUN apt-get install -y git
RUN apt-get install -y golang-go
RUN apt-get install -y pkgconf
RUN apt-get install -y libx11-xcb-dev
RUN apt-get install -y xvfb
