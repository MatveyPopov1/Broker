FROM ubuntu:22.04 AS builder

RUN apt update && apt install -y \
    g++ \
    libssl-dev \
    make \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make

FROM ubuntu:22.04

RUN apt update && apt install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/broker .

EXPOSE 8080

CMD ["./broker"]
