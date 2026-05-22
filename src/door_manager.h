#ifndef DOOR_MANAGER_H
#define DOOR_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include "camera_manager.h"

using namespace godot;

struct Door {
    TextureRect* display  = nullptr;
    bool         is_closed = false;
};

class DoorManager : public Node {
    GDCLASS(DoorManager, Node);

private:
    Door left_door;
    Door right_door;
    bool power_out = false;

    void toggle_door(Door& door);
    CameraManager* camera_manager = nullptr;

public:
    void _ready() override;

    void toggle_left();
    void toggle_right();
    void set_power_out();
    void hide_doors();    
    void restore_doors(); 

    bool is_left_closed()  const { return left_door.is_closed; }
    bool is_right_closed() const { return right_door.is_closed; }

protected:
    static void _bind_methods();
};

#endif