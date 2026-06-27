#include "emote_display.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "assets/lang_config.h"

namespace emote {

static const char* TAG = "EmoteDisplayMurni";

// Warna format RGB565 untuk layar Xiaozhi
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF

static uint32_t get_millis() {
    return esp_timer_get_time() / 1000;
}

// ============================================================================
// ENGINE GRAFIK INTERAL (Bypass Adafruit_GFX ke format Array RGB565)
// ============================================================================
static void drawPixel(uint16_t* buffer, int width, int height, int x, int y, uint16_t color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        buffer[y * width + x] = color;
    }
}

static void fillRect(uint16_t* buffer, int width, int height, int x, int y, int w, int h, uint16_t color) {
    for (int i = y; i < y + h; ++i) {
        for (int j = x; j < x + w; ++j) {
            drawPixel(buffer, width, height, j, i, color);
        }
    }
}

static void fillCircle(uint16_t* buffer, int width, int height, int cx, int cy, int r, uint16_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                drawPixel(buffer, width, height, cx + x, cy + y, color);
            }
        }
    }
}

static void drawFastHLine(uint16_t* buffer, int width, int height, int x, int y, int w, uint16_t color) {
    for (int j = x; j < x + w; ++j) {
        drawPixel(buffer, width, height, j, y, color);
    }
}

static void fillTriangle(uint16_t* buffer, int width, int height, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    int minX = std::min({x0, x1, x2});
    int maxX = std::max({x0, x1, x2});
    int minY = std::min({y0, y1, y2});
    int maxY = std::max({y0, y1, y2});

    auto sign = [](int p1x, int p1y, int p2x, int p2y, int p3x, int p3y) {
        return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
    };

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float d1 = sign(x, y, x0, y0, x1, y1);
            float d2 = sign(x, y, x1, y1, x2, y2);
            float d3 = sign(x, y, x2, y2, x0, y0);
            if (!((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0))) {
                drawPixel(buffer, width, height, x, y, color);
            }
        }
    }
}

// ============================================================================
// TASK LOOP RENDERING UTAMA WAJAH LUBBY
// ============================================================================
static void EmoteRenderTask(void* pvParameters) {
    EmoteDisplay* displayInstance = static_cast<EmoteDisplay*>(pvParameters);
    int w = displayInstance->width();
    int h = displayInstance->height();
    esp_lcd_panel_handle_t panel = displayInstance->GetPanelHandle();

    uint16_t* frame_buffer = (uint16_t*)malloc(w * h * sizeof(uint16_t));
    if (!frame_buffer) {
        ESP_LOGE(TAG, "Gagal alokasi buffer wajah murni");
        vTaskDelete(NULL);
        return;
    }

    for (;;) {
        // 1. Hitung Fisika Gerakan Wajah Lubby
        displayInstance->UpdateFacePhysics();
        displayInstance->UpdateLook();
        displayInstance->UpdateBlink();

        // 2. Bersihkan Layar (Background Hitam)
        std::fill_n(frame_buffer, w * h, COLOR_BLACK);

        // 3. Ambil State & Koordinat Terkini
        RobotEmotion currentEmotion = displayInstance->GetCurEmotion();
        float fX = displayInstance->GetFaceX();
        float fY = displayInstance->GetFaceY();
        bool isBlinking = displayInstance->IsBlinking();

        // Titik Tengah Layar Dinamis (Menyesuaikan Ukuran Layar Xiaozhi)
        int cx = (w / 2) + fX; 
        int cy = (h / 2) + fY;
        int leftEyeX = cx - 26; 
        int rightEyeX = cx + 26; 
        int eyePosY = cy - 2;
        int pupilOffsetX = fX * 0.4; 
        int pupilOffsetY = fY * 0.2;

        // 4. Logika Gambar Persis dari Wajah.cpp Lubby Robot
        if (isBlinking) {
            drawFastHLine(frame_buffer, w, h, leftEyeX - 16, eyePosY, 32, COLOR_WHITE);
            drawFastHLine(frame_buffer, w, h, rightEyeX - 16, eyePosY, 32, COLOR_WHITE);
        } else {
            fillCircle(frame_buffer, w, h, leftEyeX, eyePosY, 16, COLOR_WHITE);
            fillCircle(frame_buffer, w, h, rightEyeX, eyePosY, 16, COLOR_WHITE);
            
            if (currentEmotion == EMOT_NGANTUK) {
                fillCircle(frame_buffer, w, h, leftEyeX + pupilOffsetX, eyePosY + 4, 6, COLOR_BLACK);
                fillCircle(frame_buffer, w, h, rightEyeX + pupilOffsetX, eyePosY + 4, 6, COLOR_BLACK);
                fillRect(frame_buffer, w, h, leftEyeX - 16, eyePosY - 16, 32, 16, COLOR_BLACK);
                fillRect(frame_buffer, w, h, rightEyeX - 16, eyePosY - 16, 32, 16, COLOR_BLACK);
            } else {
                fillCircle(frame_buffer, w, h, leftEyeX + pupilOffsetX, eyePosY + pupilOffsetY, 7, COLOR_BLACK);
                fillCircle(frame_buffer, w, h, rightEyeX + pupilOffsetX, eyePosY + pupilOffsetY, 7, COLOR_BLACK);
                fillCircle(frame_buffer, w, h, leftEyeX + pupilOffsetX - 2, eyePosY + pupilOffsetY - 2, 2, COLOR_WHITE);
                fillCircle(frame_buffer, w, h, rightEyeX + pupilOffsetX - 2, eyePosY + pupilOffsetY - 2, 2, COLOR_WHITE);
                
                if (currentEmotion == EMOT_MARAH) {
                    fillTriangle(frame_buffer, w, h, leftEyeX - 16, eyePosY - 16, leftEyeX + 16, eyePosY - 16, leftEyeX + 16, eyePosY - 4, COLOR_BLACK);
                    fillTriangle(frame_buffer, w, h, rightEyeX - 16, eyePosY - 16, rightEyeX + 16, eyePosY - 16, rightEyeX - 16, eyePosY - 4, COLOR_BLACK);
                } else if (currentEmotion == EMOT_SEDIH) {
                    fillTriangle(frame_buffer, w, h, leftEyeX - 16, eyePosY - 16, leftEyeX + 16, eyePosY - 16, leftEyeX - 16, eyePosY - 4, COLOR_BLACK);
                    fillTriangle(frame_buffer, w, h, rightEyeX - 16, eyePosY - 16, rightEyeX + 16, eyePosY - 16, rightEyeX + 16, eyePosY - 4, COLOR_BLACK);
                }
            }
        }

        // Gambar Mulut Lubby
        if (currentEmotion == EMOT_SENANG) {
            fillTriangle(frame_buffer, w, h, cx - 10, cy + 18, cx + 10, cy + 18, cx, cy + 28, COLOR_WHITE);
        } else if (currentEmotion == EMOT_SEDIH || currentEmotion == EMOT_MARAH) {
            fillRect(frame_buffer, w, h, cx - 9, cy + 25, 20, 4, COLOR_WHITE);
            fillRect(frame_buffer, w, h, cx - 9, cy + 27, 20, 3, COLOR_BLACK);
        } else if (currentEmotion == EMOT_NGANTUK) {
            fillRect(frame_buffer, w, h, cx - 6, cy + 22, 12, 3, COLOR_WHITE);
            fillRect(frame_buffer, w, h, cx - 6, cy + 24, 12, 2, COLOR_BLACK);
        } else {
            fillRect(frame_buffer, w, h, cx - 9, cy + 23, 20, 4, COLOR_WHITE);
        }

        // Push bitmap wajah murni langsung ke LCD via ESP-IDF Panel Driver
        if (panel) {
            esp_lcd_panel_draw_bitmap(panel, 0, 0, w, h, frame_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(25)); // ~40 FPS Refresh Rate
    }
}

// ============================================================================
// CONSTRUCTOR & OVERRIDE FUNCTION XIAOZHI
// ============================================================================
EmoteDisplay::EmoteDisplay(const esp_lcd_panel_handle_t panel, const esp_lcd_panel_io_handle_t panel_io,
                           const int width, const int height)
{
    width_ = width;
    height_ = height;
    panel_handle_ = panel;
    current_emotion_ = EMOT_BIASA;
    
    // Alokasikan pointer dummy/kosong agar validasi pengecekan '!= nullptr' di assets.cc lolos
    emote_handle_ = (emote_handle_t)1; // Trik pointer dummy agar tidak NULL jika system mengeceknya

    // Jalankan task wajah murni Lubby Robot kita
    xTaskCreatePinnedToCore(EmoteRenderTask, "WajahMurniTask", 4096, this, 5, NULL, 1);
}

EmoteDisplay::~EmoteDisplay() {}

void EmoteDisplay::SetEmotion(const char* const emotion) {
    if (!emotion) return;
    ESP_LOGI(TAG, "SetEmotion: %s", emotion);

    if (std::strcmp(emotion, "happy") == 0) current_emotion_ = EMOT_SENANG;
    else if (std::strcmp(emotion, "sad") == 0) current_emotion_ = EMOT_SEDIH;
    else if (std::strcmp(emotion, "angry") == 0) current_emotion_ = EMOT_MARAH;
    else if (std::strcmp(emotion, "sleepy") == 0) current_emotion_ = EMOT_NGANTUK;
    else current_emotion_ = EMOT_BIASA;
}

void EmoteDisplay::SetStatus(const char* const status) {
    if (!status) return;
    ESP_LOGI(TAG, "SetStatus: %s", status);

    if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
        current_emotion_ = EMOT_BIASA; 
    } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
        current_emotion_ = EMOT_SENANG; // Otomatis senyum saat berbicara
    } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
        current_emotion_ = EMOT_MARAH;
    }
}

// SUNTIK MATI SEMUA EVENT TEKS & BAR STATUS BAWAAN XIAOZHI
void EmoteDisplay::SetChatMessage(const char* const role, const char* const content) {}
void EmoteDisplay::ShowNotification(const char* notification, int duration_ms) {}
void EmoteDisplay::UpdateStatusBar(bool update_all) {}
void EmoteDisplay::SetPowerSaveMode(bool on) {}
void EmoteDisplay::SetTheme(Theme* const theme) {}
bool EmoteDisplay::Lock(const int timeout_ms) { return true; }
void EmoteDisplay::Unlock() {}

// ============================================================================
// SIKLUS FISIKA WAJAH LUBBY (Diterjemahkan ke standar ESP-IDF)
// ============================================================================
void EmoteDisplay::UpdateFacePhysics() {
    float ax = (targetFaceX - faceX) * spring;
    float ay = (targetFaceY - height_ * 0.0) * spring; // disesuaikan agar stabil di tengah
    velX = (velX + ax) * damping; 
    velY = (velY + ay) * damping;
    faceX += velX; 
    faceY += velY;
}

void EmoteDisplay::UpdateLook() {
    uint32_t now = get_millis();
    if (now - lastLook > 2000) {
        lastLook = now;
        targetFaceX = (rand() % 13) - 6; 
        targetFaceY = (rand() % 9) - 4;
    }
}

void EmoteDisplay::UpdateBlink() {
    uint32_t now = get_millis();
    if (now > nextBlink) { 
        blinking = true; 
        lastBlink = now; 
        nextBlink = now + (rand() % 4000) + 3000; 
    }
    if (blinking && (now - lastBlink > 120)) { 
        blinking = false; 
    }
}

} // namespace emote