#pragma once

#include <algorithm>
#include <cstring>

// Included after AiPassportDisplay: keep the assistant layout separate from
// the board's radio, reader and other standalone application screens.
class AiPassportConversationDisplay : public AiPassportDisplay {
public:
    using AiPassportDisplay::AiPassportDisplay;

    void SetupUI() override {
        if (conversation_title_ != nullptr) {
            return;
        }
        AiPassportDisplay::SetupUI();
        DisplayLockGuard lock(this);
        if (!lock || bottom_bar_ == nullptr || chat_message_label_ == nullptr) {
            return;
        }

        // Reserve the centre of the top row for status and notifications;
        // network and battery icons keep their own space at either edge.
        lv_obj_set_size(top_bar_, width_, 32);
        lv_obj_set_style_pad_ver(top_bar_, 4, 0);
        lv_obj_set_size(status_bar_, width_ - 96, 32);
        lv_obj_set_style_pad_all(status_bar_, 0, 0);
        lv_obj_set_style_radius(status_bar_, 8, 0);
        for (auto label : {status_label_, notification_label_}) {
            lv_obj_set_width(label, width_ - 100);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        }

        auto screen = lv_screen_active();
        conversation_title_ = lv_label_create(screen);
        lv_label_set_text(conversation_title_, "小智 · 对话");
        lv_obj_set_width(conversation_title_, width_ - 76);
        lv_label_set_long_mode(conversation_title_, LV_LABEL_LONG_DOT);
        lv_obj_align(conversation_title_, LV_ALIGN_TOP_LEFT, 64, 48);
        // The expression is a companion avatar, not a full-screen layer.
        lv_obj_set_size(emoji_box_, 40, 40);
        lv_obj_set_style_radius(emoji_box_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(emoji_box_, 2, 0);
        lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(emoji_box_, LV_ALIGN_TOP_LEFT, kMargin, 40);
        lv_obj_center(emoji_label_);
        lv_obj_set_style_transform_scale(emoji_label_, 128, 0);

        question_panel_ = lv_obj_create(screen);
        lv_obj_set_size(question_panel_, width_ - 2 * kMargin, 36);
        lv_obj_align(question_panel_, LV_ALIGN_TOP_MID, 0, kBodyTop);
        lv_obj_set_style_radius(question_panel_, 10, 0);
        lv_obj_set_style_border_width(question_panel_, 0, 0);
        lv_obj_set_style_pad_all(question_panel_, 6, 0);
        lv_obj_remove_flag(question_panel_, LV_OBJ_FLAG_SCROLLABLE);
        question_label_ = lv_label_create(question_panel_);
        lv_obj_set_width(question_label_, width_ - 2 * kMargin - 12);
        lv_obj_set_height(
            question_label_,
            static_cast<LvglTheme*>(current_theme_)->text_font()->font()->line_height);
        lv_label_set_long_mode(question_label_, LV_LABEL_LONG_DOT);
        lv_label_set_text(question_label_, "");
        lv_obj_align(question_label_, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_add_flag(question_panel_, LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_width(bottom_bar_, width_ - 2 * kMargin);
        lv_obj_set_style_pad_all(bottom_bar_, 12, 0);
        lv_obj_set_style_radius(bottom_bar_, 12, 0);
        lv_obj_set_style_border_width(bottom_bar_, 1, 0);
        lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(bottom_bar_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_line_space(chat_message_label_, 4, 0);

        footer_ = lv_label_create(screen);
        lv_obj_set_width(footer_, width_ - 2 * kMargin);
        lv_label_set_long_mode(footer_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(footer_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(footer_, LV_ALIGN_BOTTOM_MID, 0, -8);
        empty_hint_ = lv_label_create(screen);
        lv_obj_set_width(empty_hint_, width_ - 2 * kMargin);
        lv_obj_set_style_text_align(empty_hint_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty_hint_, LV_ALIGN_TOP_MID, 0, kBodyTop + 64);
        SetIdleHintsLocked();
        ApplyConversationThemeLocked();
        LayoutReplyLocked();
    }

    void SetChatMessage(const char* role, const char* content) override {
        DisplayLockGuard lock(this);
        if (!lock || conversation_title_ == nullptr) {
            return;
        }
        content = content != nullptr ? content : "";
        const bool user = role != nullptr && strcmp(role, "user") == 0;
        const bool assistant = role != nullptr && strcmp(role, "assistant") == 0;
        if (user && content[0] != '\0') {
            lv_label_set_text_fmt(question_label_, "我：%s", content);
            if (!hide_subtitle_) {
                lv_obj_remove_flag(question_panel_, LV_OBJ_FLAG_HIDDEN);
            }
            // A new question should not retain the previous turn's answer.
            has_reply_ = false;
            SpiLcdDisplay::SetChatMessage("assistant", "正在思考…");
            lv_label_set_text(conversation_title_, "小智 · 思考");
        } else {
            // Base implementation owns subtitle visibility and cancels the
            // previous scroll animation before replacing its text.
            SpiLcdDisplay::SetChatMessage(role, content);
            has_reply_ = assistant && content[0] != '\0';
            if (content[0] != '\0') {
                lv_label_set_text(conversation_title_, assistant ? "小智 · 回答" : "小智 · 提示");
            }
            if (!assistant) {
                lv_obj_add_flag(question_panel_, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(question_label_, "");
            }
        }
        LayoutReplyLocked();
    }

    void SetStatus(const char* status) override {
        SpiLcdDisplay::SetStatus(status);
        DisplayLockGuard lock(this);
        if (!lock || footer_ == nullptr || status == nullptr) {
            return;
        }
        if (strcmp(status, Lang::Strings::LISTENING) == 0) {
            lv_obj_set_style_border_color(emoji_box_, lv_color_hex(0x43B99A), 0);
            lv_label_set_text(conversation_title_, "小智 · 聆听");
            lv_label_set_text(empty_hint_, "我在听，请说话…");
            lv_label_set_text(footer_, "OK 结束对话");
        } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
            lv_obj_set_style_border_color(emoji_box_, lv_color_hex(0x619BEE), 0);
            lv_label_set_text(conversation_title_, "小智 · 回答");
            lv_label_set_text(footer_, "OK 打断回答");
        } else if (strcmp(status, Lang::Strings::CONNECTING) == 0) {
            lv_obj_set_style_border_color(emoji_box_, lv_color_hex(0xD4A450), 0);
            lv_label_set_text(footer_, "▲▼ 调节音量");
            lv_label_set_text(empty_hint_, "正在连接，请稍候");
        } else {
            lv_obj_set_style_border_color(
                emoji_box_, static_cast<LvglTheme*>(current_theme_)->border_color(), 0);
            lv_label_set_text(conversation_title_, "小智 · 对话");
            SetIdleHintsLocked();
        }
    }

    void ClearChatMessages() override {
        DisplayLockGuard lock(this);
        if (!lock) {
            return;
        }
        // The application clears messages when returning to idle. Leave a
        // completed answer readable until the next question or system reset.
        if (!has_reply_) {
            SpiLcdDisplay::ClearChatMessages();
            if (question_panel_ != nullptr) {
                lv_obj_add_flag(question_panel_, LV_OBJ_FLAG_HIDDEN);
            }
            if (empty_hint_ != nullptr) {
                LayoutReplyLocked();
            }
        }
    }

    void SetEmotion(const char* emotion) override {
        SpiLcdDisplay::SetEmotion(emotion);
        DisplayLockGuard lock(this);
        if (lock && conversation_title_ != nullptr) {
            const auto image = static_cast<const lv_image_dsc_t*>(lv_image_get_src(emoji_image_));
            if (image != nullptr) {
                const int side =
                    std::max(static_cast<int>(image->header.w), static_cast<int>(image->header.h));
                if (side > 0) {
                    lv_image_set_scale(emoji_image_, 28 * 256 / side);
                }
            }
        }
    }

    void SetHideSubtitle(bool hide) override {
        SpiLcdDisplay::SetHideSubtitle(hide);
        DisplayLockGuard lock(this);
        if (lock && question_panel_ != nullptr) {
            if (hide || lv_label_get_text(question_label_)[0] == '\0') {
                lv_obj_add_flag(question_panel_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(question_panel_, LV_OBJ_FLAG_HIDDEN);
            }
            LayoutReplyLocked();
        }
    }

    void SetTheme(Theme* theme) override {
        AiPassportDisplay::SetTheme(theme);
        DisplayLockGuard lock(this);
        if (lock && conversation_title_ != nullptr) {
            ApplyConversationThemeLocked();
            LayoutReplyLocked();
        }
    }

private:
    static constexpr int kMargin = 12;
    static constexpr int kBodyTop = 88;
    lv_obj_t* conversation_title_ = nullptr;
    lv_obj_t* question_panel_ = nullptr;
    lv_obj_t* question_label_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    lv_obj_t* empty_hint_ = nullptr;
    bool has_reply_ = false;

    static void ScrollReply(void* viewport, int32_t offset) {
        auto panel = static_cast<lv_obj_t*>(viewport);
        const auto previous_y = lv_obj_get_scroll_y(panel);
        lv_obj_scroll_to_y(panel, offset, LV_ANIM_OFF);
        if (previous_y == 0 && lv_obj_get_scroll_y(panel) > 0) {
            ESP_LOGI("PassportChat", "Reply scroll started: x=%ld y=%ld",
                     static_cast<long>(lv_obj_get_scroll_x(panel)),
                     static_cast<long>(lv_obj_get_scroll_y(panel)));
        }
    }

    void SetIdleHintsLocked() {
        // The body explains how to start; the footer only describes keys.
        // Keep both initialization and idle/clock updates on this same path.
#ifdef CONFIG_USE_ESP_WAKE_WORD
        lv_label_set_text(empty_hint_, "说“你好小智”\n开始对话");
        lv_label_set_text(footer_, "OK 对话 · ▲▼ 音量");
#else
        lv_label_set_text(empty_hint_, "按 OK\n开始对话");
        lv_label_set_text(footer_, "▲▼ 调节音量");
#endif
    }

    void ApplyConversationThemeLocked() {
        auto theme = static_cast<LvglTheme*>(current_theme_);
        for (auto label : {conversation_title_, question_label_, footer_, empty_hint_}) {
            lv_obj_set_style_text_font(label, theme->text_font()->font(), 0);
            lv_obj_set_style_text_color(label, theme->text_color(), 0);
        }
        lv_obj_set_style_text_color(footer_, theme->system_text_color(), 0);
        lv_obj_set_style_text_color(empty_hint_, theme->system_text_color(), 0);
        lv_obj_set_height(question_label_, theme->text_font()->font()->line_height);
        lv_obj_set_style_bg_color(question_panel_, theme->user_bubble_color(), 0);
        lv_obj_set_style_bg_color(emoji_box_, theme->user_bubble_color(), 0);
        lv_obj_set_style_border_color(emoji_box_, theme->border_color(), 0);
        lv_obj_set_style_bg_color(bottom_bar_, theme->assistant_bubble_color(), 0);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bottom_bar_, theme->border_color(), 0);
    }

    void LayoutReplyLocked() {
        if (hide_subtitle_ || !lv_obj_has_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(empty_hint_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(empty_hint_, LV_OBJ_FLAG_HIDDEN);
        }
        const int top = kBodyTop + (lv_obj_has_flag(question_panel_, LV_OBJ_FLAG_HIDDEN) ? 0 : 44);
        const int panel_height = height_ - top - 40;
        lv_obj_set_height(bottom_bar_, panel_height);
        lv_obj_align(bottom_bar_, LV_ALIGN_TOP_MID, 0, top);
        lv_anim_delete(bottom_bar_, ScrollReply);
        lv_obj_scroll_to_y(bottom_bar_, 0, LV_ANIM_OFF);
        lv_anim_delete(chat_message_label_, nullptr);
        // Label SCROLL mode expands text horizontally and picks a marquee.
        // Keep the label permanently wrapped and at its full natural height;
        // animate the parent viewport so all visible lines move up together.
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
        lv_obj_update_layout(bottom_bar_);
        lv_obj_set_size(chat_message_label_,
                        std::max(1, static_cast<int>(lv_obj_get_content_width(bottom_bar_)) - 6),
                        LV_SIZE_CONTENT);
        lv_obj_align(chat_message_label_, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_update_layout(bottom_bar_);
        const int overflow = std::max(0, static_cast<int>(lv_obj_get_scroll_bottom(bottom_bar_)));
        ESP_LOGI("PassportChat", "Reply viewport=%ldx%ld text_height=%ld vertical_overflow=%d",
                 static_cast<long>(lv_obj_get_content_width(bottom_bar_)),
                 static_cast<long>(lv_obj_get_content_height(bottom_bar_)),
                 static_cast<long>(lv_obj_get_height(chat_message_label_)), overflow);
        if (overflow > 0 && !hide_subtitle_ && !lv_obj_has_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN)) {
            lv_anim_t scroll;
            lv_anim_init(&scroll);
            lv_anim_set_var(&scroll, bottom_bar_);
            lv_anim_set_exec_cb(&scroll, ScrollReply);
            lv_anim_set_values(&scroll, 0, overflow);
            lv_anim_set_duration(&scroll, std::clamp(overflow * 1000 / 18, 3000, 90000));
            lv_anim_set_delay(&scroll, 2000);
            lv_anim_set_repeat_delay(&scroll, 2500);
            lv_anim_set_repeat_count(&scroll, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&scroll, lv_anim_path_linear);
            lv_anim_start(&scroll);
        }
    }
};
