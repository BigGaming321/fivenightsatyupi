#include "game_manager.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void GameManager::_ready() {
    set_process(true);
    Node* root = get_parent();
    if (!root) { UtilityFunctions::printerr("GameManager: no parent found!"); return; }

    camera_manager = root->get_node<CameraManager>("CameraManager");
    power_manager  = root->get_node<PowerManager>("PowerManager");
    door_manager   = root->get_node<DoorManager>("DoorManager");
    night_manager  = root->get_node<NightManager>("NightManager");

    if (!camera_manager) UtilityFunctions::printerr("GameManager: CameraManager not found!");
    if (!power_manager)  UtilityFunctions::printerr("GameManager: PowerManager not found!");
    if (!door_manager)   UtilityFunctions::printerr("GameManager: DoorManager not found!");
    if (!night_manager)  UtilityFunctions::printerr("GameManager: NightManager not found!");

    // Door SFX
    door_sfx = memnew(AudioStreamPlayer);
    Ref<AudioStream> door_snd = ResourceLoader::get_singleton()->load(
        "res://assets/Gameplay music & suffix/doors_sfx.mp3"); // Changed door_sfx to doors_sfx
    if (!door_snd.is_null()) door_sfx->set_stream(door_snd);
    else UtilityFunctions::printerr("GameManager: door sound not found!");
    door_sfx->set_bus("SFX");
    add_child(door_sfx);

    // Lights SFX
    lights_sfx = memnew(AudioStreamPlayer);
    Ref<AudioStream> lights_snd = ResourceLoader::get_singleton()->load(
        "res://assets/Gameplay music & suffix/lights_sfx.mp3");
    if (!lights_snd.is_null()) lights_sfx->set_stream(lights_snd);
    else UtilityFunctions::printerr("GameManager: lights sound not found!");
    lights_sfx->set_bus("SFX");
    add_child(lights_sfx);

    // Connect HUD buttons
    auto find_and_connect = [&](const char* name, auto method, Button*& out) {
        Node* found = root->find_child(name, true, false);
        out = found ? Object::cast_to<Button>(found) : nullptr;
        if (out) {
            out->connect("pressed", callable_mp(this, method));
            UtilityFunctions::print("GameManager: connected ", name);
        } else {
            UtilityFunctions::printerr("GameManager: button not found -- ", name);
        }
    };

    find_and_connect("LeftDoorButton",  &GameManager::on_left_door_pressed,  left_door_btn);
    find_and_connect("RightDoorButton", &GameManager::on_right_door_pressed, right_door_btn);
    find_and_connect("LightsButton",    &GameManager::on_lights_pressed,     lights_btn);
}

void GameManager::_process(double delta) {
    if (!camera_manager || !power_manager || !door_manager || !night_manager) return;

    bool night_active = night_manager->is_night_active();
    bool hud_visible  = night_active
                        && !camera_manager->is_open()
                        && !power_manager->is_power_out();

    if (left_door_btn)  left_door_btn->set_visible(hud_visible);
    if (right_door_btn) right_door_btn->set_visible(hud_visible);
    if (lights_btn)     lights_btn->set_visible(hud_visible);

    Input* input = Input::get_singleton();
    if (input->is_action_just_pressed("cam_toggle"))    camera_manager->toggle_cameras();
    if (input->is_action_just_pressed("lights_toggle")) power_manager->toggle_lights();
    if (input->is_action_just_pressed("cam_right"))     camera_manager->next_camera();
    if (input->is_action_just_pressed("cam_left"))      camera_manager->prev_camera();
    if (input->is_action_just_pressed("door_left"))     door_manager->toggle_left();
    if (input->is_action_just_pressed("door_right"))    door_manager->toggle_right();
    if (input->is_action_just_pressed("troll_sound"))
        if (night_manager) night_manager->play_troll();
}

void GameManager::on_left_door_pressed() {
    if (door_sfx && door_sfx->is_inside_tree()) door_sfx->play();
    if (door_manager) door_manager->toggle_left();
}

void GameManager::on_right_door_pressed() {
    if (door_sfx && door_sfx->is_inside_tree()) door_sfx->play();
    if (door_manager) door_manager->toggle_right();
}

void GameManager::on_lights_pressed() {
    if (lights_sfx && lights_sfx->is_inside_tree()) lights_sfx->play();
    if (power_manager) power_manager->toggle_lights();
}

void GameManager::set_hud_buttons_visible(bool visible) {
    if (left_door_btn)  left_door_btn->set_visible(visible);
    if (right_door_btn) right_door_btn->set_visible(visible);
    if (lights_btn)     lights_btn->set_visible(visible);
}

void GameManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("on_left_door_pressed"),  &GameManager::on_left_door_pressed);
    ClassDB::bind_method(D_METHOD("on_right_door_pressed"), &GameManager::on_right_door_pressed);
    ClassDB::bind_method(D_METHOD("on_lights_pressed"),     &GameManager::on_lights_pressed);
}