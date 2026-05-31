#include "game_manager.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void GameManager::_ready() {
    set_process(true);
    Node* root = get_parent();
    if (!root) {
        UtilityFunctions::printerr("GameManager: no parent found!");
        return;
    }

    camera_manager = root->get_node<CameraManager>("CameraManager");
    if (!camera_manager) {
        UtilityFunctions::printerr("GameManager: CameraManager not found!");
        return;
    }

    power_manager = root->get_node<PowerManager>("PowerManager");
    if (!power_manager) {
        UtilityFunctions::printerr("GameManager: PowerManager not found!");
        return;
    }

    door_manager = root->get_node<DoorManager>("DoorManager");
    if (!door_manager) {
        UtilityFunctions::printerr("GameManager: DoorManager not found!");
        return;
    }

    night_manager = root->get_node<NightManager>("NightManager");
    if (!night_manager)
        UtilityFunctions::printerr("GameManager: NightManager not found!");
}

void GameManager::_process(double delta) {
    if (!camera_manager || !power_manager || !door_manager) return;

    Input* input = Input::get_singleton();

    if (input->is_action_just_pressed("cam_toggle"))
        camera_manager->toggle_cameras();

    if (input->is_action_just_pressed("lights_toggle"))
        power_manager->toggle_lights();

    if (input->is_action_just_pressed("cam_right"))
        camera_manager->next_camera();

    if (input->is_action_just_pressed("cam_left"))
        camera_manager->prev_camera();

    if (input->is_action_just_pressed("door_left"))
        door_manager->toggle_left();

    if (input->is_action_just_pressed("door_right"))
        door_manager->toggle_right();

    if (input->is_action_just_pressed("troll_sound"))
        if (night_manager) night_manager->play_troll();
}

void GameManager::_bind_methods() {}