#include "power_manager.h"

#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void PowerManager::_ready() {
    Node* root = get_parent();
    if (!root) {
        UtilityFunctions::printerr("PowerManager: no parent found!");
        return;
    }

    camera_manager = root->get_node<CameraManager>("CameraManager");
    if (!camera_manager)
        UtilityFunctions::printerr("PowerManager: CameraManager not found!");

    door_manager = root->get_node<DoorManager>("DoorManager");
    if (!door_manager)
        UtilityFunctions::printerr("PowerManager: DoorManager not found!");

    night_manager = root->get_node<NightManager>("NightManager");
    if (!night_manager)
        UtilityFunctions::printerr("PowerManager: NightManager not found!");

    CanvasLayer* canvas = memnew(CanvasLayer);
    canvas->set_layer(10);
    add_child(canvas);

    light_overlay = memnew(ColorRect);
    light_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    light_overlay->set_color(Color(0.0f, 0.0f, 0.0f, 0.45f));
    canvas->add_child(light_overlay);

    // Start hidden — overlay only appears when lights are toggled off
    apply_light_state(); 
}

void PowerManager::_process(double delta) {
    if (power_out) return;

    float drain = 0.0f;

    if (lights_on)
        drain += base_drain;

    if (camera_manager && camera_manager->is_open())
        drain += camera_drain;

    if (door_manager) {
        if (door_manager->is_left_closed())  drain += door_drain;
        if (door_manager->is_right_closed()) drain += door_drain;
    }

    if (drain == 0.0f) return;

    power -= drain * static_cast<float>(delta);

    if (power <= 0.0f) {
        power = 0.0f;
        on_power_out();
    }
}

void PowerManager::reset_power() {
    power     = 100.0f;
    power_out = false;
    lights_on = false;

    if (camera_manager) camera_manager->set_power_out(false);
    if (door_manager)   door_manager->set_power_out(false);

    apply_light_state(); 

    UtilityFunctions::print("PowerManager: power reset to 100");
}

void PowerManager::toggle_lights() {
    if (power_out) return;

    lights_on = !lights_on;
    UtilityFunctions::print("PowerManager: lights ", lights_on ? "ON" : "OFF");

    if (!lights_on && camera_manager && camera_manager->is_open())
        camera_manager->close_cameras();

    apply_light_state();
}

void PowerManager::apply_light_state() {
    if (!light_overlay) return;
    lights_on ? light_overlay->hide() : light_overlay->show();
}

void PowerManager::on_power_out() {
    power_out = true;
    lights_on = false;
    UtilityFunctions::print("PowerManager: power out!");

    if (camera_manager) {
        camera_manager->set_power_out(true);
        camera_manager->close_cameras();
    }
    if (door_manager)
        door_manager->set_power_out(true);
    if (night_manager)
        night_manager->notify_power_out();

    apply_light_state();
}

void PowerManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_power"),     &PowerManager::get_power);
    ClassDB::bind_method(D_METHOD("is_power_out"),  &PowerManager::is_power_out);
    ClassDB::bind_method(D_METHOD("are_lights_on"), &PowerManager::are_lights_on);
    ClassDB::bind_method(D_METHOD("toggle_lights"), &PowerManager::toggle_lights);
    ClassDB::bind_method(D_METHOD("reset_power"),   &PowerManager::reset_power);
}