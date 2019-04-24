FROM debian:stable-slim

RUN apt-get update
RUN apt-get install -y make gcc pkgconf libx11-xcb-dev xvfb
