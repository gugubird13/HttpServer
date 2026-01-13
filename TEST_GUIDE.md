# testServer 快速测试指南

## 快速开始

### 1. 编译和运行服务器

```bash
cd /home/gugubird/projects/http_project/HttpServer
chmod +x run_testServer.sh
./run_testServer.sh
```

你会看到类似的输出：
```
==================================
  HTTP Server - Starting
==================================
[INFO] ✓ CORS Middleware configured
[INFO] ✓ Routes registered:
[INFO]   - GET  /
[INFO]   - GET  /api/status
[INFO]   - GET  /api/time
[INFO]   - POST /api/echo
[INFO]   - GET  /api/users
[INFO]   - POST /api/users
[INFO] ==================================
[INFO]   Server listening on 127.0.0.1:8080
[INFO] ==================================
```

### 2. 在另一个终端测试

#### 方式 A：使用浏览器
直接打开：http://127.0.0.1:8080/

#### 方式 B：使用 curl 测试不同的端点

**测试主页：**
```bash
curl http://127.0.0.1:8080/
```

**测试服务器状态：**
```bash
curl http://127.0.0.1:8080/api/status
```

**测试获取时间：**
```bash
curl http://127.0.0.1:8080/api/time
```

**测试回显功能（POST）：**
```bash
curl -X POST http://127.0.0.1:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"message": "Hello Server!"}'
```

**测试获取用户列表：**
```bash
curl http://127.0.0.1:8080/api/users
```

**测试创建用户：**
```bash
curl -X POST http://127.0.0.1:8080/api/users \
  -H "Content-Type: application/json" \
  -d '{"name": "John", "email": "john@example.com"}'
```

**测试 CORS 预检请求：**
```bash
curl -X OPTIONS http://127.0.0.1:8080/api/users \
  -H "Origin: http://localhost:3000" \
  -H "Access-Control-Request-Method: POST" \
  -v
```

#### 方式 C：带详细信息的测试
```bash
# 显示响应头和状态码
curl -i http://127.0.0.1:8080/api/status

# 显示完整的请求和响应过程
curl -v http://127.0.0.1:8080/api/time
```

---

## 测试清单

### ✅ 基本功能测试

- [ ] **GET 请求**：`curl http://127.0.0.1:8080/` 返回 HTML
- [ ] **多个路由**：测试 `/api/status`, `/api/time` 都能返回正确响应
- [ ] **POST 请求**：`curl -X POST /api/echo -d '...'` 能接收和回显数据
- [ ] **JSON 响应**：所有 API 返回有效的 JSON 格式
- [ ] **HTTP 状态码**：
  - [ ] GET 返回 200
  - [ ] POST 返回 201
  - [ ] 404 返回正确的状态码

### ✅ CORS 测试

- [ ] **CORS Header**：响应中包含 `Access-Control-Allow-Origin`
- [ ] **OPTIONS 预检**：预检请求返回 204 或 200
- [ ] **多源支持**：跨域请求被正确处理

### ✅ 请求头和响应头

- [ ] **Content-Type**：响应正确设置 `application/json` 或 `text/html`
- [ ] **自定义头**：可以在响应中看到正确的 Header

### ✅ 并发测试

```bash
# 测试是否能处理多个并发请求
for i in {1..10}; do
  curl http://127.0.0.1:8080/api/status &
done
wait
```

---

## 常见测试场景

### 场景 1：完整的 HTTP 交互流程

```bash
# 1. 访问主页
curl http://127.0.0.1:8080/

# 2. 获取服务器状态
curl http://127.0.0.1:8080/api/status | json_pp

# 3. 获取当前时间
curl http://127.0.0.1:8080/api/time

# 4. 发送 POST 请求
curl -X POST http://127.0.0.1:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"test": "data"}'

# 5. 查看日志输出（在服务器窗口应该能看到对应的 LOG_INFO）
```

### 场景 2：CORS 跨域测试

```bash
# 模拟浏览器的跨域请求
curl -X OPTIONS http://127.0.0.1:8080/api/users \
  -H "Origin: http://example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: Content-Type" \
  -v

# 预期看到以下响应头：
# < HTTP/1.1 204 No Content
# < Access-Control-Allow-Origin: *
# < Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
# < Access-Control-Allow-Headers: Content-Type, Authorization
```

### 场景 3：JSON 数据交互

```bash
# 发送复杂的 JSON 数据
curl -X POST http://127.0.0.1:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{
    "user": {
      "name": "Alice",
      "age": 30,
      "emails": ["alice@example.com", "alice2@example.com"]
    },
    "timestamp": '$(date +%s)'
  }' \
  | json_pp  # 格式化 JSON 输出（如果装了 json_pp）
```

---

## 故障排查

### 问题 1：连接被拒绝

```bash
curl: (7) Failed to connect to 127.0.0.1 port 8080
```

**解决**：
- 确认服务器已启动（应该看到 "Server listening" 的日志）
- 检查防火墙是否阻止了 8080 端口
- 用 `lsof -i :8080` 查看是否有进程占用

### 问题 2：400 Bad Request

```
HTTP/1.1 400 Bad Request
```

**解决**：
- 检查请求格式是否正确（HTTP 方法、头部、URL 等）
- 查看服务器日志了解具体的解析错误

### 问题 3：500 Internal Server Error

```
HTTP/1.1 500 Internal Server Error
```

**解决**：
- 查看服务器的日志输出 (`LOG_ERROR`)
- 检查路由处理函数是否有异常

### 问题 4：404 Not Found

```
HTTP/1.1 404 Not Found
```

**解决**：
- 检查请求的路径是否注册了（查看服务器启动时的 `Routes registered` 列表）
- 确认 HTTP 方法正确（GET/POST 等）

---

## 高级测试

### 压力测试

```bash
# 安装 Apache Bench（如果没有）
sudo apt install apache2-utils

# 1000 个请求，10 个并发连接
ab -n 1000 -c 10 http://127.0.0.1:8080/api/status

# 显示更详细的统计信息
ab -n 1000 -c 10 -t 30 http://127.0.0.1:8080/
```

预期输出包括：
- Requests per second（吞吐量）
- Time per request（平均延迟）
- Failed requests（失败请求数，应该为 0）

### 使用 Python 测试

```python
import requests
import json

# 测试 GET
response = requests.get('http://127.0.0.1:8080/api/status')
print(response.status_code)
print(response.json())

# 测试 POST
data = {'name': 'test', 'value': 123}
response = requests.post('http://127.0.0.1:8080/api/echo', json=data)
print(response.json())

# 测试 CORS
headers = {'Origin': 'http://example.com'}
response = requests.options('http://127.0.0.1:8080/api/users', headers=headers)
print(response.headers)
```

---

## 预期结果总结

✅ **你应该能看到**：
- 服务器正常启动，监听在 8080 端口
- 各种 HTTP 请求都能正确路由和处理
- JSON 响应格式正确
- CORS Header 被正确设置
- 并发请求能被正常处理
- 服务器日志显示各个请求的处理过程

✅ **如果一切正常**：
- 你的 HTTP Server 实现是可用的！
- 可以继续添加更多功能（数据库、认证等）

---

## 下一步

1. **添加更多路由**：根据实际需求扩展 API
2. **集成数据库**：启用 DbConnectionPool
3. **添加身份验证**：实现 JWT Token 验证
4. **性能优化**：根据压力测试结果调优
5. **生产部署**：考虑 Nginx 反向代理、负载均衡等

祝测试顺利！🚀
