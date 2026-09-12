#include "wifi_board.h"
#include "display/lcd_display.h"
#include "lvgl_theme.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "assets/lang_config.h"
#include "cw2017_battery_monitor.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_lcd_panel_vendor.h>
#include <button_adc.h>
#include <esp_adc/adc_oneshot.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <array>

#define TAG "AiPassport"

// Physical keys share one ADC pin through a resistor ladder (see config.h).
enum {
    kAdcButtonUp = 0,
    kAdcButtonDown,
    kAdcButtonOk,
    kAdcButtonNum,
};

namespace {

#include "mao_quotations.inc"

}  // namespace

class AiPassportDisplay : public SpiLcdDisplay {
public:
    AiPassportDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y, bool mirror_x,
                      bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                        swap_xy) {}

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }

        auto theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = theme->text_font()->font();
        auto screen = lv_screen_active();

        // The stock compact LCD layout uses a single-line ticker. Passport has
        // enough vertical space for readable subtitles, so use a bounded text
        // panel that grows for short replies and scrolls long replies within
        // the screen instead of extending beyond it.
        if (bottom_bar_ != nullptr && chat_message_label_ != nullptr) {
            lv_obj_set_width(bottom_bar_, LV_HOR_RES);
            lv_obj_set_style_pad_all(bottom_bar_, kReplyPanelPadding, 0);
            lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_90, 0);
            lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);

            lv_obj_set_width(chat_message_label_, LV_HOR_RES - 2 * kReplyPanelPadding);
            lv_obj_set_style_text_line_space(chat_message_label_, 3, 0);
            lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);

            lv_anim_init(&reply_scroll_animation_);
            lv_anim_set_delay(&reply_scroll_animation_, 1200);
            lv_anim_set_repeat_delay(&reply_scroll_animation_, 1200);
            lv_anim_set_repeat_count(&reply_scroll_animation_, LV_ANIM_REPEAT_INFINITE);
            lv_obj_set_style_anim(chat_message_label_, &reply_scroll_animation_, LV_PART_MAIN);
            lv_obj_set_style_anim_duration(
                chat_message_label_, lv_anim_speed_clamped(35, 1200, 30000), LV_PART_MAIN);
        }

        quote_screen_ = lv_obj_create(screen);
        lv_obj_set_size(quote_screen_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_radius(quote_screen_, 0, 0);
        lv_obj_set_style_border_width(quote_screen_, 0, 0);
        lv_obj_set_style_pad_all(quote_screen_, 0, 0);
        lv_obj_set_style_bg_color(quote_screen_, lv_color_hex(0xF6E8C3), 0);
        lv_obj_set_style_bg_opa(quote_screen_, LV_OPA_COVER, 0);
        lv_obj_set_scrollbar_mode(quote_screen_, LV_SCROLLBAR_MODE_OFF);

        // A small, board-local vector portrait keeps the feature usable without
        // adding a large bitmap to this no-PSRAM target.
        auto portrait = lv_obj_create(quote_screen_);
        lv_obj_set_size(portrait, 76, 94);
        lv_obj_align(portrait, LV_ALIGN_TOP_MID, 0, 17);
        lv_obj_set_style_radius(portrait, 38, 0);
        lv_obj_set_style_border_width(portrait, 3, 0);
        lv_obj_set_style_border_color(portrait, lv_color_hex(0xA71918), 0);
        lv_obj_set_style_bg_color(portrait, lv_color_hex(0xD9B47A), 0);
        lv_obj_set_style_pad_all(portrait, 0, 0);
        lv_obj_clear_flag(portrait, LV_OBJ_FLAG_SCROLLABLE);

        auto hair = lv_obj_create(portrait);
        lv_obj_set_size(hair, 65, 29);
        lv_obj_align(hair, LV_ALIGN_TOP_MID, 0, -4);
        lv_obj_set_style_radius(hair, 22, 0);
        lv_obj_set_style_border_width(hair, 0, 0);
        lv_obj_set_style_bg_color(hair, lv_color_hex(0x27211D), 0);

        for (int x : {-16, 16}) {
            auto eye = lv_obj_create(portrait);
            lv_obj_set_size(eye, 7, 3);
            lv_obj_align(eye, LV_ALIGN_CENTER, x, -5);
            lv_obj_set_style_radius(eye, 2, 0);
            lv_obj_set_style_border_width(eye, 0, 0);
            lv_obj_set_style_bg_color(eye, lv_color_hex(0x35251C), 0);
        }

        auto mouth = lv_obj_create(portrait);
        lv_obj_set_size(mouth, 24, 3);
        lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 19);
        lv_obj_set_style_radius(mouth, 2, 0);
        lv_obj_set_style_border_width(mouth, 0, 0);
        lv_obj_set_style_bg_color(mouth, lv_color_hex(0x7B2D25), 0);

        auto collar = lv_obj_create(portrait);
        lv_obj_set_size(collar, 61, 25);
        lv_obj_align(collar, LV_ALIGN_BOTTOM_MID, 0, 10);
        lv_obj_set_style_radius(collar, 4, 0);
        lv_obj_set_style_border_width(collar, 0, 0);
        lv_obj_set_style_bg_color(collar, lv_color_hex(0x6A7560), 0);

        title_label_ = lv_label_create(quote_screen_);
        lv_label_set_text(title_label_, "毛教员语录");
        lv_obj_set_style_text_font(title_label_, text_font, 0);
        lv_obj_set_style_text_color(title_label_, lv_color_hex(0xA71918), 0);
        lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 116);

        quote_label_ = lv_label_create(quote_screen_);
        lv_obj_set_width(quote_label_, 204);
        lv_label_set_long_mode(quote_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(quote_label_, text_font, 0);
        lv_obj_set_style_text_align(quote_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(quote_label_, lv_color_hex(0x32281D), 0);
        lv_obj_align(quote_label_, LV_ALIGN_TOP_MID, 0, 157);

        source_label_ = lv_label_create(quote_screen_);
        lv_obj_set_width(source_label_, 210);
        lv_label_set_long_mode(source_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(source_label_, text_font, 0);
        lv_obj_set_style_text_align(source_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(source_label_, lv_color_hex(0x7B6250), 0);
        lv_obj_align(source_label_, LV_ALIGN_TOP_MID, 0, 224);

        page_label_ = lv_label_create(quote_screen_);
        lv_obj_set_style_text_font(page_label_, text_font, 0);
        lv_obj_set_style_text_color(page_label_, lv_color_hex(0xA71918), 0);
        lv_obj_align(page_label_, LV_ALIGN_BOTTOM_MID, 0, -25);

        hint_label_ = lv_label_create(quote_screen_);
        lv_label_set_text(hint_label_, "▲ 上一页    ▼ 下一页    OK 退出");
        lv_obj_set_style_text_font(hint_label_, text_font, 0);
        lv_obj_set_style_text_color(hint_label_, lv_color_hex(0x7B6250), 0);
        lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_MID, 0, -4);

        quotation_index_ = esp_random() % kMaoQuotations.size();
        UpdateQuotationLocked();
        lv_obj_add_flag(quote_screen_, LV_OBJ_FLAG_HIDDEN);
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (content == nullptr) {
            content = "";
        }
        SpiLcdDisplay::SetChatMessage(role, content);

        if (bottom_bar_ == nullptr || chat_message_label_ == nullptr || content[0] == '\0') {
            return;
        }

        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }

        lv_anim_delete(chat_message_label_, nullptr);
        lv_obj_set_width(chat_message_label_, LV_HOR_RES - 2 * kReplyPanelPadding);
        lv_obj_set_height(chat_message_label_, LV_SIZE_CONTENT);
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
        lv_obj_update_layout(chat_message_label_);

        const lv_coord_t natural_height = lv_obj_get_height(chat_message_label_);
        const lv_coord_t visible_height =
            natural_height > kReplyTextMaxHeight ? kReplyTextMaxHeight : natural_height;

        if (natural_height > kReplyTextMaxHeight) {
            lv_obj_set_height(chat_message_label_, kReplyTextMaxHeight);
            lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL);
        }

        const auto theme = static_cast<LvglTheme*>(current_theme_);
        const auto line_height = theme->text_font()->font()->line_height;
        lv_obj_set_style_text_align(chat_message_label_,
                                    natural_height <= line_height * 2 ? LV_TEXT_ALIGN_CENTER
                                                                    : LV_TEXT_ALIGN_LEFT,
                                    0);
        lv_obj_set_height(bottom_bar_, visible_height + 2 * kReplyPanelPadding);
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);
    }

    void SetTheme(Theme* theme) override {
        SpiLcdDisplay::SetTheme(theme);
        if (quote_screen_ == nullptr) {
            return;
        }

        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }
        auto text_font = static_cast<LvglTheme*>(theme)->text_font()->font();
        for (auto label : {title_label_, quote_label_, source_label_, page_label_, hint_label_}) {
            lv_obj_set_style_text_font(label, text_font, 0);
        }
    }

    bool IsQuotationMode() const { return quotation_mode_; }

    void SetQuotationMode(bool enabled) {
        if (quote_screen_ == nullptr || quotation_mode_ == enabled) {
            return;
        }
        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }
        quotation_mode_ = enabled;
        if (enabled) {
            UpdateQuotationLocked();
            lv_obj_remove_flag(quote_screen_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(quote_screen_);
        } else {
            lv_obj_add_flag(quote_screen_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void ChangeQuotation(int delta) {
        if (!quotation_mode_ || quote_screen_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }
        const int count = static_cast<int>(kMaoQuotations.size());
        quotation_index_ = (quotation_index_ + delta + count) % count;
        UpdateQuotationLocked();
    }

private:
    static constexpr lv_coord_t kReplyPanelPadding = 12;
    static constexpr lv_coord_t kReplyTextMaxHeight = DISPLAY_HEIGHT - 72;

    void UpdateQuotationLocked() {
        lv_label_set_text_fmt(quote_label_, "“%s”", kMaoQuotations[quotation_index_]);
        lv_label_set_text(source_label_, "—— 毛泽东");
        lv_label_set_text_fmt(page_label_, "%u / %u", static_cast<unsigned>(quotation_index_ + 1),
                              static_cast<unsigned>(kMaoQuotations.size()));
    }

    lv_obj_t* quote_screen_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* quote_label_ = nullptr;
    lv_obj_t* source_label_ = nullptr;
    lv_obj_t* page_label_ = nullptr;
    lv_obj_t* hint_label_ = nullptr;
    size_t quotation_index_ = 0;
    bool quotation_mode_ = false;
    lv_anim_t reply_scroll_animation_ = {};
};

class AiPassportBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button* adc_button_[kAdcButtonNum];
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    AiPassportDisplay* display_;
    Cw2017BatteryMonitor* battery_;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

        // CW2017 fuel gauge is optional; a missing chip just disables battery UI.
        battery_ = new Cw2017BatteryMonitor(codec_i2c_bus_, BATTERY_CW2017_ADDR);
        battery_->Initialize();
    }

    void ChangeVolume(int delta) {
        auto codec = GetAudioCodec();
        auto volume = codec->output_volume() + delta;
        if (volume > 100) {
            volume = 100;
        }
        if (volume < 0) {
            volume = 0;
        }
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void ToggleChat() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting) {
            EnterWifiConfigMode();
            return;
        }
        app.ToggleChatState();
    }

    void InitializeButtons() {
        for (int i = 0; i < kAdcButtonNum; i++) {
            adc_button_[i] = nullptr;
        }

        // One ADC1 unit shared by all three ladder keys. AdcButton reuses the
        // handle when adc_config.adc_handle is non-null, so the same physical
        // pin can decode several keys without "adc1 is already in use".
        adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle_));

        button_adc_config_t adc_cfg = {};
        adc_cfg.adc_handle = &adc_handle_;
        adc_cfg.unit_id = ADC_UNIT_1;
        adc_cfg.adc_channel = ADC_CHANNEL_0;  // GPIO0

        adc_cfg.button_index = kAdcButtonUp;      // UP:   ~0 mV
        adc_cfg.min = BSP_ADC_BUTTON_UP_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_UP_MAX;
        adc_button_[kAdcButtonUp] = new AdcButton(adc_cfg);

        adc_cfg.button_index = kAdcButtonDown;    // DOWN: ~300 mV
        adc_cfg.min = BSP_ADC_BUTTON_DOWN_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_DOWN_MAX;
        adc_button_[kAdcButtonDown] = new AdcButton(adc_cfg);

        adc_cfg.button_index = kAdcButtonOk;      // OK:   ~595 mV
        adc_cfg.min = BSP_ADC_BUTTON_OK_MIN;
        adc_cfg.max = BSP_ADC_BUTTON_OK_MAX;
        adc_button_[kAdcButtonOk] = new AdcButton(adc_cfg);

        // Button callbacks run on the button task; schedule all UI/audio
        // work onto the main task so LVGL and codec access stay on one thread.
        auto up = adc_button_[kAdcButtonUp];
        up->OnClick([this]() {
            Application::GetInstance().Schedule([this]() {
                if (display_->IsQuotationMode()) {
                    display_->ChangeQuotation(-1);
                } else {
                    ChangeVolume(10);
                }
            });
        });
        up->OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                GetAudioCodec()->SetOutputVolume(100);
                GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
            });
        });

        auto down = adc_button_[kAdcButtonDown];
        down->OnClick([this]() {
            Application::GetInstance().Schedule([this]() {
                if (display_->IsQuotationMode()) {
                    display_->ChangeQuotation(1);
                } else {
                    ChangeVolume(-10);
                }
            });
        });
        down->OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                GetAudioCodec()->SetOutputVolume(0);
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            });
        });

        auto ok = adc_button_[kAdcButtonOk];
        ok->OnClick([this]() {
            Application::GetInstance().Schedule([this]() {
                if (display_->IsQuotationMode()) {
                    display_->SetQuotationMode(false);
                } else {
                    ToggleChat();
                }
            });
        });
        ok->OnLongPress([this]() {
            Application::GetInstance().Schedule([this]() {
                display_->SetQuotationMode(!display_->IsQuotationMode());
            });
        });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;  // -1 -> software reset
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);

        // Panel-specific power/porch/gamma sequence for the ST7789P3 module
        // used on the Passport (copied from the original badge firmware).
        static const struct {
            uint8_t command;
            uint8_t data[16];
            uint8_t data_length;
            uint16_t delay_ms;
        } kSt7789P3InitCommands[] = {
            {0xB2, {0x05, 0x05, 0x00, 0x33, 0x33}, 5, 0},
            {0xB7, {0x35}, 1, 0},
            {0xBB, {0x21}, 1, 0},
            {0xC0, {0x2C}, 1, 0},
            {0xC2, {0x01}, 1, 0},
            {0xC3, {0x0B}, 1, 0},
            {0xC4, {0x20}, 1, 0},
            {0xC6, {0x0F}, 1, 0},
            {0xD0, {0xA7, 0xA1}, 2, 0},
            {0xD0, {0xA4, 0xA1}, 2, 0},
            {0xD6, {0xA1}, 1, 0},
            {0xE0, {0xD0, 0x04, 0x08, 0x0A, 0x09, 0x05, 0x2D, 0x43,
                    0x49, 0x09, 0x16, 0x15, 0x26, 0x2B}, 14, 0},
            {0xE1, {0xD0, 0x03, 0x09, 0x0A, 0x0A, 0x06, 0x2E, 0x44,
                    0x40, 0x3A, 0x15, 0x15, 0x26, 0x2A}, 14, 10},
        };
        for (const auto& cmd : kSt7789P3InitCommands) {
            esp_lcd_panel_io_tx_param(panel_io, cmd.command, cmd.data, cmd.data_length);
            if (cmd.delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(cmd.delay_ms));
            }
        }

        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_set_gap(panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new AiPassportDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                         DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                         DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    AiPassportBoard() : display_(nullptr), battery_(nullptr) {
        InitializeCodecI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!battery_ || !battery_->IsPresent()) {
            return false;
        }
        int soc = battery_->GetBatteryLevel();
        if (soc < 0) {
            return false;
        }
        level = soc;
        // CW2017 reports no charge state and the Passport has no charge-detect
        // GPIO, so report a plain (discharging) reading.
        charging = false;
        discharging = true;
        return true;
    }
};

DECLARE_BOARD(AiPassportBoard);
