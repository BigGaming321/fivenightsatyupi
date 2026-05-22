#include "animatronic.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ------------------------------------------------------------------ helpers --

static TextureRect* make_fullscreen_rect(const char* path, bool hidden = true) {
    TextureRect* rect = memnew(TextureRect);
    rect->set_anchors_preset(Control::PRESET_FULL_RECT);
    rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);

    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
    if (!tex.is_null())
        rect->set_texture(tex);
    else
        UtilityFunctions::printerr("Animatronic: failed to load ", path);

    if (hidden) rect->hide();
    return rect;
}

// ------------------------------------------------------------------ _ready ---
void Animatronic::_ready() {
    Node* root = get_parent();
    if (!root) return;

    rng.instantiate();
    rng->randomize();

    all_cams    = {0, 1, 2, 3, 4};
    attack_cams = {0, 4};

    door_manager   = root->get_node<DoorManager>("DoorManager");
    camera_manager = root->get_node<CameraManager>("CameraManager");

    if (!door_manager)   UtilityFunctions::printerr("Animatronic: DoorManager not found!");
    if (!camera_manager) UtilityFunctions::printerr("Animatronic: CameraManager not found!");

    // --- Cam overlay ---------------------------------------------------------------------------
    cam_canvas = memnew(CanvasLayer);
    cam_canvas->set_layer(10);

    cam_overlay = memnew(TextureRect);
    cam_overlay->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    cam_overlay->set_anchors_preset(Control::PRESET_CENTER);
    cam_overlay->set_custom_minimum_size(Vector2(400, 400)); // adjust to taste

    Ref<Texture2D> ryan_tex = ResourceLoader::get_singleton()->load(
        "res://assets/images/ryan.png");
    if (!ryan_tex.is_null())
        cam_overlay->set_texture(ryan_tex);
    else
        UtilityFunctions::printerr("Animatronic: ryan.png not found!");

    cam_overlay->hide();
    cam_canvas->add_child(cam_overlay);
    root->call_deferred("add_child", cam_canvas);
    // --- End screens ---
    gameover_canvas = memnew(CanvasLayer);
    gameover_canvas->set_layer(12);
    gameover_image = make_fullscreen_rect("res://assets/images/gameover.png");
    gameover_canvas->add_child(gameover_image);
    root->call_deferred("add_child", gameover_canvas);

    youwin_canvas = memnew(CanvasLayer);
    youwin_canvas->set_layer(12);
    youwin_image = make_fullscreen_rect("res://assets/images/youwin.png");
    youwin_canvas->add_child(youwin_image);
    root->call_deferred("add_child", youwin_canvas);

    // --- Jumpscare ---
    jumpscare_canvas = memnew(CanvasLayer);
    jumpscare_canvas->set_layer(15);
    jumpscare_image = make_fullscreen_rect("res://assets/images/ryan_jumpscare.png");
    jumpscare_canvas->add_child(jumpscare_image);

    jumpscare_audio = memnew(AudioStreamPlayer);
    Ref<AudioStream> js_audio = ResourceLoader::get_singleton()->load(
        "res://assets/music/jumpscare.mp3");
    if (!js_audio.is_null())
        jumpscare_audio->set_stream(js_audio);
    else
        UtilityFunctions::printerr("Animatronic: jumpscare.mp3 not found!");

    // --- Troll audio ---
    troll_audio = memnew(AudioStreamPlayer);
    Ref<AudioStream> troll_sfx = ResourceLoader::get_singleton()->load(
        "res://assets/music/i_feel_good.mp3");
    if (!troll_sfx.is_null())
        troll_audio->set_stream(troll_sfx);
    else
        UtilityFunctions::printerr("Animatronic: i_feel_good.mp3 not found!");

    root->call_deferred("add_child", troll_audio);
    root->call_deferred("add_child", jumpscare_canvas);
    root->call_deferred("add_child", jumpscare_audio);

    troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);

    // --- Move timer ---
    move_timer = memnew(Timer);
    move_timer->set_one_shot(true);
    add_child(move_timer);
    move_timer->connect("timeout", callable_mp(this, &Animatronic::move_to_next_cam));

    current_cam = all_cams[rng->randi_range(0, all_cams.size() - 1)];
    schedule_next_move();

    UtilityFunctions::print("Animatronic: ready on CAM ", current_cam, " — 60s survival starts now");
}

// --------------------------------------------------------------- _process ---

void Animatronic::_process(double delta) {
    if (game_over || game_won) return;

    game_timer += static_cast<float>(delta);
    if (game_timer >= game_duration) {
        trigger_you_win();
        return;
    }

    if (power_out) return;

    refresh_cam_overlay();

    if (waiting_at_door) {
        door_timer -= static_cast<float>(delta);

        bool blocked = (target_door == 0)
            ? door_manager->is_left_closed()
            : door_manager->is_right_closed();

        if (blocked) {
            UtilityFunctions::print("Animatronic: door closed in time, retreating");
            waiting_at_door = false;
            std::vector<int> safe_cams;
            for (int c : all_cams)
                if (c != attack_cams[0] && c != attack_cams[1]) safe_cams.push_back(c);
            current_cam = safe_cams[rng->randi_range(0, safe_cams.size() - 1)];
            state = AnimatronicState::IDLE;
            schedule_next_move();
        } else if (door_timer <= 0.0f) {
            UtilityFunctions::print("Animatronic: door still open — JUMPSCARE");
            waiting_at_door = false;
            trigger_jumpscare();
        }
        return;
    }

    if (state == AnimatronicState::JUMPSCARING) {
        jumpscare_timer -= static_cast<float>(delta);
        if (jumpscare_timer <= 0.0f)
            end_jumpscare();
    }

    troll_timer -= static_cast<float>(delta);
    if (troll_timer <= 0.0f) {
        play_troll();
        troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);
        UtilityFunctions::print("Animatronic: troll sound triggered");
    }
}

// --------------------------------------------------------- camera overlay ---

void Animatronic::refresh_cam_overlay() {
    if (!cam_overlay) return;

    bool should_show = camera_manager &&
                       camera_manager->is_open() &&
                       (camera_manager->get_camera_index() == current_cam);

    if (should_show)
        cam_overlay->show();
    else
        cam_overlay->hide();
}

void Animatronic::hide_cam_overlay() {
    if (cam_overlay) cam_overlay->hide();
}

// ------------------------------------------------------- movement helpers ---

void Animatronic::schedule_next_move() {
    float interval = rng->randf_range(move_interval_min, move_interval_max);
    move_timer->start(interval);
    UtilityFunctions::print("Animatronic: [CAM ", current_cam, "] waiting ", interval, "s");
}

void Animatronic::move_to_next_cam() {
    if (power_out || state == AnimatronicState::JUMPSCARING) return;

    std::vector<int> choices;
    for (int c : all_cams)
        if (c != current_cam) choices.push_back(c);

    current_cam = choices[rng->randi_range(0, choices.size() - 1)];
    UtilityFunctions::print("Animatronic: moved to CAM ", current_cam);

    bool is_attack = (current_cam == attack_cams[0] || current_cam == attack_cams[1]);
    if (is_attack) {
        attack_door();
        return;
    }

    state = AnimatronicState::MOVING;
    schedule_next_move();
}

void Animatronic::attack_door() {
    state       = AnimatronicState::AT_DOOR;
    target_door = (current_cam == attack_cams[0]) ? 0 : 1;

    const char* side = (target_door == 0) ? "LEFT" : "RIGHT";
    UtilityFunctions::print("Animatronic: [AT DOOR] CAM ", current_cam,
                            " — ", side, " door — player has 5s");

    door_timer      = 5.0f;
    waiting_at_door = true;
}

// ------------------------------------------------------------ jumpscare ----

void Animatronic::trigger_jumpscare() {
    state           = AnimatronicState::JUMPSCARING;
    jumpscare_timer = jumpscare_duration;

    hide_cam_overlay();

    if (jumpscare_image) jumpscare_image->show();
    if (jumpscare_audio && jumpscare_audio->is_inside_tree())
        jumpscare_audio->play();

    UtilityFunctions::print("Animatronic: JUMPSCARE! timer=", jumpscare_timer);
}

void Animatronic::end_jumpscare() {
    if (jumpscare_image) jumpscare_image->hide();
    move_timer->stop();
    trigger_game_over();
}

// --------------------------------------------------------- end screens ----

void Animatronic::trigger_game_over() {
    game_over = true;
    state     = AnimatronicState::GAME_OVER;
    move_timer->stop();

    if (gameover_image) gameover_image->call_deferred("show");

    UtilityFunctions::print("Animatronic: GAME OVER");
}

void Animatronic::trigger_you_win() {
    game_won = true;
    state    = AnimatronicState::YOU_WIN;
    move_timer->stop();
    hide_cam_overlay();

    if (youwin_image) youwin_image->call_deferred("show");

    UtilityFunctions::print("Animatronic: YOU WIN");
}

void Animatronic::play_troll() {
    if (!troll_audio || !troll_audio->is_inside_tree()) return;
    if (troll_audio->is_playing()) return;
    troll_audio->play();
}

// --------------------------------------------------------- _bind_methods ---

void Animatronic::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_current_cam"),  &Animatronic::get_current_cam);
    ClassDB::bind_method(D_METHOD("set_power_out"),    &Animatronic::set_power_out);
    ClassDB::bind_method(D_METHOD("move_to_next_cam"), &Animatronic::move_to_next_cam);
    ClassDB::bind_method(D_METHOD("get_game_timer"),   &Animatronic::get_game_timer);
    ClassDB::bind_method(D_METHOD("is_game_over"),     &Animatronic::is_game_over);
    ClassDB::bind_method(D_METHOD("is_game_won"),      &Animatronic::is_game_won);
}