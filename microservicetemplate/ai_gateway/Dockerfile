FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        clang-tools-18 \
        cmake \
        default-libmysqlclient-dev \
        libcurl4-openssl-dev \
        librabbitmq-dev \
        git \
        ninja-build \
        python3-pip \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages --no-cache-dir cmake==3.29.6

WORKDIR /src/stasis

COPY microservicetemplate/stasis-src /src/stasis

RUN cmake -S /src/stasis -B /src/stasis/cmake-build-release -G Ninja \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="-O0 -DNDEBUG" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O0 -DNDEBUG"

RUN cmake --build /src/stasis/cmake-build-release --target compiler --parallel 1

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        default-libmysqlclient-dev \
        librabbitmq4 \
        libcurl4 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/stasis/bin/compiler /app/compiler
COPY microservicetemplate/stasis-src/grammar /src/stasis/grammar
COPY microservicetemplate/bin/stl /app/microservicetemplate/bin/stl
COPY microservicetemplate/ai_gateway /app/microservicetemplate/ai_gateway
COPY microservicetemplate/web /app/microservicetemplate/web
COPY microservicetemplate/ai_gateway/entrypoint.sh /app/entrypoint.sh

RUN chmod +x /app/entrypoint.sh

WORKDIR /app/microservicetemplate/ai_gateway

ENV AI_GATEWAY_PROVIDER_URL=https://api.openai.com/v1/chat/completions
ENV AI_GATEWAY_API_KEY=
ENV AI_GATEWAY_MODE=mock
ENV AI_GATEWAY_MODEL=qwen-plus
ENV AI_GATEWAY_HTTP_PORT=8084

EXPOSE 8084

ENTRYPOINT ["/app/entrypoint.sh"]
