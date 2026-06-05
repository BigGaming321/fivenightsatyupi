#include "camera_manager.h"
#include "door_manager.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

using namespace godot;

void CameraManager::_ready() {
    UtilityFunctions::print("CameraManager _ready CALLED");
    set_process_unhandled_input(true);


    Node* root = get_parent();
    if (!root) { UtilityFunctions::printerr("CameraManager: no parent!"); return; }

    door_manager = root->get_node<DoorManager>("DoorManager");
    if (!door_manager) { UtilityFunctions::printerr("CameraManager: DoorManager not found!"); return; }

    main_feed = root->get_node<TextureRect>("CameraSystem/MainFeed");
    if (!main_feed) { UtilityFunctions::printerr("CameraManager: MainFeed not found!"); return; }

    Node* parent = main_feed->get_parent();
    if (parent) parent->move_child(main_feed, 0);

    camera_feed = root->get_node<TextureRect>("CameraSystem/CameraFeed");
    if (!camera_feed) { UtilityFunctions::printerr("CameraManager: CameraFeed not found!"); return; }

    // Camera view images (shown when a room is selected)
    const char* image_names[5] = {
        "Canteen", "Office", "Library", "Oblesquare", "Hallway"
    };
    float size = 50.0f;
    camera_zones = {
        { Rect2(815.0f - size/2, 539.0f - size/2, size, size), 0 }, // Canteen
        { Rect2(1025.0f - size/2, 557.0f - size/2, size, size), 1 }, // Office
        { Rect2(825.0f - size/2, 442.0f - size/2, size, size), 2 }, // Library
        { Rect2(918.0f - size/2, 442.0f - size/2, size, size), 3 }, // Oblesquare 
        { Rect2(1004.0f - size/2, 351.0f - size/2, size, size), 4 }, // Hallway
    };
    for (int i = 0; i < 5; i++) {
        cam_images[i] = get_node<TextureRect>(image_names[i]);
        if (!cam_images[i]) {
            UtilityFunctions::printerr("CameraManager: image not found: ", image_names[i]);
        } else {
            cam_images[i]->set_visible(false);
        }
    }

    camera_paths = {
        "res://assets/cams/canteen.png",
        "res://assets/cams/office.png",
        "res://assets/cams/library.png",
        "res://assets/cams/oblesquare.png",
        "res://assets/cams/hallway.png"
    };

    Ref<Texture2D> main_tex = ResourceLoader::get_singleton()->load("res://scenes/main.jpg");
    if (main_tex.is_null()) { UtilityFunctions::printerr("CameraManager: failed to load main.jpg"); return; }
    main_feed->set_texture(main_tex);

    camera_feed->hide();
    load_camera(0);

    // Camera switch sound
    cam_switch_sfx = memnew(AudioStreamPlayer);
    {
        Ref<AudioStream> s = ResourceLoader::get_singleton()->load(
            "res://assets/Gameplay music & suffix/cam_switch.mp3");
        if (!s.is_null()) cam_switch_sfx->set_stream(s);
        else UtilityFunctions::printerr("CameraManager: cam_switch.mp3 not found!");
    }
    cam_switch_sfx->set_bus("Master");
    add_child(cam_switch_sfx);
  
    cam_static_sfx = memnew(AudioStreamPlayer);
    {
        Ref<AudioStream> s = ResourceLoader::get_singleton()->load(
            "res://assets/Gameplay music & suffix/cam_static.mp3");
        if (!s.is_null()) cam_static_sfx->set_stream(s);
        else UtilityFunctions::printerr("CameraManager: cam_static.mp3 not found!");
    }
    cam_static_sfx->set_bus("SFX");
    add_child(cam_static_sfx);
}

void CameraManager::_unhandled_input(const Ref<InputEvent>& event) {
    Ref<InputEventMouseButton> mb = event;
    if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MOUSE_BUTTON_LEFT) {
        Vector2 pos = mb->get_position();
        bool clicked_a_camera = false;
        
        // Check if we hit a camera
        for (const auto& zone : camera_zones) {
            if (zone.area.has_point(pos)) {
                switch_camera(zone.camera_index);
                if (!cam_open) open_cameras();
                clicked_a_camera = true;
                break;
            }
        }
        
        // If we didn't hit a camera, but cameras are open, close them (Go Back)
        if (!clicked_a_camera && cam_open) {
            close_cameras();
        }
        get_viewport()->set_input_as_handled();
    }
}

void CameraManager::switch_camera(int index) {
    if (power_out) return;

    for (int i = 0; i < 5; i++) {
        if (cam_images[i]) cam_images[i]->set_visible(false);
    }
    if (cam_images[index]) cam_images[index]->set_visible(true);
    load_camera(index);

    if (cam_switch_sfx && cam_switch_sfx->get_stream().is_valid())
        cam_switch_sfx->play();

    UtilityFunctions::print("Switched to camera: ", index);
}

void CameraManager::load_camera(int index) {
    int count = static_cast<int>(camera_paths.size());
    if (count == 0 || !camera_feed) return;

    current_cam = ((index % count) + count) % count;
    String path(camera_paths[current_cam].c_str());
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
    if (tex.is_null()) { UtilityFunctions::printerr("CameraManager: failed to load: ", path); return; }
    camera_feed->set_texture(tex);
}

void CameraManager::open_cameras() {
    if (cam_open || !camera_feed) return;
    cam_open = true;
    camera_feed->show();
    if (door_manager) door_manager->hide_doors();
}

void CameraManager::close_cameras() {
    if (!cam_open || !camera_feed) return;
    cam_open = false;
    camera_feed->hide();
    if (door_manager) door_manager->restore_doors();
}

void CameraManager::toggle_cameras() {
    if (power_out) return;
    cam_open ? close_cameras() : open_cameras();
}

void CameraManager::next_camera() {
    if (!cam_open || power_out) return;
    load_camera(current_cam + 1);
}

void CameraManager::prev_camera() {
    if (!cam_open || power_out) return;
    load_camera(current_cam - 1);
}

void CameraManager::play_static_sound() {
    if (cam_static_sfx && cam_static_sfx->get_stream().is_valid() && !cam_static_sfx->is_playing())
        cam_static_sfx->play();
}

void CameraManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("open_cameras"),    &CameraManager::open_cameras);
    ClassDB::bind_method(D_METHOD("close_cameras"),   &CameraManager::close_cameras);
    ClassDB::bind_method(D_METHOD("toggle_cameras"),  &CameraManager::toggle_cameras);
    ClassDB::bind_method(D_METHOD("next_camera"),     &CameraManager::next_camera);
    ClassDB::bind_method(D_METHOD("prev_camera"),     &CameraManager::prev_camera);
    ClassDB::bind_method(D_METHOD("is_open"),         &CameraManager::is_open);
    ClassDB::bind_method(D_METHOD("switch_camera", "index"), &CameraManager::switch_camera);
    ClassDB::bind_method(D_METHOD("_input", "event"), &CameraManager::_input);
    ClassDB::bind_method(D_METHOD("reset_for_new_night"), &CameraManager::reset_for_new_night);
    ClassDB::bind_method(D_METHOD("play_static_sound"),   &CameraManager::play_static_sound);
}
