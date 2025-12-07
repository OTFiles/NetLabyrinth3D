#!/usr/bin/env python3
import websocket
import thread
import time
import json

def on_message(ws, message):
    print(f"收到消息: {message}")

def on_error(ws, error):
    print(f"错误: {error}")

def on_close(ws, close_status_code, close_msg):
    print("### 连接关闭 ###")

def on_open(ws):
    print("### 连接建立 ###")
    # 发送认证消息
    auth_msg = {
        "type": "auth",
        "playerId": "test_client_123",
        "playerName": "TestClient",
        "token": ""
    }
    ws.send(json.dumps(auth_msg))
    print("已发送认证消息")

if __name__ == "__main__":
    websocket.enableTrace(True)
    ws = websocket.WebSocketApp("ws://localhost:8081/",
                              on_open=on_open,
                              on_message=on_message,
                              on_error=on_error,
                              on_close=on_close)

    print("开始连接WebSocket服务器...")
    ws.run_forever()