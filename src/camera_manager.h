#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <vector>
#include <string>

using namespace godot;
class DoorManager;

class CameraManager : public Node {
    GDCLASS(CameraManager, Node);

private:
    TextureRect* camera_feed = nullptr;  // the overlay feed
    TextureRect* main_feed   = nullptr;  // the always-visible main view
    DoorManager* door_manager = nullptr;

    std::vector<std::string> camera_paths;
    int  current_cam   = 0;
    bool cam_open      = false;
    bool power_out  = false;  // add this

    void load_camera(int index);
public:
    void _ready() override;

    void open_cameras();
    void close_cameras();
    void toggle_cameras();
    void next_camera();
    void prev_camera();

    bool is_open() const { return cam_open; }
    void set_power_out(bool value) { power_out = value; }
    int get_camera_index() const { return current_cam; }
    void reset_for_new_night();


protected:
    static void _bind_methods();
};

#endif