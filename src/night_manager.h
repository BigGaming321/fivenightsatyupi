#ifndef NIGHT_MANAGER_H
#define NIGHT_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/timer.hpp>
#include "animatronic.h"
#include <vector>

using namespace godot;

class PowerManager;  // forward declare — avoid circular include

// ===========================================================================
// NightConfig -- per-night tuning table
// ===========================================================================

struct NightConfig {
    int         night_number;
    float       game_duration;      // seconds the player must survive
    float       speed_multiplier;   // < 1.0 = faster animatronics
    const char* night_title;        // shown on the start overlay
    const char* subtitle;           // flavour text shown below the title
};

// ===========================================================================
// NightManager
//
// Owns the full 5-night loop:
//   - Spawns all animatronics once; resets them between nights.
//   - Shows a "Night X" intro overlay with a Start button before each night.
//   - Ticks the shared game clock and broadcasts win/lose to animatronics.
//   - Night clear  -> YOU WIN screen for 3 s, then advance to next night.
//   - Night 5 clear -> true ending screen, game stops.
//   - Any jumpscare -> GAME OVER screen, game stops (no auto-retry).
//
// Place this node in your scene. It expects DoorManager and CameraManager
// as siblings under the scene root.
// ===========================================================================

class NightManager : public Node {
    GDCLASS(NightManager, Node);

public:
    void _ready()               override;
    void _process(double delta) override;

    // Called by DoorManager / PowerManager / GameManager
    void notify_light_on(bool left_side);
    void notify_power_out();
    void play_troll();

    // Queries
    bool  is_any_game_over() const;
    bool  is_game_won()      const;
    int   get_active_count() const;
    int   get_current_night() const { return current_night; }
    float get_night_timer()   const { return night_timer; }

protected:
    static void _bind_methods();

private:
    // ---- Night configuration table ----------------------------------------
    static const NightConfig NIGHTS[5];

    // ---- Animatronics ------------------------------------------------------
    std::vector<Animatronic*> animatronics;

    // ---- External managers -------------------------------------------------
    PowerManager* power_manager = nullptr;  // for per-night power reset

    // ---- Night state -------------------------------------------------------
    int   current_night   = 1;
    float night_timer     = 0.0f;  // injected into animatronics as a pointer
    bool  night_started   = false;
    bool  night_finished  = false;
    bool  game_fully_over = false; // set on Night 5 win or any game-over

    // ---- Shared end-screen images (given to each animatronic) --------------
    TextureRect* gameover_image = nullptr;
    TextureRect* youwin_image   = nullptr;

    // ---- Canvas layers for end screens ------------------------------------
    CanvasLayer* gameover_canvas = nullptr;
    CanvasLayer* youwin_canvas   = nullptr;
    CanvasLayer* truend_canvas   = nullptr;
    TextureRect* truend_image    = nullptr;

    // ---- Night-start overlay -----------------------------------------------
    CanvasLayer* overlay_canvas = nullptr;
    Label*       night_label    = nullptr;
    Label*       subtitle_label = nullptr;
    Button*      start_button   = nullptr;

    // ---- Helpers -----------------------------------------------------------
    const NightConfig& cfg() const { return NIGHTS[current_night - 1]; }

    void build_end_screens();
    void build_start_overlay();
    void show_night_overlay(int night);
    void hide_night_overlay();

    void start_current_night();
    void activate_night_animatronics();
    void reset_all_animatronics();

    void on_night_cleared();     // called when night_timer >= duration
    void advance_to_next_night();// connected to a one-shot Timer after win
    void on_game_over();
    void on_true_ending();

    // Connected to start_button.pressed
    void on_start_pressed();
};

#endif // NIGHT_MANAGER_H