#include "power_ui.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void PowerUI::_ready() {
    Node* root = get_parent();
    if (!root) {
        UtilityFunctions::printerr("PowerUI: no parent found!");
        return;
    }

    power_manager = root->get_node<PowerManager>("PowerManager");
    if (!power_manager)
        UtilityFunctions::printerr("PowerUI: PowerManager not found!");

    night_manager = root->get_node<NightManager>("NightManager");
    if (!night_manager)
        UtilityFunctions::printerr("PowerUI: NightManager not found!");

    // Timer label
    timer_label = memnew(Label);
    timer_label->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
    timer_label->set_position(Vector2(20, -120));
    timer_label->set_text("12:00 AM");
    add_child(timer_label);

    // Bar background
    bar_bg = memnew(ColorRect);
    bar_bg->set_color(Color(0.15f, 0.15f, 0.15f, 1.0f));
    bar_bg->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
    bar_bg->set_position(Vector2(20, -80));
    bar_bg->set_size(Vector2(240, 28));
    add_child(bar_bg);

    // Bar fill
    bar_fill = memnew(ColorRect);
    bar_fill->set_color(Color(0.2f, 0.85f, 0.2f, 1.0f));
    bar_fill->set_position(Vector2(0, 0));
    bar_fill->set_size(Vector2(240, 28));
    bar_bg->add_child(bar_fill);

    // Percentage label
    pct_label = memnew(Label);
    pct_label->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
    pct_label->set_position(Vector2(20, -48));
    pct_label->set_text("100%");
    add_child(pct_label);

    // Blackout overlay
    dim_overlay = memnew(ColorRect);
    dim_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    dim_overlay->set_color(Color(0.0f, 0.0f, 0.0f, 0.75f));
    dim_overlay->hide();
    add_child(dim_overlay);
}

void PowerUI::update_bar(float pct) {
    float t = pct / 100.0f;

    Color fill_color;
    if (t > 0.5f) {
        float u = (t - 0.5f) * 2.0f;
        fill_color = Color(1.0f - u, 0.85f, 0.2f * u, 1.0f);
    } else {
        float u = t * 2.0f;
        fill_color = Color(0.85f, 0.85f * u, 0.0f, 1.0f);
    }

    bar_fill->set_color(fill_color);
    bar_fill->set_size(Vector2(240.0f * t, 28.0f));
    pct_label->set_text(String::num_int64((int)pct) + "%");
}

void PowerUI::update_timer() {
    if (!night_manager) return;

    // 60 real seconds = 6 in-game hours (12:00 AM -> 6:00 AM = 360 minutes).
    float elapsed = night_manager->get_night_timer();
    float clamped = CLAMP(elapsed, 0.0f, 60.0f);

    int total_minutes = ((int)(clamped * 6.0f) / 10) * 10;
    int hour          = 12 + total_minutes / 60;
    int minute        = total_minutes % 60;
    int display_hour  = (hour == 12) ? 12 : hour % 12;

    String h_str = String::num_int64(display_hour);
    String m_str = (minute < 10)
        ? ("0" + String::num_int64(minute))
        : String::num_int64(minute);

    timer_label->set_text(h_str + ":" + m_str + " AM");
}

void PowerUI::trigger_blackout() {
    blackout_triggered = true;
    dim_overlay->show();
    pct_label->set_text("0%");
    bar_fill->set_size(Vector2(0, 28));
}

void PowerUI::_process(double delta) {
    if (!power_manager) return;

    if (power_manager->is_power_out()) {
        if (!blackout_triggered) trigger_blackout();
    } else {
        update_bar(power_manager->get_power());
    }

    update_timer();
}

void PowerUI::_bind_methods() {}
