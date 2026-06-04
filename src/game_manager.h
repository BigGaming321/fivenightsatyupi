#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/button.hpp>
#include "camera_manager.h"
#include "power_manager.h"
#include "door_manager.h"
#include "night_manager.h"

using namespace godot;

class GameManager : public Node {
    GDCLASS(GameManager, Node);

private:
    CameraManager*     camera_manager = nullptr;
    PowerManager*      power_manager  = nullptr;
    DoorManager*       door_manager   = nullptr;
    NightManager*      night_manager  = nullptr;

    AudioStreamPlayer* door_sfx   = nullptr;
    AudioStreamPlayer* lights_sfx = nullptr;

    Button* left_door_btn  = nullptr;
    Button* right_door_btn = nullptr;
    Button* lights_btn     = nullptr;

    void set_hud_buttons_visible(bool visible);

public:
    void _ready()               override;
    void _process(double delta) override;

    void on_left_door_pressed();
    void on_right_door_pressed();
    void on_lights_pressed();

protected:
    static void _bind_methods();
};

#endif