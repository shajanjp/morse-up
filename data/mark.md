Got it — you’re asking specifically **how to upload those files (`.md`, `.bmp`) into ESP32 Flash (LittleFS)**.

This part is a bit confusing the first time, so here’s the **cleanest working method** 👇

---

# 🧱 Step 1: Create the `data` folder

In your Arduino project folder:

```
your_project/
 ├── your_project.ino
 └── data/
      ├── book/
      │    └── chapter1.md
      └── images/
           └── sample.bmp
```

👉 The folder **must be named exactly `data`**

---

# ⚙️ Step 2: Install ESP32 Filesystem Uploader

### Option A (Recommended - Arduino IDE 2.x users)

Install this plugin:

👉 ESP32 LittleFS Uploader

### How:

1. Download from:
   [https://github.com/lorol/arduino-esp32littlefs-plugin](https://github.com/lorol/arduino-esp32littlefs-plugin)
2. Copy to:

   ```
   Documents/Arduino/tools/
   ```
3. Restart Arduino IDE

---

# 🚀 Step 3: Select correct board settings

In Arduino IDE:

* Board: **M5Stack Paper**
* Partition Scheme:
  👉 **“Huge APP (3MB No OTA / 1MB SPIFFS)”**
  OR
  👉 Any scheme with **SPIFFS/LittleFS space**

---

# 📤 Step 4: Upload files to ESP32

Now in Arduino IDE:

👉 Tools → **ESP32 LittleFS Data Upload**

This will:

* Take everything inside `data/`
* Upload it into Flash

---

# 🧪 Step 5: Verify it works

Add this debug code:

```cpp
File root = LittleFS.open("/");
File file = root.openNextFile();

while (file) {
  Serial.println(file.name());
  file = root.openNextFile();
}
```

👉 You should see:

```
/book/chapter1.md
/images/sample.bmp
```

---

# 🧠 Common mistakes (you will hit at least one)

### ❌ “LittleFS Mount Failed”

* You forgot `LittleFS.begin()`

---

### ❌ Upload option not visible

* Plugin not installed correctly
* Restart IDE

---

### ❌ Files not found

* Wrong path (`/book/...` vs `book/...`)
* Must start with `/`

---

### ❌ Not enough space

* Change partition scheme

---

# 💡 Pro Tips (for your use case)

Since you're building a **reader on M5Paper**:

### ✅ Keep files small

* Markdown: fine
* Images: compress + resize

---

### ✅ Use this structure

```
/book/
/images/
```

---

### ✅ Use BMP (not PNG/JPG initially)

* Much easier on ESP32
* Faster render on e-ink

---

# 🔥 Alternative (better long-term)

If you don’t want this upload step every time:

### Option B: SD Card (M5Paper supports it)

Just:

* Copy files → SD card
* Use:

```cpp
SD.open("/book/chapter1.md");
```

👉 Much faster for iteration

---

# 🧠 Simple mental model

* `data/` → your local files
* Upload tool → pushes into Flash
* `LittleFS` → reads it at runtime

---

If you want next:
👉 I can show you:

* One-command script to auto-convert images + upload
* OR a live update system (push new chapters over WiFi without reflashing)
