#include <M5Unified.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// ================= CONFIG =================
#define DOT_PIN 1
#define DASH_PIN 2

#define INPUT_TIMEOUT 2000
#define FEEDBACK_DURATION 2000
#define MAX_REINSERT 5
#define DASH_THRESHOLD 400

// ================= DATA =================
struct MorseItem {
  char character;
  const char* morse;
  int score;
};

std::vector<MorseItem> items = {
  {'A', "._"}, {'B', "_..."}, {'C', "_._."}, {'D', "_.."}, {'E', "."},
  {'F', ".._."}, {'G', "__."}, {'H', "...."}, {'I', ".."}, {'J', ".___"},
  {'K', "_._"}, {'L', "._.."}, {'M', "__"}, {'N', "_."}, {'O', "___"},
  {'P', ".__."}, {'Q', "__._"}, {'R', "._."}, {'S', "..."}, {'T', "_"},
  {'U', ".._"}, {'V', "..._"}, {'W', ".__"}, {'X', "_.._"}, {'Y', "_.__"},
  {'Z', "__.."},
  {'0', "_____"}, {'1', ".____"}, {'2', "..___"}, {'3', "...__"},
  {'4', "...._"}, {'5', "....."}, {'6', "_...."}, {'7', "__..."},
  {'8', "___.."}, {'9', "____."}
};

std::vector<int> queue;
int currentIndex = 0;

// ================= STATE =================
enum State { SHOW_CHAR, INPUT_STATE, FEEDBACK };
State state = SHOW_CHAR;

String currentInput = "";
unsigned long lastInputTime = 0;
unsigned long feedbackStart = 0;

bool isCorrect = false;
bool isPrefixCorrect = true;

// Render control
bool needsRedraw = true;

// ================= STORAGE =================
void saveScores() {
  DynamicJsonDocument doc(2048);
  for (auto &item : items) {
    doc[String(item.character)] = item.score;
  }

  File file = LittleFS.open("/scores.json", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to file");
  }
  file.close();
  Serial.println("Scores saved");
}

void loadScores() {
  if (!LittleFS.exists("/scores.json")) {
    Serial.println("No score file found");
    return;
  }

  File file = LittleFS.open("/scores.json", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    file.close();
    return;
  }

  for (auto &item : items) {
    item.score = doc[String(item.character)] | 0;
  }
  file.close();
  Serial.println("Scores loaded");
}

// ================= QUEUE =================
void initQueue() {
  queue.clear();
  for (int i = 0; i < items.size(); i++) {
    queue.push_back(i);
  }
  // Fisher-Yates shuffle
  for (int i = queue.size() - 1; i > 0; i--) {
    int j = random(i + 1);
    int temp = queue[i];
    queue[i] = queue[j];
    queue[j] = temp;
  }
}

void reinsert(int idx) {
  // Insert at a random position soon (between index 0 and 4) to ensure frequent appearance
  int maxPos = min((int)queue.size(), 4);
  int pos = random(0, maxPos + 1);
  queue.insert(queue.begin() + pos, idx);

  // Also add another copy a bit later (between index 5 and 10) to reinforce learning
  if (queue.size() > 6) {
    int pos2 = random(5, min((int)queue.size(), 10) + 1);
    queue.insert(queue.begin() + pos2, idx);
  }
}

// ================= UTILS =================
bool isPrefix(String input, const char* target) {
  for (int i = 0; i < input.length(); i++) {
    if (input[i] != target[i]) return false;
  }
  return true;
}

int getProgress() {
  int learned = 0;
  for (auto &item : items) {
    if (item.score > 0) learned++;
  }
  return (learned * 100) / items.size();
}

// ================= UI =================
void render() {
  M5.Display.startWrite();
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(TFT_WHITE);

  // Progress
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(String(getProgress()) + "%", M5.Display.width() / 2, 12);

  // Character
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(String(items[currentIndex].character), M5.Display.width() / 2, M5.Display.height() / 2);

  // Input
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  if (!isPrefixCorrect) {
    M5.Display.setTextColor(TFT_RED);
  }
  M5.Display.drawString(currentInput, M5.Display.width() / 2, 90);

  // Correct answer display
  if (!isPrefixCorrect || state == FEEDBACK) {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_GREEN);
    M5.Display.drawString(String(items[currentIndex].morse), M5.Display.width() / 2, 110);
  }

  M5.Display.endWrite();
}

// ================= LOGIC =================
void startNext() {
  if (queue.empty()) initQueue();

  currentIndex = queue.front();
  queue.erase(queue.begin());

  currentInput = "";
  isPrefixCorrect = true;
  state = INPUT_STATE;

  needsRedraw = true;
}

void validate() {
  const char* correct = items[currentIndex].morse;

  isCorrect = (currentInput == correct);

  if (isCorrect) {
    items[currentIndex].score++;
  } else {
    reinsert(currentIndex);
  }
  saveScores();

  state = FEEDBACK;
  feedbackStart = millis();

  needsRedraw = true;
}

// ================= SETUP =================
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  delay(1000); 

  randomSeed(esp_random());

  pinMode(DOT_PIN, INPUT_PULLUP);
  pinMode(DASH_PIN, INPUT_PULLUP);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed, but formatted");
  } else {
    Serial.println("LittleFS Mounted");
  }
  
  loadScores();
  initQueue();

  startNext();
}

// ================= LOOP =================
void loop() {
  M5.update();

  // INPUT STATE
  if (state == INPUT_STATE) {
    // --- External Buttons (Non-blocking) ---
    static bool dotWasPressed = false;
    static bool dashWasPressed = false;
    bool dotIsPressed = digitalRead(DOT_PIN) == LOW;
    bool dashIsPressed = digitalRead(DASH_PIN) == LOW;

    if (dotIsPressed && !dotWasPressed) {
      currentInput += ".";
      lastInputTime = millis();
      needsRedraw = true;
    }
    dotWasPressed = dotIsPressed;

    if (dashIsPressed && !dashWasPressed) {
      currentInput += "_";
      lastInputTime = millis();
      needsRedraw = true;
    }
    dashWasPressed = dashIsPressed;

    // --- Internal Button (GPIO 41) ---
    static bool internalDashTriggered = false;
    if (M5.BtnA.pressedFor(DASH_THRESHOLD)) {
      if (!internalDashTriggered) {
        currentInput += "_";
        lastInputTime = millis();
        needsRedraw = true;
        internalDashTriggered = true;
      }
    }
    if (M5.BtnA.wasReleased()) {
      if (!internalDashTriggered) {
        // Short press detected on release
        currentInput += ".";
        lastInputTime = millis();
        needsRedraw = true;
      }
      internalDashTriggered = false;
    }

    // Prefix check
    String compact = currentInput;
    bool prev = isPrefixCorrect;
    isPrefixCorrect = isPrefix(compact, items[currentIndex].morse);

    if (prev != isPrefixCorrect) {
      needsRedraw = true;
    }

    // Timeout validation
    if (currentInput.length() > 0 &&
        millis() - lastInputTime > INPUT_TIMEOUT) {
      validate();
    }
  }

  // FEEDBACK STATE
  else if (state == FEEDBACK) {
    // Reset internal button state for next item
    // (Static vars inside if(state==INPUT_STATE) might need to be reset
    // if we want to be perfectly safe, but since BtnA.wasReleased()
    // and pressedFor() are stateful in M5Unified, it's generally fine).

    if (millis() - feedbackStart > FEEDBACK_DURATION) {
      startNext();
    }
  }

  // 🔥 Render only when needed
  if (needsRedraw) {
    render();
    needsRedraw = false;
  }
}
