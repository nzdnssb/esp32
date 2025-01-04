#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <FS.h>

const char* ssid = "ESP32-FileServer";
const char* password = "12345678";

WebServer server(80);

// 获取文件系统信息
String getFileSystemInfo() {
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();

  String info = "存储信息:\n";
  info += "总容量: " + String(totalBytes / 1024.0, 2) + " KB\n";
  info += "已使用: " + String(usedBytes / 1024.0, 2) + " KB\n";
  info += "剩余空间: " + String((totalBytes - usedBytes) / 1024.0, 2) + " KB\n";
  return info;
}

// 在文件开头添加新的处理函数声明
void handleDelete();
void handleFormat();  // 添加格式化处理函数声明
void handleDownload();  // 添加下载处理函数声明
void handleView();      // 添加查看处理函数声明
void handleEditor();
void handleSave();

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS挂载失败");
    return;
  }
  
  Serial.println("SPIFFS挂载成功");
  Serial.println(getFileSystemInfo());
  
  // 检查文件系统
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println("\n当前存在的文件：");
  while (file) {
    Serial.printf("- %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }
  root.close();

  // 设置AP模式
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("WiFi接入点已启动");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP地址: ");
  Serial.println(WiFi.softAPIP());  // 默认IP通常是192.168.4.1

  // 设置服务器路由
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, []() {
    // 这个回调在上传完成后被调用
  }, handleUpload);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/format", HTTP_POST, handleFormat);  // 添加格式化路由
  server.on("/download", HTTP_GET, handleDownload);  // 添加下载路由
  server.on("/view", HTTP_GET, handleView);         // 添加查看路由
  server.on("/editor", HTTP_GET, handleEditor);  // 添加编辑器路由
  server.on("/save", HTTP_POST, handleSave);     // 添加保存路由

  server.begin();
}

void loop() {
  server.handleClient();
}

// 处理根路径请求
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h2 { color: #333; text-align: center; margin-bottom: 20px; }";
  html += "pre { background: #f8f9fa; padding: 15px; border-radius: 4px; white-space: pre-wrap; word-wrap: break-word; }";
  html += ".upload-form { margin: 20px 0; text-align: center; }";
  html += "input[type='file'] { display: block; margin: 10px auto; max-width: 100%; }";
  html += "input[type='submit'] { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; }";
  html += "input[type='submit']:hover { background: #0056b3; }";
  html += "a { display: block; text-align: center; color: #007bff; text-decoration: none; margin-top: 10px; }";
  html += "a:hover { text-decoration: underline; }";
  html += ".format-btn { background: #dc3545; color: white; border: none; padding: 10px 20px; ";
  html += "border-radius: 4px; cursor: pointer; margin-top: 20px; }";
  html += ".format-btn:hover { background: #c82333; }";
  html += "</style>";
  html += "<script>";
  html += "function formatFS() {";
  html += "  if (confirm('警告：格式化将删除所有文件！确定要继续吗？')) {";
  html += "    const form = document.createElement('form');";
  html += "    form.method = 'POST';";
  html += "    form.action = '/format';";
  html += "    document.body.appendChild(form);";
  html += "    form.submit();";
  html += "  }";
  html += "}";
  html += "</script>";
  html += "</head><body><div class='container'>";
  html += "<h2>ESP32文件存储服务器</h2>";
  html += "<pre>" + getFileSystemInfo() + "</pre>";
  html += "<div class='upload-form'>";
  html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
  html += "<input type='file' name='file'>";
  html += "<input type='submit' value='上传文件'>";
  html += "</form></div>";
  html += "<a href='/list'>查看文件列表</a>";
  html += "<a href='/editor'>在线记事本</a>";
  html += "<button class='format-btn' onclick='formatFS()'>格式化文件系统</button>";
  html += "</div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

// 处理文件上传
void handleUpload() {
  if (server.uri() != "/upload") {
    server.send(500, "text/plain; charset=utf-8", "错误的上传路径");
    return;
  }
  
  HTTPUpload& upload = server.upload();
  static File file;

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;  // 确保文件名以/开头
    }
    
    Serial.printf("开始上传: %s\n", filename.c_str());
    
    // 如果文件已存在，先删除
    if (SPIFFS.exists(filename)) {
      SPIFFS.remove(filename);
    }
    
    file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("无法创建文件");
      return;
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE && file) {
    // 分块写入，每次写入后让出一些时间给系统
    size_t written = file.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      Serial.println("写入错误");
    }
    yield();  // 让出CPU时间给其他任务
  } 
  else if (upload.status == UPLOAD_FILE_END && file) {
    Serial.printf("上传完成: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
    file.close();
    
    // 发送上传成功页面
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<meta http-equiv='refresh' content='2;url=/'>";  // 2秒后自动返回首页
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;text-align:center;}";
    html += ".box{background:white;max-width:600px;margin:20px auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "h2{color:#28a745;}";
    html += "</style></head><body>";
    html += "<div class='box'>";
    html += "<h2>✓ 上传成功</h2>";
    html += "<p>文件名: " + String(upload.filename) + "</p>";
    html += "<p>大小: " + String(upload.totalSize / 1024.0, 2) + " KB</p>";
    html += "<p>2秒后自动返回...</p>";
    html += "</div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("上传被中止");
    if (file) {
      file.close();
      // 删除未完成的文件
      SPIFFS.remove(file.name());
    }
  }
}

// 添加删除文件的处理函数
void handleDelete() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain; charset=utf-8", "缺少文件名参数");
    return;
  }
  
  String filename = server.arg("file");
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  
  if (SPIFFS.exists(filename)) {
    if (SPIFFS.remove(filename)) {
      server.sendHeader("Location", "/list");
      server.send(303);
    } else {
      server.send(500, "text/plain; charset=utf-8", "删除失败");
    }
  } else {
    server.send(404, "text/plain; charset=utf-8", "文件不存在");
  }
}

// 修改文件列表显示函数
void handleFileList() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h2 { color: #333; text-align: center; margin-bottom: 20px; }";
  html += "pre { background: #f8f9fa; padding: 15px; border-radius: 4px; white-space: pre-wrap; word-wrap: break-word; }";
  html += ".file-list { margin: 20px 0; }";
  html += ".file-item { display: flex; justify-content: space-between; align-items: center; padding: 10px; border-bottom: 1px solid #eee; }";
  html += ".file-info { flex-grow: 1; }";
  html += ".delete-btn { background: #dc3545; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; margin-left: 10px; }";
  html += ".delete-btn:hover { background: #c82333; }";
  html += ".no-files { text-align: center; color: #666; padding: 20px; }";
  html += "a.back { display: block; text-align: center; color: #007bff; text-decoration: none; margin-top: 20px; }";
  html += "a.back:hover { text-decoration: underline; }";
  html += ".btn-group { display: flex; gap: 5px; }";
  html += ".view-btn { background: #28a745; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; }";
  html += ".download-btn { background: #17a2b8; color: white; border: none; padding: 5px 10px; border-radius: 4px; cursor: pointer; }";
  html += ".view-btn:hover { background: #218838; }";
  html += ".download-btn:hover { background: #138496; }";
  html += "</style>";
  html += "<script>";
  html += "function deleteFile(filename) {";
  html += "  if (confirm('确定要删除文件 ' + filename + ' 吗？')) {";
  html += "    const form = document.createElement('form');";
  html += "    form.method = 'POST';";
  html += "    form.action = '/delete';";
  html += "    const input = document.createElement('input');";
  html += "    input.type = 'hidden';";
  html += "    input.name = 'file';";
  html += "    input.value = filename;";
  html += "    form.appendChild(input);";
  html += "    document.body.appendChild(form);";
  html += "    form.submit();";
  html += "  }";
  html += "}";
  html += "</script>";
  html += "</head><body><div class='container'>";
  html += "<h2>文件列表</h2>";
  html += "<pre>" + getFileSystemInfo() + "</pre>";
  html += "<div class='file-list'>";

  bool hasFiles = false;
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  
  Serial.println("\n正在获取文件列表：");

  while (file) {
    hasFiles = true;
    String fileName = String(file.name());
    size_t fileSize = file.size();
    
    // 打印调试信息
    Serial.printf("找到文件: %s (%u bytes)\n", fileName.c_str(), fileSize);
    
    html += "<div class='file-item'>";
    html += "<div class='file-info'>" + fileName + " - " + String(fileSize / 1024.0, 2) + " KB</div>";
    html += "<div class='btn-group'>";
    html += "<button class='view-btn' onclick='window.open(\"/view?file=" + fileName + "\", \"_blank\")'>查看</button>";
    html += "<button class='download-btn' onclick='window.location.href=\"/download?file=" + fileName + "\"'>下载</button>";
    html += "<button class='delete-btn' onclick='deleteFile(\"" + fileName + "\")'>删除</button>";
    html += "</div>";
    html += "</div>";
    
    file = root.openNextFile();
  }

  root.close();

  if (!hasFiles) {
    Serial.println("未找到任何文件");
    html += "<div class='no-files'>暂无文件</div>";
  }

  html += "</div>";
  html += "<a class='back' href='/'>返回首页</a>";
  html += "</div></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

// 添加格式化处理函数
void handleFormat() {
  Serial.println("开始格式化文件系统...");
  
  if (SPIFFS.format()) {
    Serial.println("格式化成功");
    
    // 发送格式化成功页面
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<meta http-equiv='refresh' content='2;url=/'>";  // 2秒后自动返回首页
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;text-align:center;}";
    html += ".box{background:white;max-width:600px;margin:20px auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "h2{color:#28a745;}";
    html += "</style></head><body>";
    html += "<div class='box'>";
    html += "<h2>✓ 格式化成功</h2>";
    html += "<p>文件系统已清空</p>";
    html += "<p>2秒后自动返回...</p>";
    html += "</div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  } else {
    Serial.println("格式化失败");
    server.send(500, "text/plain; charset=utf-8", "格式化失败");
  }
}

// 添加文件下载处理函数
void handleDownload() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain; charset=utf-8", "缺少文件名参数");
    return;
  }
  
  String filename = server.arg("file");
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  
  if (SPIFFS.exists(filename)) {
    File file = SPIFFS.open(filename, "r");
    if (file) {
      // 设置Content-Disposition头，使浏览器下载文件
      server.sendHeader("Content-Disposition", "attachment; filename=" + filename.substring(1));
      server.streamFile(file, "application/octet-stream");
      file.close();
    } else {
      server.send(500, "text/plain; charset=utf-8", "无法打开文件");
    }
  } else {
    server.send(404, "text/plain; charset=utf-8", "文件不存在");
  }
}

// 添加文件查看处理函数
void handleView() {
  if (!server.hasArg("file")) {
    server.send(400, "text/plain; charset=utf-8", "缺少文件名参数");
    return;
  }
  
  String filename = server.arg("file");
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  
  if (SPIFFS.exists(filename)) {
    File file = SPIFFS.open(filename, "r");
    if (file) {
      // 对于文本文件，显示编辑界面
      if (filename.endsWith(".txt") || filename.endsWith(".html") || 
          filename.endsWith(".css") || filename.endsWith(".js") || 
          filename.endsWith(".json") || filename.endsWith(".xml")) {
        
        String content = file.readString();
        file.close();
        
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<style>";
        html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }";
        html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
        html += "h2 { color: #333; text-align: center; margin-bottom: 20px; }";
        html += "textarea { width: 100%; height: 400px; margin-bottom: 10px; padding: 8px; border: 1px solid #ddd; border-radius: 4px; resize: vertical; }";
        html += ".btn-group { display: flex; gap: 10px; justify-content: center; }";
        html += ".btn { padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; color: white; text-decoration: none; }";
        html += ".save-btn { background: #007bff; }";
        html += ".back-btn { background: #6c757d; }";
        html += "</style>";
        html += "</head><body><div class='container'>";
        html += "<h2>查看/编辑文件: " + filename + "</h2>";
        html += "<form action='/save' method='POST'>";
        html += "<input type='hidden' name='filename' value='" + filename + "'>";
        html += "<textarea name='content'>" + content + "</textarea>";
        html += "<div class='btn-group'>";
        html += "<input type='submit' class='btn save-btn' value='保存'>";
        html += "<a href='/list' class='btn back-btn'>返回列表</a>";
        html += "</div>";
        html += "</form>";
        html += "</div></body></html>";
        
        server.send(200, "text/html; charset=utf-8", html);
      } else {
        // 对于非文本文件，直接显示
        String contentType = "application/octet-stream";
        if (filename.endsWith(".png")) contentType = "image/png";
        else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) contentType = "image/jpeg";
        else if (filename.endsWith(".gif")) contentType = "image/gif";
        else if (filename.endsWith(".ico")) contentType = "image/x-icon";
        else if (filename.endsWith(".pdf")) contentType = "application/pdf";
        
        server.streamFile(file, contentType);
        file.close();
      }
    } else {
      server.send(500, "text/plain; charset=utf-8", "无法打开文件");
    }
  } else {
    server.send(404, "text/plain; charset=utf-8", "文件不存在");
  }
}

// 处理编辑器页面
void handleEditor() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
  html += "h2 { color: #333; text-align: center; margin-bottom: 20px; }";
  html += ".editor-form { margin: 20px 0; }";
  html += "input[type='text'], textarea { width: 100%; margin-bottom: 10px; padding: 8px; border: 1px solid #ddd; border-radius: 4px; }";
  html += "textarea { height: 300px; resize: vertical; }";
  html += ".btn { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; }";
  html += ".btn:hover { background: #0056b3; }";
  html += ".back-btn { background: #6c757d; }";
  html += ".back-btn:hover { background: #5a6268; }";
  html += ".btn-group { display: flex; gap: 10px; justify-content: center; }";
  html += "</style>";
  html += "<script>";
  html += "function saveFile() {";
  html += "  const filename = document.getElementById('filename').value;";
  html += "  const content = document.getElementById('content').value;";
  html += "  if (!filename) {";
  html += "    alert('请输入文件名！');";
  html += "    return false;";
  html += "  }";
  html += "  return true;";
  html += "}";
  html += "</script>";
  html += "</head><body><div class='container'>";
  html += "<h2>在线记事本</h2>";
  html += "<form class='editor-form' action='/save' method='POST' onsubmit='return saveFile()'>";
  html += "<input type='text' id='filename' name='filename' placeholder='输入文件名 (例如: note.txt)' required>";
  html += "<textarea id='content' name='content' placeholder='在此输入内容...'></textarea>";
  html += "<div class='btn-group'>";
  html += "<input type='submit' class='btn' value='保存'>";
  html += "<a href='/' class='btn back-btn' style='text-decoration: none;'>返回首页</a>";
  html += "</div>";
  html += "</form>";
  html += "</div></body></html>";
  
  server.send(200, "text/html; charset=utf-8", html);
}

// 处理文件保存
void handleSave() {
  String filename = server.arg("filename");
  String content = server.arg("content");
  
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    server.send(500, "text/plain; charset=utf-8", "无法创建文件");
    return;
  }
  
  if (file.print(content)) {
    file.close();
    // 发送保存成功页面
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='2;url=/list'>";
    html += "<style>";
    html += "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;text-align:center;}";
    html += ".box{background:white;max-width:600px;margin:20px auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "h2{color:#28a745;}";
    html += "</style></head><body>";
    html += "<div class='box'>";
    html += "<h2>✓ 保存成功</h2>";
    html += "<p>文件已保存：" + filename + "</p>";
    html += "<p>2秒后自动跳转到文件列表...</p>";
    html += "</div></body></html>";
    server.send(200, "text/html; charset=utf-8", html);
  } else {
    file.close();
    server.send(500, "text/plain; charset=utf-8", "保存失败");
  }
}
