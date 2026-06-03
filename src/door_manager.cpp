#include "door_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void DoorManager::_ready() {
    Node* root = get_parent();
    if (!root) {
        UtilityFunctions::printerr("DoorManager: no parent found!");
        return;
    }
    camera_manager = root->get_node<CameraManager>("CameraManager");
    if (!camera_manager){
        UtilityFunctions::printerr("DoorManager: CameraManager not found!");
        return;
    }

    left_door.display  = root->get_node<TextureRect>("DoorSystem/LeftDoor");
    right_door.display = root->get_node<TextureRect>("DoorSystem/RightDoor");

    if (!left_door.display){
        UtilityFunctions::printerr("DoorManager: LeftDoor not found!");
        return;
    }
    if (!right_door.display){
        UtilityFunctions::printerr("DoorManager: RightDoor not found!");
        return;
    }

    left_door.display->hide();
    right_door.display->hide();
}

void DoorManager::toggle_door(Door& door) {
    if (!door.display || power_out) return;

    if (camera_manager && camera_manager->is_open()) {
        UtilityFunctions::print("DoorManager: cameras open, doors locked");
        return;
    }
    door.is_closed = !door.is_closed;
    door.is_closed ? door.display->show() : door.display->hide();

    UtilityFunctions::print("DoorManager: door is now ", door.is_closed ? "CLOSED" : "OPEN");
}

void DoorManager::toggle_left()  { toggle_door(left_door);  }
void DoorManager::toggle_right() { toggle_door(right_door); }

void DoorManager::set_power_out(bool value) {
    power_out = value;

    if (value) {
        // power lost — force both doors open and hidden
        left_door.is_closed  = false;
        right_door.is_closed = false;
        if (left_door.display)  left_door.display->hide();
        if (right_door.display) right_door.display->hide();
    }
    // if value is false (night reset), doors stay closed/open as-is
    // since reset_power() is called at night start when both are already open
}
void DoorManager::hide_doors() {
    if (left_door.display)  left_door.display->hide();
    if (right_door.display) right_door.display->hide();
}

void DoorManager::restore_doors() {
    if (left_door.display)
        left_door.is_closed ? left_door.display->show() : left_door.display->hide();
    if (right_door.display)
        right_door.is_closed ? right_door.display->show() : right_door.display->hide();
}

void DoorManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("toggle_left"),    &DoorManager::toggle_left);
    ClassDB::bind_method(D_METHOD("toggle_right"),   &DoorManager::toggle_right);
    ClassDB::bind_method(D_METHOD("is_left_closed"), &DoorManager::is_left_closed);
    ClassDB::bind_method(D_METHOD("is_right_closed"),&DoorManager::is_right_closed);
}