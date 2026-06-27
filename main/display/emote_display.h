#pragma once

#include "display.h"
#include <memory>
#include <string>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

namespace emote {

// Enum Emosi diadopsi dari wajah.h Lubby
enum RobotEmotion { 
    EMOT_BIASA, 
    EMOT_SENANG, 
    EMOT_MARAH, 
    EMOT_SEDIH, 
    EMOT_NGANTUK 
};

class EmoteDisplay : public Display {
public:
    EmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io, int width, int height);
    virtual ~EmoteDisplay();

    // Override fungsi bawaan Xiaozhi
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetPowerSaveMode(bool on) override;

    // Fungsi update fisik wajah dari Wajah.cpp Lubby
    void UpdateFacePhysics();
    void UpdateLook();
    void UpdateBlink();

    // Getter untuk diakses oleh Task Render
    esp_lcd_panel_handle_t GetPanelHandle() const { return panel_handle_; }
    RobotEmotion GetCurEmotion() const { return current_emotion_; }
    float GetFaceX() const { return faceX; }
    float GetFaceY() const { return faceY; }
    bool IsBlinking() const { return blinking; }

private:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    esp_lcd_panel_handle_t panel_handle_ = nullptr;
    RobotEmotion current_emotion_ = EMOT_BIASA;

    // Variabel fisik wajah internal (pindahan dari Wajah.cpp)
    float faceX = 0, faceY = 0;
    float targetFaceX = 0, targetFaceY = 0;
    float velX = 0, velY = 0;
    float spring = 0.08, damping = 0.75;
    bool blinking = false;
    uint32_t lastBlink = 0, nextBlink = 0, lastLook = 0;
};

} // namespace emote