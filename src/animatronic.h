#ifndef ANIMATRONIC_H
#define ANIMATRONIC_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include "door_manager.h"
#include "camera_manager.h"
#include <vector>

using namespace godot;

enum class AnimatronicState {
    IDLE,
    MOVING,
    AT_DOOR,
    JUMPSCARING,
    GAME_OVER,
    YOU_WIN
};

class Animatronic : public Node {
    GDCLASS(Animatronic, Node);

private:
    DoorManager*       door_manager      = nullptr;
    CameraManager*     camera_manager    = nullptr;

    // camera feed overlay (shows animatronic on cam)
    TextureRect*       cam_overlay       = nullptr;
    CanvasLayer*       cam_canvas        = nullptr;

    // jumpscare
    TextureRect*       jumpscare_image   = nullptr;
    AudioStreamPlayer* jumpscare_audio   = nullptr;
    CanvasLayer*       jumpscare_canvas  = nullptr;

    // end screens
    CanvasLayer*       gameover_canvas   = nullptr;
    TextureRect*       gameover_image    = nullptr;
    CanvasLayer*       youwin_canvas     = nullptr;
    TextureRect*       youwin_image      = nullptr;

    // move timer
    Timer*             move_timer        = nullptr;

    // rng
    Ref<RandomNumberGenerator> rng;
    std::vector<int> all_cams;
    std::vector<int> attack_cams;

    AnimatronicState state       = AnimatronicState::IDLE;
    int  current_cam             = 0;
    int  target_door             = -1;
    bool power_out               = false;

    // tuning
    float move_interval_min  = 8.0f;
    float move_interval_max  = 15.0f;
    float jumpscare_duration = 2.0f;

    // jumpscare state
    float jumpscare_timer    = 0.0f;

    // game timer
    float game_duration      = 60.0f;
    float game_timer         = 0.0f;
    bool  game_over          = false;
    bool  game_won           = false;

    // give it time to counter
    bool  waiting_at_door = false;
    float door_timer      = 0.0f;

    // troll music heehee
    AudioStreamPlayer* troll_audio     = nullptr;
    float troll_timer                  = 0.0f;
    float troll_interval_min           = 30.0f; 
    float troll_interval_max           = 90.0f;

    void move_to_next_cam();
    void attack_door();
    void trigger_jumpscare();
    void end_jumpscare();
    void schedule_next_move();
    void refresh_cam_overlay();
    void hide_cam_overlay();
    void trigger_game_over();
    void trigger_you_win();

public:
    void _ready()  override;
    void _process(double delta) override;

    void set_power_out() { power_out = true; }
    void play_troll();
    int  get_current_cam() const { return current_cam; }
    bool is_game_over()   const { return game_over; }
    bool is_game_won()    const { return game_won; }
    float get_game_timer() const { return game_timer; }

protected:
    static void _bind_methods();
};

#endif