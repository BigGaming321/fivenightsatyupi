#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include "camera_manager.h"
#include "power_manager.h"
#include "door_manager.h"
#include "animatronic.h"

using namespace godot;

class GameManager : public Node {
    GDCLASS(GameManager, Node);

private:
    CameraManager*      camera_manager      = nullptr;
    PowerManager*       power_manager       = nullptr;
    DoorManager*        door_manager        = nullptr;
    AnimatronicManager* animatronic_manager = nullptr;

public:
    void _ready() override;
    void _process(double delta) override;

protected:
    static void _bind_methods();
};

#endif