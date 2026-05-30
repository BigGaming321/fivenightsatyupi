#ifndef ANIMATRONIC_H
#define ANIMATRONIC_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/button.hpp>
#include "door_manager.h"
#include "camera_manager.h"
#include <vector>

using namespace godot;

// ===========================================================================
// AnimatronicState
// ===========================================================================

enum class AnimatronicState {
    INACTIVE,
    IDLE,
    MOVING,
    AT_DOOR,
    JUMPSCARING,
    GAME_OVER,
    YOU_WIN
};

// ===========================================================================
// Animatronic — abstract base class
//
// Subclasses override:
//   setup_cams()          — fill all_cams / attack_cams and tuning values
//   get_cam_image_path()  — res:// path to the camera overlay sprite
//   get_jumpscare_path()  — res:// path to the fullscreen jumpscare image
//   get_name_str()        — display name used in debug prints
//   get_active_night()    — which night this animatronic wakes up on
//   on_move()             — called every time the animatronic picks a new cam
//                           (override to add power drain, camera blind, etc.)
//   can_move()            — return false to block movement (freeze-on-cam)
//   doors_are_useless()   — return true to ignore closed doors (Ryan)
//   is_repelled_by_light(bool left) — return true to retreat on light
// ===========================================================================

class Animatronic : public Node {
    GDCLASS(Animatronic, Node);

public:
    void _ready()           override;
    void _process(double delta) override;

    // Called by AnimatronicManager after the player presses Start.
    void activate();

    // Called by AnimatronicManager to broadcast events.
    void notify_power_out();
    void notify_light_on(bool left_side);

    // Shared end-screen nodes owned by AnimatronicManager.
    void set_gameover_image(TextureRect* img) { gameover_image = img; }
    void set_youwin_image(TextureRect*   img) { youwin_image   = img; }

    bool is_game_over() const { return game_over; }
    bool is_game_won()  const { return game_won;  }
    bool is_inactive()  const { return state == AnimatronicState::INACTIVE; }
    int  get_current_cam() const { return current_cam; }
    float get_game_timer() const { return game_timer; }
    virtual int get_active_night() = 0;

    void play_troll();       // callable by GameManager
    void set_power_out();    // callable by PowerManager (replaces notify_power_out)

protected:
    // -----------------------------------------------------------------------
    // Interface for subclasses
    // -----------------------------------------------------------------------
    virtual void        setup()                  = 0;
    virtual const char* get_cam_image_path()     = 0;
    virtual const char* get_jumpscare_path()     = 0;
    virtual const char* get_jumpscare_audio_path() { return "res://assets/music/jumpscare.mp3"; }
    virtual const char* get_troll_audio_path()     { return "res://assets/music/i_feel_good.mp3"; }
    virtual const char* get_name_str()           = 0;

    // Behaviour hooks — override as needed
    virtual void on_move()                        {}
    virtual bool can_move()                       { return true; }
    virtual bool doors_are_useless()              { return false; }
    virtual bool is_repelled_by_light(bool /*left*/) { return false; }

    // Available to subclasses
    bool is_watched_on_cam() const;

    // Data subclasses fill in setup()
    std::vector<int> all_cams    = {0, 1, 2, 3, 4};
    std::vector<int> attack_cams = {0, 4};

    float move_interval_min  = 8.0f;
    float move_interval_max  = 15.0f;
    float door_reaction_time = 5.0f;
    float jumpscare_duration = 2.0f;
    float troll_interval_min = 30.0f;
    float troll_interval_max = 90.0f;
    int   cam_layer          = 10;
    int   jumpscare_layer    = 15;

    // RNG — available to subclasses (e.g. for custom movement)
    Ref<RandomNumberGenerator> rng;

    // Emitting the power_drained signal from Janitor subclass
    void emit_power_drained(float amount);

    static void _bind_methods();

private:
    // ---- external refs ----------------------------------------------------
    DoorManager*   door_manager   = nullptr;
    CameraManager* camera_manager = nullptr;

    // ---- visuals ----------------------------------------------------------
    CanvasLayer*       cam_canvas      = nullptr;
    TextureRect*       cam_overlay     = nullptr;
    TextureRect*       static_overlay  = nullptr; // set by Librarian subclass
    float              static_timer    = 0.0f;

    CanvasLayer*       jumpscare_canvas = nullptr;
    TextureRect*       jumpscare_image  = nullptr;
    AudioStreamPlayer* jumpscare_audio  = nullptr;
    AudioStreamPlayer* troll_audio      = nullptr;

    TextureRect* gameover_image = nullptr;
    TextureRect* youwin_image   = nullptr;

    // ---- state ------------------------------------------------------------
    Timer*           move_timer    = nullptr;
    AnimatronicState state         = AnimatronicState::INACTIVE;
    int              current_cam   = 0;
    int              target_door   = -1;
    bool             power_out     = false;

    bool  waiting_at_door = false;
    float door_timer      = 0.0f;
    float jumpscare_timer = 0.0f;

    float game_duration = 60.0f;  // 1 minute per night
    float game_timer    = 0.0f;
    bool  game_over     = false;
    bool  game_won      = false;

    float troll_timer = 0.0f;

    // ---- internal helpers -------------------------------------------------
    void schedule_next_move();
    void move_to_next_cam();
    void attack_door();
    void trigger_jumpscare();
    void end_jumpscare();
    void refresh_cam_overlay();
    void hide_cam_overlay();
    void trigger_game_over();
    void trigger_you_win();

    // Librarian uses this to hand the static_overlay pointer back to base
    friend class Librarian;
    void set_static_overlay(TextureRect* overlay, float duration);
};

// ===========================================================================
// Subclass declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// Dean — Night 1.  Attacks left door only.  No special behaviour.
// ---------------------------------------------------------------------------
class Dean : public Animatronic {
    GDCLASS(Dean, Animatronic);
protected:
    void        setup()                override;
    const char* get_cam_image_path()   override { return "res://assets/images/dean.png"; }
    const char* get_jumpscare_path()   override { return "res://assets/images/dean_jumpscare.png"; }
    const char* get_name_str()         override { return "Dean"; }
    int         get_active_night()     override { return 1; }
    static void _bind_methods() {}
};

// ---------------------------------------------------------------------------
// Student — Night 2.  Attacks right door only.
// ---------------------------------------------------------------------------
class Student : public Animatronic {
    GDCLASS(Student, Animatronic);
protected:
    void        setup()                override;
    const char* get_cam_image_path()   override { return "res://assets/images/student.png"; }
    const char* get_jumpscare_path()   override { return "res://assets/images/student_jumpscare.png"; }
    const char* get_name_str()         override { return "Student"; }
    int         get_active_night()     override { return 2; }
    static void _bind_methods() {}
};

// ---------------------------------------------------------------------------
// Librarian — Night 2.  Shows static overlay on every move.
// ---------------------------------------------------------------------------
class Librarian : public Animatronic {
    GDCLASS(Librarian, Animatronic);
public:
    void _ready() override; // builds the static overlay then calls base
protected:
    void        setup()                override;
    const char* get_cam_image_path()   override { return "res://assets/images/librarian.png"; }
    const char* get_jumpscare_path()   override { return "res://assets/images/librarian_jumpscare.png"; }
    const char* get_name_str()         override { return "Librarian"; }
    int         get_active_night()     override { return 2; }
    void        on_move()              override; // triggers static
    static void _bind_methods() {}
private:
    float camera_blind_duration = 5.0f;
};

// ---------------------------------------------------------------------------
// Janitor — Night 3.  Drains power on every move.
// ---------------------------------------------------------------------------
class Janitor : public Animatronic {
    GDCLASS(Janitor, Animatronic);
protected:
    void        setup()                override;
    const char* get_cam_image_path()   override { return "res://assets/images/janitor.png"; }
    const char* get_jumpscare_path()   override { return "res://assets/images/janitor_jumpscare.png"; }
    const char* get_name_str()         override { return "Janitor"; }
    int         get_active_night()     override { return 3; }
    void        on_move()              override; // emits power_drained
    static void _bind_methods() {}
private:
    float power_drain_amount = 7.0f;
};

// ---------------------------------------------------------------------------
// Oble — Night 4.  Statue: completely frozen while player watches this cam.
// ---------------------------------------------------------------------------
class Oble : public Animatronic {
    GDCLASS(Oble, Animatronic);
protected:
    void        setup()                override;
    const char* get_cam_image_path()   override { return "res://assets/images/oble.png"; }
    const char* get_jumpscare_path()   override { return "res://assets/images/oble_jumpscare.png"; }
    const char* get_name_str()         override { return "Oble"; }
    int         get_active_night()     override { return 4; }
    bool        can_move()             override; // freezes when watched
    static void _bind_methods() {}
};

// ---------------------------------------------------------------------------
// RyanAnimatronic — Night 5.
//   • Fast movement, halved again when NOT on camera.
//   • Stops (can_move = false) when player is watching.
//   • Destroys doors — closed doors don't save you.
//   • Retreats when hallway light is turned on.
// ---------------------------------------------------------------------------
class RyanAnimatronic : public Animatronic {
    GDCLASS(RyanAnimatronic, Animatronic);
protected:
    void        setup()                      override;
    const char* get_cam_image_path()         override { return "res://assets/images/ryan.png"; }
    const char* get_jumpscare_path()         override { return "res://assets/images/ryan_jumpscare.png"; }
    const char* get_name_str()               override { return "Ryan"; }
    int         get_active_night()           override { return 5; }
    bool        can_move()                   override; // stops when spotted
    bool        doors_are_useless()          override { return true; }
    bool        is_repelled_by_light(bool l) override;
    static void _bind_methods() {}
};

// ===========================================================================
// AnimatronicManager
//
// Place this node in your scene.  It will:
//   1. Spawn all six animatronics as children.
//   2. Build the shared game-over / you-win screens.
//   3. Show a temporary "Start Night 1" button.  Pressing it activates all
//      Night-1 animatronics and hides the button.
//   4. When the night ends it shows youwin or gameover and stops — no
//      automatic scene change.
// ===========================================================================

class AnimatronicManager : public Node {
    GDCLASS(AnimatronicManager, Node);

public:
    void _ready()  override;
    void _process(double delta) override;

    // External events
    void notify_light_on(bool left_side);
    void notify_power_out();
    void play_troll(); // broadcasts to all active animatronics

    bool is_any_game_over() const;
    bool is_game_won()      const;
    int  get_active_count() const;
    float get_game_timer()  const; // for PowerUI clock display

protected:
    static void _bind_methods();

private:
    static constexpr int NUM_ANIMATRONICS = 6;

    std::vector<Animatronic*> animatronics;

    // Shared end screens
    CanvasLayer* gameover_canvas = nullptr;
    TextureRect* gameover_image  = nullptr;
    CanvasLayer* youwin_canvas   = nullptr;
    TextureRect* youwin_image    = nullptr;

    // Temporary start button
    CanvasLayer* button_canvas = nullptr;
    Button*      start_button  = nullptr;
    bool         night_started = false;

    void build_end_screens();
    void build_start_button();
    void on_start_pressed();   // connected to start_button.pressed
};

#endif // ANIMATRONIC_H