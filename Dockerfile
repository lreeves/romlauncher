FROM docker.io/devkitpro/devkita64:latest

WORKDIR /usr/src/project
COPY . /usr/src/project
CMD ["make"]
