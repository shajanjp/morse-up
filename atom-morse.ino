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
  serializeJson(doc, file);
  file.close();
}

void loadScores() {
  if (!LittleFS.exists("/scores.json")) return;

  File file = LittleFS.open("/scores.json", "r");
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, file);

  for (auto &item : items) {
    item.score = doc[String(item.character)] | 0;
  }
  file.close();
}

// ================= QUEUE =================
void initQueue() {
  queue.clear();
  for (int i = 0; i < items.size(); i++) {
    queue.push_back(i);
  }
}

void reinsert(int idx) {
  int count = min(items[idx].score + 1, MAX_REINSERT);
  for (int i = 0; i < count; i++) {
    queue.push_back(idx);
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
    if (item.score == 0) learned++;
  }
  return (learned * 100) / items.size();
}

// ================= UI =================
void render() {
  M5.Display.startWrite();
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(TFT_WHITE); 

  // Progress
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(String(getProgress()) + "%", M5.Display.width() / 2, 10);

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
    M5.Display.setTextColor(TFT_GREEN);   // red text
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

  if (!isCorrect) {
    items[currentIndex].score++;
    reinsert(currentIndex);
    saveScores();
  }

  state = FEEDBACK;
  feedbackStart = millis();

  needsRedraw = true;
}

// ================= SETUP =================
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  pinMode(DOT_PIN, INPUT_PULLUP);
  pinMode(DASH_PIN, INPUT_PULLUP);

  LittleFS.begin();
  loadScores();
  initQueue();

  startNext();
}

// ================= LOOP =================
void loop() {
  M5.update();

  // INPUT STATE
  if (state == INPUT_STATE) {
    bool dotPressed = digitalRead(DOT_PIN) == LOW;
    bool dashPressed = digitalRead(DASH_PIN) == LOW;

    if (dotPressed) {
      currentInput += ".";
      lastInputTime = millis();
      needsRedraw = true;

      while (digitalRead(DOT_PIN) == LOW); // wait release
    }

    if (dashPressed) {
      currentInput += "_";
      lastInputTime = millis();
      needsRedraw = true;

      while (digitalRead(DASH_PIN) == LOW); // wait release
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