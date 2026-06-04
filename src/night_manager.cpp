#include "night_manager.h"
#include "power_manager.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>

using namespace godot;

// ===========================================================================
// Night configuration table
// ===========================================================================

const NightConfig NightManager::NIGHTS[5] = {
    { 1, 60.0f, 1.00f,
      "Night 1 -- The First Shift",
      "Only Dean is active. Learn his pattern." },
    { 2, 60.0f, 0.85f,
      "Night 2 -- Double Trouble",
      "Student and Librarian join the hunt. Watch both doors." },
    { 3, 60.0f, 0.70f,
      "Night 3 -- Lights Out",
      "The Janitor drains your power. Conserve electricity." },
    { 4, 60.0f, 0.55f,
      "Night 4 -- The Statue",
      "Oble never moves while you watch -- but you can't watch forever." },
    { 5, 60.0f, 0.40f,
      "Night 5 -- Endgame",
      "Ryan ignores doors and retreats from light. This is the final night." },
};

// ===========================================================================
// File-local helper
// ===========================================================================

static TextureRect* make_fullscreen_rect(const char* path, bool hidden = true) {
    TextureRect* r = memnew(TextureRect);
    r->set_anchors_preset(Control::PRESET_FULL_RECT);
    r->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
    if (!tex.is_null()) r->set_texture(tex);
    else UtilityFunctions::printerr("NightManager: missing texture: ", path);
    if (hidden) r->hide();
    return r;
}

// ===========================================================================
// NightManager -- _ready
// ===========================================================================

void NightManager::_ready() {
    build_end_screens();
    build_start_overlay();

    // Spawn all animatronics once. They persist across all 5 nights.
    auto spawn = [&](Animatronic* a) {
        a->set_gameover_image(gameover_image);
        a->set_youwin_image(youwin_image);
        add_child(a);
        animatronics.push_back(a);
    };

    spawn(memnew(Dean));
    spawn(memnew(Student));
    spawn(memnew(Librarian));
    spawn(memnew(Janitor));
    spawn(memnew(Oble));
    spawn(memnew(RyanAnimatronic));

    show_night_overlay(current_night);
    UtilityFunctions::print("NightManager: ready -- showing Night 1 intro");

    // Grab PowerManager for per-night resets (it's a sibling node).
    power_manager = get_parent() ? get_parent()->get_node<PowerManager>("PowerManager") : nullptr;
    if (!power_manager)
        UtilityFunctions::printerr("NightManager: PowerManager not found -- power won't reset between nights");
}

// ===========================================================================
// NightManager -- _process
// ===========================================================================

void NightManager::_process(double delta) {
    if (game_fully_over || !night_started || night_finished) return;

    night_timer += static_cast<float>(delta);

    // Win: player survived the full duration
    if (night_timer >= cfg().game_duration) {
        night_finished = true;
        UtilityFunctions::print("NightManager: Night ", current_night, " cleared!");
        on_night_cleared();
        return;
    }

    // Lose: any animatronic triggered a game over
    if (is_any_game_over()) {
        night_finished  = true;
        game_fully_over = true;
        on_game_over();
    }
}

// ===========================================================================
// NightManager -- night lifecycle
// ===========================================================================

void NightManager::on_start_pressed() {
    if (night_started) return;
    if (click_sfx && click_sfx->is_inside_tree()) click_sfx->play();
    if (back_button) back_button->hide();
    hide_night_overlay();
    start_current_night();
}

void NightManager::start_current_night() {
    night_started  = false;
    night_finished = false;
    night_timer    = 0.0f;

    // Give every animatronic a pointer to the shared clock and this night's duration.
    for (auto* a : animatronics)
        if (a) a->set_shared_timer(&night_timer, cfg().game_duration);

    activate_night_animatronics();
    night_started = true;

    if (bg_music && bg_music->is_inside_tree()) {
        bg_music->play();
    }

    UtilityFunctions::print("NightManager: Night ", current_night,
                            " started -- survive ", cfg().game_duration, "s");
}

void NightManager::activate_night_animatronics() {
    for (auto* a : animatronics) {
        if (!a) continue;
        if (a->get_active_night() <= current_night)
            a->activate();
    }
}

void NightManager::reset_all_animatronics() {
    for (auto* a : animatronics)
        if (a) a->reset_for_next_night();
    night_started  = false;
    night_finished = false;
    night_timer    = 0.0f;
}

void NightManager::on_night_cleared() {
    if (current_night >= 5) {
        on_true_ending();
        return;
    }

    if (bg_music && bg_music->is_playing()) {
        bg_music->stop();
    }

    if (current_night >= 5) {
        on_true_ending();
        return;
    }

    if (youwin_image) youwin_image->call_deferred("show");

    Timer* t = memnew(Timer);
    t->set_one_shot(true);
    add_child(t);
    t->connect("timeout", callable_mp(this, &NightManager::advance_to_next_night));
    t->start(3.0f);    
}

void NightManager::advance_to_next_night() {
    if (youwin_image) youwin_image->hide();

    current_night++;
    UtilityFunctions::print("NightManager: advancing to Night ", current_night);

    reset_all_animatronics();
    if (power_manager) power_manager->reset_power();

    // ADD THIS:
    Node* root = get_parent();
    if (root) {
        CameraManager* cam = root->get_node<CameraManager>("CameraManager");
        if (cam) cam->reset_for_new_night();
        DoorManager* doors = root->get_node<DoorManager>("DoorManager");
        if (doors) {
            doors->force_open_doors();
            UtilityFunctions::print("NightManager: doors reset to open");
        }
    }
    show_night_overlay(current_night);
}
void NightManager::on_game_over() {
    
    if (bg_music && bg_music->is_playing()) {
        bg_music->stop();
    }

    UtilityFunctions::print("NightManager: GAME OVER on Night ", current_night);
    for (auto* a : animatronics) if (a) a->deactivate();
    if (gameover_image) gameover_image->call_deferred("show");
}

void NightManager::on_true_ending() {
    game_fully_over = true;
    UtilityFunctions::print("NightManager: TRUE ENDING -- all 5 nights survived!");
    for (auto* a : animatronics) if (a) a->deactivate();
    if (truend_image) truend_image->call_deferred("show");
    // Show the back button so the player can return to the main menu
    if (back_button) {
        back_button->call_deferred("show");
        // Re-parent it into the truend_canvas so it sits above the ending image
        if (truend_canvas && back_button->get_parent() == overlay_canvas)
            back_button->reparent(truend_canvas);
    }
}

// ===========================================================================
// NightManager -- UI builders
// ===========================================================================

void NightManager::build_end_screens() {
    Node* root = get_parent();
    if (!root) return;

    // Game-over
    gameover_canvas = memnew(CanvasLayer);
    gameover_canvas->set_layer(50);
    gameover_image = make_fullscreen_rect("res://assets/images/gameover.png");
    gameover_canvas->add_child(gameover_image);
    root->call_deferred("add_child", gameover_canvas);

    // Night-clear (YOU WIN)
    youwin_canvas = memnew(CanvasLayer);
    youwin_canvas->set_layer(50);
    youwin_image = make_fullscreen_rect("res://assets/images/youwin.png");
    youwin_canvas->add_child(youwin_image);
    root->call_deferred("add_child", youwin_canvas);

    // Night 5 true ending
    truend_canvas = memnew(CanvasLayer);
    truend_canvas->set_layer(51);
    truend_image = make_fullscreen_rect("res://assets/images/true_ending.png");
    truend_canvas->add_child(truend_image);
    root->call_deferred("add_child", truend_canvas);
}

void NightManager::build_start_overlay() {
    overlay_canvas = memnew(CanvasLayer);
    overlay_canvas->set_layer(60);

    TextureRect* bg = memnew(TextureRect);
    bg->set_anchors_preset(Control::PRESET_FULL_RECT);
    bg->set_modulate(Color(0, 0, 0, 0.75f));
    bg->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);  // ADD
    overlay_canvas->add_child(bg);

    night_label = memnew(Label);
    night_label->set_anchors_preset(Control::PRESET_CENTER_TOP);
    night_label->set_position(Vector2(-300, 150));
    night_label->set_size(Vector2(600, 80));
    night_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    night_label->add_theme_font_size_override("font_size", 48);
    night_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);  // ADD
    overlay_canvas->add_child(night_label);

    subtitle_label = memnew(Label);
    subtitle_label->set_anchors_preset(Control::PRESET_CENTER_TOP);
    subtitle_label->set_position(Vector2(-280, 250));
    subtitle_label->set_size(Vector2(560, 120));
    subtitle_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    subtitle_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
    subtitle_label->add_theme_font_size_override("font_size", 22);
    subtitle_label->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);  // ADD
    overlay_canvas->add_child(subtitle_label);

    // Start button
    start_button = memnew(Button);
    start_button->set_anchors_preset(Control::PRESET_CENTER);
    start_button->set_position(Vector2(-120, 50));
    start_button->set_size(Vector2(240, 60));
    start_button->set_custom_minimum_size(Vector2(240, 60));
    start_button->add_theme_font_size_override("font_size", 28);
    start_button->connect("pressed",
        callable_mp(this, &NightManager::on_start_pressed));
    overlay_canvas->add_child(start_button);

    // Back to Main Menu button (visible on overlay and true ending; hidden when night starts)
    back_button = memnew(Button);
    back_button->set_anchors_preset(Control::PRESET_CENTER);
    back_button->set_position(Vector2(-120, 130));
    back_button->set_size(Vector2(240, 50));
    back_button->set_custom_minimum_size(Vector2(240, 50));
    back_button->add_theme_font_size_override("font_size", 22);
    back_button->set_text("Back to Main Menu");
    back_button->connect("pressed",
        callable_mp(this, &NightManager::go_to_main_menu));
    overlay_canvas->add_child(back_button);

    overlay_canvas->hide();
    add_child(overlay_canvas);

    // Click sound for overlay buttons
    click_sfx = memnew(AudioStreamPlayer);
    Ref<AudioStream> clk = ResourceLoader::get_singleton()->load(
        "res://assets/MainMenu music & suffix/the-sound-designer-electtic-button-on-sound-a-fl-mastyer-edited-and-final-520904.mp3");
    if (!clk.is_null()) click_sfx->set_stream(clk);
    click_sfx->set_bus("SFX");
    add_child(click_sfx);

    // ==========================================
    // UPDATED: Background Music Setup & Looping
    // ==========================================
    bg_music = memnew(AudioStreamPlayer);
    Ref<AudioStream> bg_stream = ResourceLoader::get_singleton()->load(
        "res://assets/Gameplay music & suffix/game-bg-music.mp3");
    
    if (!bg_stream.is_null()) {
        bg_music->set_stream(bg_stream);
        
        // Explicitly cast to AudioStreamMP3 to turn looping on via code
        Ref<AudioStreamMP3> mp3_stream = bg_stream;
        if (mp3_stream.is_valid()) {
            mp3_stream->set_loop(true);
            UtilityFunctions::print("NightManager: Continuous looping forced for game-bg-music.");
        }
    } else {
        UtilityFunctions::printerr("NightManager: game-bg-music sound not found!");
    }
    bg_music->set_bus("Music"); // Assigned to Music Bus
    add_child(bg_music);
}

void NightManager::show_night_overlay(int night) {
    if (!overlay_canvas) return;
    int idx = CLAMP(night - 1, 0, 4);
    if (night_label)    night_label->set_text(NIGHTS[idx].night_title);
    if (subtitle_label) subtitle_label->set_text(NIGHTS[idx].subtitle);
    if (start_button)   start_button->set_text(
        String("Start Night ") + String::num_int64(night));
    overlay_canvas->show();
}

void NightManager::hide_night_overlay() {
    if (overlay_canvas) overlay_canvas->hide();
}

// ===========================================================================
// NightManager -- public API
// ===========================================================================

void NightManager::notify_light_on(bool left_side) {
    for (auto* a : animatronics) if (a) a->notify_light_on(left_side);
}

void NightManager::notify_power_out() {
    // for (auto* a : animatronics) if (a) a->notify_power_out();
}

void NightManager::play_troll() {
    for (auto* a : animatronics) if (a) a->play_troll();
}

bool NightManager::is_any_game_over() const {
    for (auto* a : animatronics) if (a && a->is_game_over()) return true;
    return false;
}

bool NightManager::is_game_won() const {
    return game_fully_over && current_night > 5;
}

int NightManager::get_active_count() const {
    int n = 0;
    for (auto* a : animatronics)
        if (a && !a->is_inactive() && !a->is_game_over()) ++n;
    return n;
}

// ===========================================================================
// NightManager -- go_to_main_menu
// ===========================================================================

void NightManager::go_to_main_menu() {
    if (click_sfx && click_sfx->is_inside_tree()) click_sfx->play();
    if (bg_music && bg_music->is_playing()) {
        bg_music->stop();
    }

    current_night   = 1;
    night_started   = false;
    night_finished  = false;
    game_fully_over = false;
    night_timer     = 0.0f;
    reset_all_animatronics();
    get_tree()->change_scene_to_file("res://scenes/MM Scenes/Main_Menu.tscn");
}

// ===========================================================================
// NightManager -- _bind_methods
// ===========================================================================

void NightManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("notify_light_on", "left_side"), &NightManager::notify_light_on);
    ClassDB::bind_method(D_METHOD("notify_power_out"),              &NightManager::notify_power_out);
    ClassDB::bind_method(D_METHOD("play_troll"),                    &NightManager::play_troll);
    ClassDB::bind_method(D_METHOD("is_any_game_over"),              &NightManager::is_any_game_over);
    ClassDB::bind_method(D_METHOD("is_game_won"),                   &NightManager::is_game_won);
    ClassDB::bind_method(D_METHOD("get_active_count"),              &NightManager::get_active_count);
    ClassDB::bind_method(D_METHOD("get_current_night"),             &NightManager::get_current_night);
    ClassDB::bind_method(D_METHOD("get_night_timer"),               &NightManager::get_night_timer);
    ClassDB::bind_method(D_METHOD("on_start_pressed"),              &NightManager::on_start_pressed);
    ClassDB::bind_method(D_METHOD("go_to_main_menu"),               &NightManager::go_to_main_menu);
}