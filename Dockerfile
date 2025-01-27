FROM docker.io/devkitpro/devkita64:latest

WORKDIR /usr/src/project

COPY . /usr/src/project

RUN apt update && apt install -y clang-tools-16

CMD ["make"]
