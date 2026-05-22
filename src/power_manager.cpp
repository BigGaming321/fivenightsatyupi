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
    if (!camera_manager) {
        UtilityFunctions::printerr("PowerManager: CameraManager not found!");
        return;
    }
    door_manager = root->get_node<DoorManager>("DoorManager");
    if (!door_manager)
        UtilityFunctions::printerr("PowerManager: DoorManager not found!");

    animatronic = root->get_node<Animatronic>("Animatronic");
    if (!animatronic)
        UtilityFunctions::printerr("PowerManager: Animatronic not found!");

    // CanvasLayer so the overlay actually renders on top of everything
    CanvasLayer* canvas = memnew(CanvasLayer);
    canvas->set_layer(10);  // above game, below PowerUI's blackout
    add_child(canvas);

    light_overlay = memnew(ColorRect);
    light_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    light_overlay->set_color(Color(0.0f, 0.0f, 0.0f, 0.45f));
    canvas->add_child(light_overlay);  // add to canvas, not root

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

    // no drain at all only if both lights off AND cams closed AND DOORS
    if (drain == 0.0f) return;

    power -= drain * static_cast<float>(delta);

    if (power <= 0.0f) {
        power = 0.0f;
        on_power_out();
    }
}

void PowerManager::toggle_lights() {
    if (power_out) return;  // can't toggle lights if power is out

    lights_on = !lights_on;
    UtilityFunctions::print("PowerManager: lights ", lights_on ? "ON" : "OFF");

    // Cameras can't be open without lights
    if (!lights_on && camera_manager && camera_manager->is_open()) {
        camera_manager->close_cameras();
    }

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
        camera_manager->set_power_out();   // lock camera input
        camera_manager->close_cameras();   // close the overlay
    }
    if (door_manager)
        door_manager->set_power_out(); 
    if (animatronic)
    animatronic->set_power_out();
    
    apply_light_state();
}

void PowerManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_power"),     &PowerManager::get_power);
    ClassDB::bind_method(D_METHOD("is_power_out"),  &PowerManager::is_power_out);
    ClassDB::bind_method(D_METHOD("are_lights_on"), &PowerManager::are_lights_on);
    ClassDB::bind_method(D_METHOD("toggle_lights"), &PowerManager::toggle_lights);
}