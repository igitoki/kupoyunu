FROM ubuntu:latest
RUN apt-get update && apt-get install -y g++ curl
COPY . /app
WORKDIR /app
RUN g++ server.cpp -o server
CMD ["./server"]
