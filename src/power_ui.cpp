#include "power_ui.h"
#include "animatronic.h"

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

    animatronic = root->get_node<Animatronic>("Animatronic");
    if (!animatronic)
        UtilityFunctions::printerr("PowerUI: Animatronic not found!");

    // --- Timer label (above power bar) ---
    timer_label = memnew(Label);
    timer_label->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
    timer_label->set_position(Vector2(20, -120));  // above power bar
    timer_label->set_text("60s");
    add_child(timer_label);

    // --- Bar background (bigger: 240x28) ---
    bar_bg = memnew(ColorRect);
    bar_bg->set_color(Color(0.15f, 0.15f, 0.15f, 1.0f));
    bar_bg->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
    bar_bg->set_position(Vector2(20, -80));
    bar_bg->set_size(Vector2(240, 28));   // wider + taller
    add_child(bar_bg);

    // --- Bar fill ---
    bar_fill = memnew(ColorRect);
    bar_fill->set_color(Color(0.2f, 0.85f, 0.2f, 1.0f));
    bar_fill->set_position(Vector2(0, 0));
    bar_fill->set_size(Vector2(240, 28));
    bar_bg->add_child(bar_fill);

    // --- Percentage label (bigger font size) ---
    pct_label = memnew(Label);
    pct_label->set_anchors_preset(Control::PRESET_BOTTOM_LEFT);
    pct_label->set_position(Vector2(20, -48));
    pct_label->set_text("100%");
    add_child(pct_label);

    // --- Blackout overlay ---
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
    bar_fill->set_size(Vector2(240.0f * t, 28.0f));  // match new size
    pct_label->set_text(String::num_int64((int)pct) + "%");
}

void PowerUI::update_timer() {
    if (!animatronic) return;

    float remaining = 60.0f - animatronic->get_game_timer();
    if (remaining < 0.0f) remaining = 0.0f;

    timer_label->set_text("Time: " + String::num_int64((int)remaining) + "s");
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