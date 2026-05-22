#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include "camera_manager.h"
#include "door_manager.h"
#include "animatronic.h"


using namespace godot;

class PowerManager : public Node {
    GDCLASS(PowerManager, Node);

private:
    CameraManager* camera_manager = nullptr;
    DoorManager* door_manager = nullptr;
    ColorRect*     light_overlay  = nullptr;  // dark tint over scene when lights off
    Animatronic* animatronic = nullptr;

    float power        = 100.0f;
    float base_drain   = 2.0f;   // % per second, lights on
    float camera_drain = 3.0f;   // additional % per second, cams open
    float door_drain = 1.5f;
    // lights off = zero drain

    bool power_out     = false;
    bool lights_on     = false;  // off by default

    void on_power_out();
    void apply_light_state();

public:
    void _ready() override;
    void _process(double delta) override;

    void toggle_lights();

    float get_power()      const { return power; }
    bool  is_power_out()   const { return power_out; }
    bool  are_lights_on()  const { return lights_on; }

protected:
    static void _bind_methods();
};

#endif