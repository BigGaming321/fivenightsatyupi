#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <vector>
#include <string>
#include <godot_cpp/variant/rect2.hpp>

struct CameraZone {
    godot::Rect2 area;
    int camera_index;
};

using namespace godot;
class DoorManager;

class CameraManager : public Node {
    GDCLASS(CameraManager, Node);
    std::vector<CameraZone> camera_zones;

private:
    TextureRect*   camera_feed  = nullptr;
    TextureRect*   main_feed    = nullptr;
    DoorManager*   door_manager = nullptr;
    TextureRect*   cam_images[5] = {};

    std::vector<std::string> camera_paths;
    int  current_cam = 0;
    bool cam_open    = false;
    bool power_out   = false;

    void load_camera(int index);

public:
    void _ready()   override;
    void _unhandled_input(const Ref<InputEvent>& event) override;

    void open_cameras();
    void close_cameras();
    void toggle_cameras();
    void next_camera();
    void prev_camera();
    void switch_camera(int index);

    bool is_open()          const { return cam_open; }
    void set_power_out(bool value) { power_out = value; }  
    int  get_camera_index() const { return current_cam; }

    void reset_for_new_night() {  
        power_out = false;
        close_cameras();
        load_camera(0);
    }

protected:
    static void _bind_methods();
};

#endif