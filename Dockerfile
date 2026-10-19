# Образ arenda: сборка в debian:trixie, запуск в debian:trixie-slim от непривилегированного пользователя.
# База, резервные копии и всё состояние - в томе /data. Пример запуска: docker-compose.yml.
FROM debian:trixie AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake libsqlite3-dev libssl-dev nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/usr \
    && cmake --build /build -j"$(nproc)" \
    && DESTDIR=/out cmake --install /build --strip

FROM debian:trixie-slim
# tzdata: время копий по расписанию берётся из TZ; ca-certificates: проверка сертификата DaData
RUN apt-get update \
    && apt-get install -y --no-install-recommends libsqlite3-0 libssl3t64 ca-certificates tzdata \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --home-dir /data --shell /usr/sbin/nologin arenda \
    && install -d -o arenda -g arenda -m 0750 /data
COPY --from=build /out/usr/bin/arenda /usr/bin/arenda
COPY --from=build /out/usr/share/arenda /usr/share/arenda
COPY --from=build /out/usr/share/doc/arenda /usr/share/doc/arenda
USER arenda
VOLUME /data
EXPOSE 8080
ENTRYPOINT ["/usr/bin/arenda", "--db", "/data/arenda.db", "--host", "0.0.0.0", "--port", "8080"]
