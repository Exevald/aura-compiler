FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        libmysqlclient-dev \
        librabbitmq-dev \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt /src/CMakeLists.txt
COPY main.cpp /src/main.cpp
COPY grammar.md /src/grammar.md
COPY src /src/src
COPY stdlib /src/stdlib
COPY tests /src/tests
COPY samples /src/samples
COPY microservicetemplate /src/microservicetemplate

RUN cmake -S /src -B /src/cmake-build-release -G Ninja \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="-O0 -DNDEBUG" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O0 -DNDEBUG"
RUN cmake --build /src/cmake-build-release --target aura-compiler --parallel 1

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        default-libmysqlclient-dev \
        librabbitmq4 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/cmake-build-release/aura-compiler /app/aura-compiler
COPY --from=build /src/grammar.md /app/grammar.md
COPY --from=build /src/stdlib /app/stdlib
COPY --from=build /src/microservicetemplate /app/microservicetemplate

WORKDIR /app/microservicetemplate

ENV HOST=0.0.0.0
ENV PORT=8082
ENV MYSQL_POOL_SIZE=4
ENV READ_TIMEOUT_MS=1000
ENV WRITE_TIMEOUT_MS=1000

EXPOSE 8082

CMD ["/app/aura-compiler", "run", "/app/microservicetemplate/cmd/main.aura"]
