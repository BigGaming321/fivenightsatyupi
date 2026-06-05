#include "night_manager.h"
#include "power_manager.h"
#include "camera_manager.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/audio_stream_mp3.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>

using namespace godot;

// Night configuration table

const NightConfig NightManager::NIGHTS[5] = {
    { 1, 60.0f, 1.00f, "", "" },   // shown via assets/night/night1.png
    { 2, 60.0f, 0.85f, "", "" },   // shown via assets/night/night2.png
    { 3, 60.0f, 0.70f, "", "" },   // shown via assets/night/night3.png
    { 4, 60.0f, 0.55f, "", "" },   // shown via assets/night/night4.png
    { 5, 60.0f, 0.40f, "", "" },   // shown via assets/night/night5.png
};

// File-local helper

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

// NightManager -- _ready

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

    power_manager = get_parent() ? get_parent()->get_node<PowerManager>("PowerManager") : nullptr;
    if (!power_manager)
        UtilityFunctions::printerr("NightManager: PowerManager not found -- power won't reset between nights");
}

// NightManager -- _process

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

// NightManager -- night lifecycle

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

void NightManager::fade_to_black(float duration, const Callable& on_done) {
    if (!fade_rect) { on_done.call(); return; }
    fade_rect->set_color(Color(0, 0, 0, 0));
    Ref<Tween> tw = get_tree()->create_tween();
    tw->tween_property(fade_rect, "color", Color(0, 0, 0, 1), duration);
    tw->tween_callback(on_done);
}

void NightManager::fade_from_black(float duration) {
    if (!fade_rect) return;
    fade_rect->set_color(Color(0, 0, 0, 1));
    Ref<Tween> tw = get_tree()->create_tween();
    tw->tween_property(fade_rect, "color", Color(0, 0, 0, 0), duration);
}

// Night lifecycle

void NightManager::on_night_cleared() {
    if (current_night >= 5) {
        on_true_ending();
        return;
    }

    if (bg_music && bg_music->is_playing())
        bg_music->stop();

    fade_to_black(0.5f, callable_mp(this, &NightManager::on_fade_to_youwin_done));
}

void NightManager::on_fade_to_youwin_done() {
    if (youwin_image) youwin_image->show();
    fade_from_black(0.5f);

    Timer* t = memnew(Timer);
    t->set_one_shot(true);
    add_child(t);
    t->connect("timeout", callable_mp(this, &NightManager::on_youwin_hold_done));
    t->start(3.0f);
}

void NightManager::on_youwin_hold_done() {
    fade_to_black(0.5f, callable_mp(this, &NightManager::advance_to_next_night));
}

void NightManager::advance_to_next_night() {
    if (youwin_image) youwin_image->hide();

    current_night++;
    UtilityFunctions::print("NightManager: advancing to Night ", current_night);

    reset_all_animatronics();
    if (power_manager) power_manager->reset_power();

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
    fade_from_black(0.5f);
}
void NightManager::on_game_over() {

    if (bg_music && bg_music->is_playing()) {
        bg_music->stop();
    }

    UtilityFunctions::print("NightManager: GAME OVER on Night ", current_night);

    for (auto* a : animatronics)
        if (a) a->deactivate();

    if (gameover_image) {
        gameover_image->call_deferred("show");
    }

    // wait 10 seconds then go back to menu
    Ref<SceneTreeTimer> timer = get_tree()->create_timer(10.0);

    timer->connect(
        "timeout",
        Callable(this, "_return_to_menu")
    );
}
void NightManager::_return_to_menu() {
    get_tree()->change_scene_to_file("res://scenes/MM Scenes/Main_Menu.tscn");
}

void NightManager::on_true_ending() {
    game_fully_over = true;
    UtilityFunctions::print("NightManager: TRUE ENDING -- all 5 nights survived!");
    for (auto* a : animatronics) if (a) a->deactivate();
    if (truend_image) truend_image->call_deferred("show");
    // Show the back button so the player can return to the main menu
    if (back_button) {
        back_button->call_deferred("show");
        if (truend_canvas && back_button->get_parent() == overlay_canvas)
            back_button->reparent(truend_canvas);
    }
}

// NightManager -- UI builders

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

    // Fade-to-black overlay -- sits above everything (layer 70)
    fade_canvas = memnew(CanvasLayer);
    fade_canvas->set_layer(70);
    fade_rect = memnew(ColorRect);
    fade_rect->set_anchors_preset(Control::PRESET_FULL_RECT);
    fade_rect->set_color(Color(0, 0, 0, 0));   // start fully transparent
    fade_rect->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    fade_canvas->add_child(fade_rect);
    root->call_deferred("add_child", fade_canvas);
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

    for (int i = 0; i < 5; i++) {
        night_images[i] = memnew(TextureRect);
        night_images[i]->set_anchors_preset(Control::PRESET_FULL_RECT);
        night_images[i]->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
        night_images[i]->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
        String path = String("res://assets/night/night") + String::num_int64(i + 1) + ".png";
        Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
        if (!tex.is_null()) night_images[i]->set_texture(tex);
        else UtilityFunctions::printerr("NightManager: missing texture: ", path);
        night_images[i]->hide();
        overlay_canvas->add_child(night_images[i]);
    }

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

   
    bg_music = memnew(AudioStreamPlayer);
    Ref<AudioStream> bg_stream = ResourceLoader::get_singleton()->load(
        "res://assets/Gameplay music & suffix/game-bg-music.mp3");
    
    if (!bg_stream.is_null()) {
        bg_music->set_stream(bg_stream);
        
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

    for (int i = 0; i < 5; i++) {
        night_voice[i] = memnew(AudioStreamPlayer);
        if (i < 4) { // nights 1-4 only
            String path = String("res://assets/voicerecordings/night_")
                          + String::num_int64(i + 1) + ".mp3";
            Ref<AudioStream> vo = ResourceLoader::get_singleton()->load(path);
            if (!vo.is_null())
                night_voice[i]->set_stream(vo);
            else
                UtilityFunctions::printerr("NightManager: missing voice recording: ", path);
        }
        night_voice[i]->set_bus("SFX");
        add_child(night_voice[i]);
    }
}

void NightManager::show_night_overlay(int night) {
    if (!overlay_canvas) return;

    for (int i = 0; i < 5; i++) {
        if (night_images[i])
            night_images[i]->set_visible(i == night - 1);
    }

    if (night_label)    night_label->hide();
    if (subtitle_label) subtitle_label->hide();

    if (start_button)
        start_button->set_text(String("Start Night ") + String::num_int64(night));
    overlay_canvas->show();

    int idx = night - 1;
    if (idx >= 0 && idx < 5 && night_voice[idx] && night_voice[idx]->get_stream().is_valid())
        night_voice[idx]->play();
}

void NightManager::hide_night_overlay() {
    if (overlay_canvas) overlay_canvas->hide();
}


// NightManager -- public API

void NightManager::notify_light_on(bool left_side) {
    for (auto* a : animatronics) if (a) a->notify_light_on(left_side);
}

void NightManager::notify_power_out() {
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


// NightManager -- go_to_main_menu


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


// NightManager -- _bind_methods

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
    ClassDB::bind_method(D_METHOD("on_fade_to_youwin_done"),        &NightManager::on_fade_to_youwin_done);
    ClassDB::bind_method(D_METHOD("on_youwin_hold_done"),           &NightManager::on_youwin_hold_done);
    ClassDB::bind_method(D_METHOD("advance_to_next_night"),         &NightManager::advance_to_next_night);
    ClassDB::bind_method(D_METHOD("go_to_main_menu"),               &NightManager::go_to_main_menu);
    ClassDB::bind_method(D_METHOD("_return_to_menu"), &NightManager::_return_to_menu);
}
