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
// AnimatronicState

enum class AnimatronicState {
    INACTIVE,
    IDLE,
    MOVING,
    AT_DOOR,
    JUMPSCARING,
    GAME_OVER,
    YOU_WIN
};

// Animatronic — abstract base class
//
// Subclasses override:
//   setup()               — fill all_cams / attack_cams and tuning values
//   get_cam_image_path()  — res:// path to the camera overlay sprite
//   get_jumpscare_path()  — res:// path to the fullscreen jumpscare image
//   get_name_str()        — display name used in debug prints
//   get_active_night()    — which night this animatronic first appears on
//   on_move()             — called every time the animatronic picks a new cam
//   can_move()            — return false to block movement
//   doors_are_useless()   — return true to ignore closed doors (Ryan)
//   is_repelled_by_light(bool left) — return true to retreat on light

class Animatronic : public Node {
    GDCLASS(Animatronic, Node);

public:
    void _ready()               override;
    void _process(double delta) override;

    // Called by NightManager after the player presses Start.
    void activate();

    // Stops all timers and hides overlays without destroying the node.
    void deactivate();

    // Full state reset so the node can be re-activated on a later night.
    void reset_for_next_night();

    // Broadcast events from NightManager.
    void notify_power_out();
    void notify_light_on(bool left_side);

    // Shared end-screen images owned by NightManager.
    void set_gameover_image(TextureRect* img) { gameover_image = img; }
    void set_youwin_image(TextureRect*   img) { youwin_image   = img; }

    bool  is_game_over()    const { return game_over; }
    bool  is_game_won()     const { return game_won;  }
    bool  is_inactive()     const { return state == AnimatronicState::INACTIVE; }
    int   get_current_cam() const { return current_cam; }
    float get_game_timer()  const;   // defined in animatronic.cpp

    virtual int get_active_night() = 0;

    void play_troll();    // callable by GameManager
    void set_power_out(); // callable by PowerManager

protected:
    
    // Pure-virtual interface — subclasses must implement
    virtual void        setup()              = 0;
    virtual const char* get_cam_image_path() = 0;
    virtual const char* get_jumpscare_path() = 0;
    virtual const char* get_name_str()       = 0;

    // Optional overrides
    virtual const char* get_jumpscare_audio_path() { return "res://assets/music/jumpscare.mp3"; }
    virtual const char* get_troll_audio_path()     { return "res://assets/music/i_feel_good.mp3"; }

    // Behaviour hooks
    virtual void on_move()                            {}
    virtual bool can_move()                           { return true; }
    virtual bool doors_are_useless()                  { return false; }
    virtual bool is_repelled_by_light(bool /*left*/)  { return false; }

    // Utility for subclasses
    bool is_watched_on_cam() const;

    // Tuning — filled in by setup()
    std::vector<int> all_cams    = {0, 1, 2, 3, 4};
    std::vector<int> attack_cams = {0, 4};

    float move_interval_min  = 8.0f;
    float move_interval_max  = 15.0f;
    float door_reaction_time = 5.0f;
    float jumpscare_duration = 2.0f;
    float troll_interval_min = 120.0f;  
    float troll_interval_max = 300.0f;  
    int   cam_layer          = 10;
    int   jumpscare_layer    = 15;

    Ref<RandomNumberGenerator> rng;

    void emit_power_drained(float amount);

    static void _bind_methods();

private:
    DoorManager*   door_manager   = nullptr;
    CameraManager* camera_manager = nullptr;

    CanvasLayer*       cam_canvas      = nullptr;
    TextureRect*       cam_overlay     = nullptr;
    TextureRect*       static_overlay  = nullptr;
    float              static_timer    = 0.0f;

    CanvasLayer*       jumpscare_canvas = nullptr;
    TextureRect*       jumpscare_image  = nullptr;
    AudioStreamPlayer* jumpscare_audio  = nullptr;
    AudioStreamPlayer* troll_audio      = nullptr;

    TextureRect* gameover_image = nullptr;
    TextureRect* youwin_image   = nullptr;

    Timer*           move_timer    = nullptr;
    AnimatronicState state         = AnimatronicState::INACTIVE;
    int              current_cam   = 0;
    int              target_door   = -1;
    bool             power_out     = false;

    bool  waiting_at_door = false;
    float door_timer      = 0.0f;
    float jumpscare_timer = 0.0f;
    float troll_timer     = 0.0f;

    bool  game_over = false;
    bool  game_won  = false;

    float* shared_game_timer = nullptr;
    float  game_duration     = 60.0f;

    void schedule_next_move();
    void move_to_next_cam();
    void attack_door();
    void trigger_jumpscare();
    void end_jumpscare();
    void refresh_cam_overlay();
    void hide_cam_overlay();
    void trigger_game_over();
    void trigger_you_win();
    void set_static_overlay(TextureRect* overlay, float duration);

    friend class NightManager;
    friend class Librarian;

    void set_shared_timer(float* ptr, float duration) {
        shared_game_timer = ptr;
        game_duration     = duration;
    }

    float spawn_delay_min = 5.0f;   
    float spawn_delay_max = 30.0f; 

    void do_spawn();
};

// Concrete animatronics


// Dean — Night 1.  Left door only.

class Dean : public Animatronic {
    GDCLASS(Dean, Animatronic);
protected:
    void        setup()              override;
    const char* get_cam_image_path() override { return "res://assets/images/dean.png"; }
    const char* get_jumpscare_path() override { return "res://assets/images/dean_jumpscare.png"; }
    const char* get_name_str()       override { return "Dean"; }
    int         get_active_night()   override { return 1; }
    static void _bind_methods() {}
};

// Student — Night 2.  Right door only.

class Student : public Animatronic {
    GDCLASS(Student, Animatronic);
protected:
    void        setup()              override;
    const char* get_cam_image_path() override { return "res://assets/images/student.png"; }
    const char* get_jumpscare_path() override { return "res://assets/images/student_jumpscare.png"; }
    const char* get_name_str()       override { return "Student"; }
    int         get_active_night()   override { return 2; }
    static void _bind_methods() {}
};

// Librarian — Night 2.  Blinds cameras with static on every move.

class Librarian : public Animatronic {
    GDCLASS(Librarian, Animatronic);
public:
    void _ready() override; 
protected:
    void        setup()              override;
    const char* get_cam_image_path() override { return "res://assets/images/librarian.png"; }
    const char* get_jumpscare_path() override { return "res://assets/images/librarian_jumpscare.png"; }
    const char* get_name_str()       override { return "Librarian"; }
    int         get_active_night()   override { return 2; }
    void        on_move()            override;
    static void _bind_methods() {}
private:
    float        camera_blind_duration = 5.0f;
    TextureRect* my_static_overlay     = nullptr;
};

// Janitor — Night 3.  Drains power on every move.

class Janitor : public Animatronic {
    GDCLASS(Janitor, Animatronic);
protected:
    void        setup()              override;
    const char* get_cam_image_path() override { return "res://assets/images/janitor.png"; }
    const char* get_jumpscare_path() override { return "res://assets/images/janitor_jumpscare.png"; }
    const char* get_name_str()       override { return "Janitor"; }
    int         get_active_night()   override { return 3; }
    void        on_move()            override;
    static void _bind_methods() {}
private:
    float power_drain_amount = 7.0f;
};

// Oble — Night 4.  Statue: frozen while the player watches this cam.
class Oble : public Animatronic {
    GDCLASS(Oble, Animatronic);
protected:
    void        setup()              override;
    const char* get_cam_image_path() override { return "res://assets/images/oble.png"; }
    const char* get_jumpscare_path() override { return "res://assets/images/oble_jumpscare.png"; }
    const char* get_name_str()       override { return "Oble"; }
    int         get_active_night()   override { return 4; }
    bool        can_move()           override; // freezes when watched
    static void _bind_methods() {}
};


// RyanAnimatronic — Night 5.
//   Fast off-camera, stops on-camera, ignores doors, retreats from light.

class RyanAnimatronic : public Animatronic {
    GDCLASS(RyanAnimatronic, Animatronic);
protected:
    void        setup()                      override;
    const char* get_cam_image_path()         override { return "res://assets/images/ryan.png"; }
    const char* get_jumpscare_path()         override { return "res://assets/images/ryan_jumpscare.png"; }
    const char* get_name_str()               override { return "Ryan"; }
    int         get_active_night()           override { return 5; }
    bool        can_move()                   override;
    bool        doors_are_useless()          override { return true; }
    bool        is_repelled_by_light(bool l) override;
    static void _bind_methods() {}
};

#endif // ANIMATRONIC_H
