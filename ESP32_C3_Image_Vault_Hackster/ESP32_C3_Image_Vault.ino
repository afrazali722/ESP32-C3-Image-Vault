#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>

// =====================================================
// ESP32-C3 IMAGE VAULT + W25Q128
// Board: ESP32-C3 Super Mini
// Arduino IDE compatible
// =====================================================

// ---------- W25Q128 SPI PINS ----------
#define FLASH_SCK   4
#define FLASH_MISO  5
#define FLASH_MOSI  6
#define FLASH_CS    7

// ---------- Wi-Fi Access Point ----------
const char* AP_SSID     = "ESP32-VAULT";
const char* AP_PASSWORD = "12345678";   // minimum 8 characters

WebServer server(80);

// ---------- W25Q128 geometry ----------
const uint32_t FLASH_SIZE_BYTES = 16UL * 1024UL * 1024UL; // 16 MB
const uint32_t SECTOR_SIZE      = 4096;
const uint32_t PAGE_SIZE        = 256;

// Sector 0 stores image information.
// Actual image data starts from sector 1.
const uint32_t META_ADDR       = 0x000000;
const uint32_t IMAGE_DATA_ADDR = 0x001000;
const uint32_t MAX_IMAGE_SIZE  = FLASH_SIZE_BYTES - IMAGE_DATA_ADDR;

const char IMAGE_MAGIC[8] = {'I','M','G','V','A','U','L','T'};

struct ImageHeader {
  char magic[8];
  uint32_t size;
  char mime[32];
  char filename[80];
};

bool flashReady = false;
bool uploadHadError = false;
bool uploadFinalized = false;
uint32_t uploadAddress = IMAGE_DATA_ADDR;
uint32_t imageBytesWritten = 0;
uint32_t lastErasedSector = 0xFFFFFFFF;
String uploadFilename;
String uploadMime;

// =====================================================
// WEB PAGE
// =====================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-C3 Image Vault</title>
  <style>
    *{box-sizing:border-box}
    body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#08111f;color:#eef6ff;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:22px}
    .wrap{width:min(920px,100%)}
    .hero{margin-bottom:18px}
    h1{margin:0 0 8px;font-size:clamp(30px,5vw,52px)}
    h1 span{color:#39c4ff}
    .sub{color:#9eb3c9;line-height:1.5}
    .grid{display:grid;grid-template-columns:1fr 1fr;gap:18px}
    .card{background:#0e1c2f;border:1px solid #1d3857;border-radius:20px;padding:20px;box-shadow:0 18px 50px rgba(0,0,0,.25)}
    .drop{border:2px dashed #2a5f88;border-radius:16px;padding:22px;text-align:center;background:#0a1727}
    input[type=file]{width:100%;margin:14px 0;color:#cfe9ff}
    button{border:0;border-radius:12px;padding:13px 18px;font-weight:700;cursor:pointer;background:#29b6f6;color:#04111c;width:100%;font-size:16px}
    button:disabled{opacity:.5;cursor:not-allowed}
    .danger{margin-top:10px;background:#ff6b6b;color:#240606}
    .preview{width:100%;aspect-ratio:4/3;object-fit:contain;background:#050b13;border-radius:14px;border:1px solid #203a55;margin-top:15px}
    .hidden{display:none}
    .bar{height:12px;border-radius:20px;background:#132b43;overflow:hidden;margin-top:14px}
    .fill{height:100%;width:0;background:#39c4ff;transition:width .15s}
    .row{display:flex;justify-content:space-between;gap:15px;margin-top:10px;color:#9eb3c9;font-size:14px}
    .status{margin-top:13px;min-height:22px;color:#d8ecff}
    .badge{display:inline-block;margin-top:8px;padding:7px 10px;border-radius:999px;background:#12304a;color:#72d7ff;font-size:13px}
    @media(max-width:720px){.grid{grid-template-columns:1fr}}
  </style>
</head>
<body>
<div class="wrap">
  <div class="hero">
    <h1>ESP32-C3 <span>Image Vault</span></h1>
    <div class="sub">Upload an image over Wi-Fi and store it directly inside a W25Q128 16 MB external SPI flash memory.</div>
    <div class="badge">ESP32-VAULT • 192.168.4.1</div>
  </div>

  <div class="grid">
    <div class="card">
      <h2>Upload Image</h2>
      <div class="drop">
        <div>Select JPG, PNG, GIF or WEBP</div>
        <input id="file" type="file" accept="image/*">
        <img id="localPreview" class="preview hidden" alt="Selected image preview">
      </div>
      <div class="row"><span id="fileName">No file selected</span><span id="fileSize">0 KB</span></div>
      <div class="bar"><div id="fill" class="fill"></div></div>
      <div id="progressText" class="row"><span>Progress</span><span>0%</span></div>
      <button id="uploadBtn" disabled>Upload to W25Q128</button>
      <div id="status" class="status"></div>
    </div>

    <div class="card">
      <h2>Stored Image</h2>
      <div id="storedInfo" class="sub">Checking external flash...</div>
      <img id="storedImage" class="preview hidden" alt="Stored image">
      <button id="refreshBtn" style="margin-top:14px">Refresh Stored Image</button>
      <button id="deleteBtn" class="danger">Delete Stored Image</button>
    </div>
  </div>
</div>

<script>
const fileInput = document.getElementById('file');
const uploadBtn = document.getElementById('uploadBtn');
const localPreview = document.getElementById('localPreview');
const storedImage = document.getElementById('storedImage');
const storedInfo = document.getElementById('storedInfo');
const statusBox = document.getElementById('status');
const fill = document.getElementById('fill');
const progressText = document.getElementById('progressText');
const fileName = document.getElementById('fileName');
const fileSize = document.getElementById('fileSize');

function niceSize(bytes){
  if(bytes < 1024) return bytes + ' B';
  if(bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
  return (bytes/(1024*1024)).toFixed(2) + ' MB';
}

fileInput.addEventListener('change', () => {
  const f = fileInput.files[0];
  if(!f){ uploadBtn.disabled = true; return; }
  if(!f.type.startsWith('image/')){
    statusBox.textContent = 'Please select an image file.';
    uploadBtn.disabled = true;
    return;
  }
  localPreview.src = URL.createObjectURL(f);
  localPreview.classList.remove('hidden');
  fileName.textContent = f.name;
  fileSize.textContent = niceSize(f.size);
  uploadBtn.disabled = false;
  statusBox.textContent = 'Ready to upload.';
  fill.style.width = '0%';
  progressText.lastElementChild.textContent = '0%';
});

uploadBtn.addEventListener('click', () => {
  const f = fileInput.files[0];
  if(!f) return;

  const data = new FormData();
  data.append('image', f, f.name);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/upload');
  uploadBtn.disabled = true;
  statusBox.textContent = 'Uploading and writing to external flash...';

  xhr.upload.onprogress = e => {
    if(e.lengthComputable){
      const p = Math.round((e.loaded/e.total)*100);
      fill.style.width = p + '%';
      progressText.lastElementChild.textContent = p + '%';
    }
  };

  xhr.onload = () => {
    uploadBtn.disabled = false;
    if(xhr.status === 200){
      statusBox.textContent = 'Upload complete. Image stored in W25Q128.';
      fill.style.width = '100%';
      progressText.lastElementChild.textContent = '100%';
      loadStoredInfo();
    }else{
      statusBox.textContent = 'Upload failed: ' + xhr.responseText;
    }
  };

  xhr.onerror = () => {
    uploadBtn.disabled = false;
    statusBox.textContent = 'Connection error during upload.';
  };

  xhr.send(data);
});

async function loadStoredInfo(){
  try{
    const r = await fetch('/info?x=' + Date.now(), {cache:'no-store'});
    const info = await r.json();
    if(info.exists){
      storedInfo.textContent = info.name + ' • ' + niceSize(info.size) + ' • ' + info.mime;
      storedImage.src = '/image?x=' + Date.now();
      storedImage.classList.remove('hidden');
    }else{
      storedInfo.textContent = 'No image is currently stored.';
      storedImage.classList.add('hidden');
    }
  }catch(e){
    storedInfo.textContent = 'Could not read flash information.';
  }
}

document.getElementById('refreshBtn').onclick = loadStoredInfo;
document.getElementById('deleteBtn').onclick = async () => {
  if(!confirm('Delete the stored image?')) return;
  const r = await fetch('/delete', {method:'POST'});
  statusBox.textContent = await r.text();
  loadStoredInfo();
};

loadStoredInfo();
</script>
</body>
</html>
)rawliteral";

// =====================================================
// W25Q128 LOW-LEVEL FUNCTIONS
// =====================================================
void flashSelect() {
  digitalWrite(FLASH_CS, LOW);
}

void flashDeselect() {
  digitalWrite(FLASH_CS, HIGH);
}

uint8_t flashReadStatus1() {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  flashSelect();
  SPI.transfer(0x05); // Read Status Register-1
  uint8_t status = SPI.transfer(0x00);
  flashDeselect();
  SPI.endTransaction();
  return status;
}

bool flashWaitReady(uint32_t timeoutMs = 15000) {
  uint32_t start = millis();
  while (flashReadStatus1() & 0x01) {
    delay(1);
    if (millis() - start > timeoutMs) return false;
  }
  return true;
}

void flashWriteEnable() {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  flashSelect();
  SPI.transfer(0x06); // Write Enable
  flashDeselect();
  SPI.endTransaction();
}

uint32_t flashReadJedecId() {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  flashSelect();
  SPI.transfer(0x9F);
  uint8_t mfr  = SPI.transfer(0x00);
  uint8_t type = SPI.transfer(0x00);
  uint8_t cap  = SPI.transfer(0x00);
  flashDeselect();
  SPI.endTransaction();

  Serial.printf("Manufacturer : 0x%02X\n", mfr);
  Serial.printf("Memory Type  : 0x%02X\n", type);
  Serial.printf("Capacity     : 0x%02X\n", cap);

  return ((uint32_t)mfr << 16) | ((uint32_t)type << 8) | cap;
}

bool flashEraseSector(uint32_t address) {
  if (!flashWaitReady()) return false;
  flashWriteEnable();

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  flashSelect();
  SPI.transfer(0x20); // 4 KB Sector Erase
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  flashDeselect();
  SPI.endTransaction();

  return flashWaitReady();
}

bool flashPageProgram(uint32_t address, const uint8_t* data, size_t len) {
  if (len == 0 || len > PAGE_SIZE) return false;
  if (((address & 0xFF) + len) > PAGE_SIZE) return false;
  if (!flashWaitReady()) return false;

  flashWriteEnable();
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  flashSelect();
  SPI.transfer(0x02); // Page Program
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  for (size_t i = 0; i < len; i++) SPI.transfer(data[i]);
  flashDeselect();
  SPI.endTransaction();

  return flashWaitReady();
}

void flashRead(uint32_t address, uint8_t* data, size_t len) {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  flashSelect();
  SPI.transfer(0x03); // Read Data
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  for (size_t i = 0; i < len; i++) data[i] = SPI.transfer(0x00);
  flashDeselect();
  SPI.endTransaction();
}

bool flashWrite(uint32_t address, const uint8_t* data, size_t len) {
  while (len > 0) {
    size_t pageRemaining = PAGE_SIZE - (address & 0xFF);
    size_t chunk = len;
    if (chunk > pageRemaining) chunk = pageRemaining;

    if (!flashPageProgram(address, data, chunk)) return false;

    address += chunk;
    data += chunk;
    len -= chunk;
  }
  return true;
}

// =====================================================
// IMAGE METADATA
// =====================================================
bool loadImageHeader(ImageHeader &header) {
  flashRead(META_ADDR, (uint8_t*)&header, sizeof(header));

  if (memcmp(header.magic, IMAGE_MAGIC, 8) != 0) return false;
  if (header.size == 0 || header.size > MAX_IMAGE_SIZE) return false;

  header.mime[sizeof(header.mime) - 1] = 0;
  header.filename[sizeof(header.filename) - 1] = 0;
  return true;
}

bool saveImageHeader(uint32_t size, const String &filename, const String &mime) {
  ImageHeader header = {};
  memcpy(header.magic, IMAGE_MAGIC, 8);
  header.size = size;
  strncpy(header.mime, mime.c_str(), sizeof(header.mime) - 1);
  strncpy(header.filename, filename.c_str(), sizeof(header.filename) - 1);

  // META sector was erased when upload started.
  return flashWrite(META_ADDR, (const uint8_t*)&header, sizeof(header));
}

String mimeFromFilename(String filename) {
  filename.toLowerCase();
  if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".webp")) return "image/webp";
  if (filename.endsWith(".bmp")) return "image/bmp";
  return "application/octet-stream";
}

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '\\' || c == '"') out += '\\';
    if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

// =====================================================
// SEQUENTIAL IMAGE WRITER
// =====================================================
bool writeImageChunk(const uint8_t* data, size_t len) {
  while (len > 0) {
    if (uploadAddress >= FLASH_SIZE_BYTES) return false;

    uint32_t sectorBase = uploadAddress & ~(SECTOR_SIZE - 1);
    if (sectorBase != lastErasedSector) {
      if (!flashEraseSector(sectorBase)) return false;
      lastErasedSector = sectorBase;
    }

    size_t sectorRemaining = SECTOR_SIZE - (uploadAddress - sectorBase);
    size_t pageRemaining = PAGE_SIZE - (uploadAddress & 0xFF);
    size_t chunk = len;
    if (chunk > sectorRemaining) chunk = sectorRemaining;
    if (chunk > pageRemaining) chunk = pageRemaining;

    if (uploadAddress + chunk > FLASH_SIZE_BYTES) return false;
    if (!flashPageProgram(uploadAddress, data, chunk)) return false;

    uploadAddress += chunk;
    imageBytesWritten += chunk;
    data += chunk;
    len -= chunk;
  }
  return true;
}

// =====================================================
// HTTP HANDLERS
// =====================================================
void handleUploadData() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadHadError = !flashReady;
    uploadFinalized = false;
    uploadAddress = IMAGE_DATA_ADDR;
    imageBytesWritten = 0;
    lastErasedSector = 0xFFFFFFFF;
    uploadFilename = upload.filename;
    uploadMime = mimeFromFilename(upload.filename);

    Serial.println("\n--- NEW IMAGE UPLOAD ---");
    Serial.print("File: ");
    Serial.println(uploadFilename);

    // Invalidate previous image before we overwrite its data.
    if (!uploadHadError && !flashEraseSector(META_ADDR)) {
      uploadHadError = true;
    }
  }

  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadHadError) {
      if ((uint64_t)imageBytesWritten + upload.currentSize > MAX_IMAGE_SIZE) {
        uploadHadError = true;
      } else if (!writeImageChunk(upload.buf, upload.currentSize)) {
        uploadHadError = true;
      }
    }
  }

  else if (upload.status == UPLOAD_FILE_END) {
    if (!uploadHadError && imageBytesWritten > 0) {
      if (!saveImageHeader(imageBytesWritten, uploadFilename, uploadMime)) {
        uploadHadError = true;
      } else {
        uploadFinalized = true;
      }
    } else {
      uploadHadError = true;
    }

    Serial.print("Bytes stored: ");
    Serial.println(imageBytesWritten);
    Serial.println(uploadHadError ? "UPLOAD FAILED" : "UPLOAD COMPLETE");
  }

  else if (upload.status == UPLOAD_FILE_ABORTED) {
    uploadHadError = true;
    uploadFinalized = false;
    Serial.println("Upload aborted.");
  }
}

void handleInfo() {
  ImageHeader header;
  if (!flashReady || !loadImageHeader(header)) {
    server.send(200, "application/json", "{\"exists\":false}");
    return;
  }

  String json = "{\"exists\":true,\"size\":" + String(header.size) +
                ",\"name\":\"" + jsonEscape(String(header.filename)) +
                "\",\"mime\":\"" + jsonEscape(String(header.mime)) + "\"}";
  server.send(200, "application/json", json);
}

void handleImage() {
  ImageHeader header;
  if (!flashReady || !loadImageHeader(header)) {
    server.send(404, "text/plain", "No image stored");
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Content-Disposition", "inline; filename=\"" + String(header.filename) + "\"");
  server.setContentLength(header.size);
  server.send(200, header.mime, "");

  WiFiClient client = server.client();
  uint8_t buffer[1024];
  uint32_t address = IMAGE_DATA_ADDR;
  uint32_t remaining = header.size;

  while (remaining > 0 && client.connected()) {
    size_t n = remaining;
    if (n > sizeof(buffer)) n = sizeof(buffer);

    flashRead(address, buffer, n);

    size_t sent = 0;
    while (sent < n && client.connected()) {
      size_t written = client.write(buffer + sent, n - sent);
      if (written == 0) {
        delay(1);
      } else {
        sent += written;
      }
    }

    if (sent != n) break;
    address += n;
    remaining -= n;
    delay(0);
  }
}

void handleDelete() {
  if (!flashReady) {
    server.send(500, "text/plain", "Flash not available");
    return;
  }

  if (flashEraseSector(META_ADDR)) {
    server.send(200, "text/plain", "Stored image deleted.");
  } else {
    server.send(500, "text/plain", "Could not erase image metadata.");
  }
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("====================================");
  Serial.println(" ESP32-C3 IMAGE VAULT + W25Q128");
  Serial.println("====================================");

  pinMode(FLASH_CS, OUTPUT);
  digitalWrite(FLASH_CS, HIGH);
  SPI.begin(FLASH_SCK, FLASH_MISO, FLASH_MOSI, FLASH_CS);

  uint32_t jedec = flashReadJedecId();
  Serial.printf("JEDEC ID     : %06lX\n", (unsigned long)jedec);

  flashReady = (jedec != 0x000000 && jedec != 0xFFFFFF);

  if (jedec == 0xEF4018) {
    Serial.println("W25Q128 detected successfully.");
  } else if (flashReady) {
    Serial.println("SPI flash detected, but JEDEC ID is not EF4018.");
  } else {
    Serial.println("ERROR: External flash not detected. Check wiring and 3.3V power.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println();
  Serial.print("Wi-Fi SSID   : ");
  Serial.println(AP_SSID);
  Serial.print("Password     : ");
  Serial.println(AP_PASSWORD);
  Serial.print("Open browser : http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/upload", HTTP_POST,
    []() {
      if (uploadHadError || !uploadFinalized) {
        server.send(500, "application/json", "{\"ok\":false,\"message\":\"Upload or flash write failed\"}");
      } else {
        server.send(200, "application/json", "{\"ok\":true}");
      }
    },
    handleUploadData
  );

  server.on("/info", HTTP_GET, handleInfo);
  server.on("/image", HTTP_GET, handleImage);
  server.on("/delete", HTTP_POST, handleDelete);

  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  delay(2);
}
