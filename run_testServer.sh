#!/bin/bash

# 快速编译和运行 testServer

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo "=================================="
echo "  HTTP Server - testServer"
echo "=================================="

# 编译 testServer
echo ""
echo "[1] Compiling testServer..."
echo ""

g++ -std=c++17 -Wall -Wextra \
    src/http/HttpServer.cc \
    src/http/HttpRequest.cc \
    src/http/HttpResponse.cc \
    src/http/HttpContext.cc \
    src/router/Router.cc \
    src/middleware/MiddlewareChain.cc \
    src/middleware/cors/CorsMiddleware.cc \
    src/session/Session.cc \
    src/session/SessionManager.cc \
    src/session/SessionStorage.cc \
    src/ssl/SslContext.cc \
    src/ssl/SslConnection.cc \
    src/ssl/SslConfig.cc \
    src/utils/db/DbConnection.cc \
    src/utils/db/DbConnectionPool.cc \
    examples/testServer.cc \
    -I./include \
    -o testServer \
    -lmuduo_net -lmuduo_base -lssl -lcrypto -lpthread -lmysqlcppconn

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Compilation successful!"
    echo ""
    echo "=================================="
    echo "  Starting testServer..."
    echo "=================================="
    echo ""

    # 运行服务器
    ./testServer
else
    echo ""
    echo "✗ Compilation failed!"
    exit 1
fi
