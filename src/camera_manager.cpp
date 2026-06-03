#include "camera_manager.h"
#include "door_manager.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void CameraManager::_ready() {
    Node* root = get_parent();
    if (!root) {
        UtilityFunctions::printerr("CameraManager: no parent found!");
        return;
    }

    door_manager = root->get_node<DoorManager>("DoorManager");
    if (!door_manager){
        UtilityFunctions::printerr("CameraManager: DoorManager not found!");
        return;
    }
    // Main feed — always visible, shows main.png
    main_feed = root->get_node<TextureRect>("CameraSystem/MainFeed");
    if (!main_feed) {
        UtilityFunctions::printerr("CameraManager: MainFeed not found!");
        return;
    }
    Node* parent = main_feed->get_parent();
    if (parent) {
        parent->move_child(main_feed, 0);
    }
    // Camera overlay — hidden until Tab is pressed
    camera_feed = root->get_node<TextureRect>("CameraSystem/CameraFeed");
    if (!camera_feed) {
        UtilityFunctions::printerr("CameraManager: CameraFeed not found!");
        return;
    }
    camera_paths = {
        "res://assets/cams/canteen.png",
        "res://assets/cams/hallway.png",
        "res://assets/cams/library.png",
        "res://assets/cams/oblesquare.png",
        "res://assets/cams/office.png",
    };

    // Load main view
    Ref<Texture2D> main_tex = ResourceLoader::get_singleton()->load("res://scenes/main.jpg");
    if (main_tex.is_null()) {
        UtilityFunctions::printerr("CameraManager: failed to load main.jpg");
        return;
    }
    main_feed->set_texture(main_tex);

    // Start with overlay hidden
    camera_feed->hide();
    load_camera(0);
}

void CameraManager::load_camera(int index) {
    int count = static_cast<int>(camera_paths.size());
    if (count == 0 || !camera_feed) return;

    current_cam = ((index % count) + count) % count;

    String path(camera_paths[current_cam].c_str());
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);

    if (tex.is_null()) {
        UtilityFunctions::printerr("CameraManager: failed to load texture: ", path);
        return;
    }

    camera_feed->set_texture(tex);
}

void CameraManager::open_cameras() {
    if (cam_open || !camera_feed) return;
    cam_open = true;
    camera_feed->show();
    if (door_manager) {
        door_manager->hide_doors();
    }
}

void CameraManager::close_cameras() {
    if (!cam_open || !camera_feed) return;
    cam_open = false;
    camera_feed->hide();
    if (door_manager) {
        door_manager->restore_doors();
    }
}
void CameraManager::next_camera() {
    if (!cam_open || power_out) return;
    load_camera(current_cam + 1);
}

void CameraManager::prev_camera() {
    if (!cam_open || power_out) return;
    load_camera(current_cam - 1);
}

void CameraManager::toggle_cameras() {
    if (power_out) return;  // can't open cams if power is out
    cam_open ? close_cameras() : open_cameras();
}
void CameraManager::reset_for_new_night() {
    power_out = false;
    if (cam_open) close_cameras();
}
void CameraManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("open_cameras"),   &CameraManager::open_cameras);
    ClassDB::bind_method(D_METHOD("close_cameras"),  &CameraManager::close_cameras);
    ClassDB::bind_method(D_METHOD("toggle_cameras"), &CameraManager::toggle_cameras);
    ClassDB::bind_method(D_METHOD("next_camera"),    &CameraManager::next_camera);
    ClassDB::bind_method(D_METHOD("prev_camera"),    &CameraManager::prev_camera);
    ClassDB::bind_method(D_METHOD("is_open"),        &CameraManager::is_open);
}