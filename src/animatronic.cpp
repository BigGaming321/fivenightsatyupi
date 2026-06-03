#include "animatronic.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ===========================================================================
// File-local helpers
// ===========================================================================

static TextureRect* make_fullscreen_rect(const char* path, bool hidden = true) {
    TextureRect* r = memnew(TextureRect);
    r->set_anchors_preset(Control::PRESET_FULL_RECT);
    r->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(path);
    if (!tex.is_null()) r->set_texture(tex);
    else UtilityFunctions::printerr("Animatronic: missing texture: ", path);
    if (hidden) r->hide();
    return r;
}

static AudioStreamPlayer* make_audio(Node* parent, const char* path, const char* label) {
    AudioStreamPlayer* p = memnew(AudioStreamPlayer);
    Ref<AudioStream>   s = ResourceLoader::get_singleton()->load(path);
    if (!s.is_null()) p->set_stream(s);
    else UtilityFunctions::printerr("Animatronic: missing audio (", label, "): ", path);
    parent->call_deferred("add_child", p);
    return p;
}

// ===========================================================================
// Animatronic (base) -- _ready
// ===========================================================================

void Animatronic::_ready() {
    // Scene hierarchy: Animatronic -> NightManager -> scene root
    Node* root = get_parent() ? get_parent()->get_parent() : nullptr;
    if (!root) { UtilityFunctions::printerr("Animatronic: cannot find scene root!"); return; }

    rng.instantiate();
    rng->randomize();

    setup(); // subclass fills all_cams, attack_cams and tuning values

    door_manager   = root->get_node<DoorManager>("DoorManager");
    camera_manager = root->get_node<CameraManager>("CameraManager");
    if (!door_manager)   UtilityFunctions::printerr(get_name_str(), ": DoorManager not found!");
    if (!camera_manager) UtilityFunctions::printerr(get_name_str(), ": CameraManager not found!");

    cam_canvas = memnew(CanvasLayer);
    cam_canvas->set_layer(cam_layer);

    cam_overlay = memnew(TextureRect);
    cam_overlay->set_anchors_preset(Control::PRESET_CENTER);
    cam_overlay->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    cam_overlay->set_custom_minimum_size(Vector2(400, 400));
    {
        Ref<Texture2D> t = ResourceLoader::get_singleton()->load(get_cam_image_path());
        if (!t.is_null()) cam_overlay->set_texture(t);
        else UtilityFunctions::printerr(get_name_str(), ": missing cam image: ", get_cam_image_path());
    }
    cam_overlay->hide();
    cam_canvas->add_child(cam_overlay);
    root->call_deferred("add_child", cam_canvas);

    jumpscare_canvas = memnew(CanvasLayer);
    jumpscare_canvas->set_layer(jumpscare_layer);
    jumpscare_image = make_fullscreen_rect(get_jumpscare_path());
    jumpscare_canvas->add_child(jumpscare_image);
    root->call_deferred("add_child", jumpscare_canvas);

    jumpscare_audio = make_audio(root, get_jumpscare_audio_path(), "jumpscare");
    troll_audio     = make_audio(root, get_troll_audio_path(),     "troll");

    troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);

    move_timer = memnew(Timer);
    move_timer->set_one_shot(true);
    add_child(move_timer);
    move_timer->connect("timeout", callable_mp(this, &Animatronic::move_to_next_cam));

    current_cam = all_cams[rng->randi_range(0, (int)all_cams.size() - 1)];

    UtilityFunctions::print(get_name_str(), ": ready (first active Night ", get_active_night(), ")");
}

// ===========================================================================
// Animatronic (base) -- activate / deactivate / reset
// ===========================================================================

void Animatronic::activate() {
    if (state != AnimatronicState::INACTIVE) return;

    // Schedule a random spawn time during the night instead of spawning instantly
    float spawn_delay = rng->randf_range(spawn_delay_min, spawn_delay_max);
    UtilityFunctions::print(get_name_str(), ": will spawn in ", spawn_delay, "s");

    Timer* spawn_timer = memnew(Timer);
    spawn_timer->set_one_shot(true);
    add_child(spawn_timer);
    spawn_timer->connect("timeout", callable_mp(this, &Animatronic::do_spawn));
    spawn_timer->start(spawn_delay);
}

void Animatronic::do_spawn() {
    if (state != AnimatronicState::INACTIVE) return;
    state = AnimatronicState::IDLE;
    current_cam = all_cams[rng->randi_range(0, (int)all_cams.size() - 1)];
    schedule_next_move();
    UtilityFunctions::print(get_name_str(), ": spawned on CAM ", current_cam);
}

void Animatronic::deactivate() {
    state = AnimatronicState::INACTIVE;
    if (move_timer) move_timer->stop();
    hide_cam_overlay();
    waiting_at_door = false;
    power_out       = false;
}

void Animatronic::reset_for_next_night() {
    deactivate();
    game_over       = false;
    game_won        = false;
    door_timer      = 0.0f;
    jumpscare_timer = 0.0f;
    static_timer    = 0.0f;
    if (static_overlay)  static_overlay->hide();
    if (jumpscare_image) jumpscare_image->hide();
    current_cam = all_cams.empty() ? 0
                                   : all_cams[rng->randi_range(0, (int)all_cams.size() - 1)];
    troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);
    UtilityFunctions::print(get_name_str(), ": reset for next night");
}

// ===========================================================================
// Animatronic (base) -- _process
// ===========================================================================

void Animatronic::_process(double delta) {
    if (game_over || game_won)               return;
    if (state == AnimatronicState::INACTIVE) return;
    if (power_out)                           return;

    const float dt = static_cast<float>(delta);

    refresh_cam_overlay();

    if (static_timer > 0.0f) {
        static_timer -= dt;
        if (static_overlay) {
            if (static_timer > 0.0f && camera_manager && camera_manager->is_open()
                && camera_manager->get_camera_index() == current_cam)
                static_overlay->show();
            else
                static_overlay->hide();
        }
    }

    if (waiting_at_door) {
        door_timer -= dt;
        bool left    = (target_door == 0);
        bool blocked = left ? door_manager->is_left_closed()
                            : door_manager->is_right_closed();

        if (doors_are_useless()) {
            if (door_timer <= 0.0f) {
                UtilityFunctions::print(get_name_str(), ": broke through door -- JUMPSCARE");
                waiting_at_door = false;
                trigger_jumpscare();
            }
        } else {
            if (blocked) {
                waiting_at_door = false;

                // 50/50: move to next cam OR despawn and re-schedule later
                bool despawn = rng->randf() < 0.5f;
                if (despawn) {
                    UtilityFunctions::print(get_name_str(), ": door blocked -- despawning");
                    hide_cam_overlay();
                    state = AnimatronicState::INACTIVE;

                    // Re-schedule a future spawn
                    float respawn_delay = rng->randf_range(spawn_delay_min, spawn_delay_max);
                    UtilityFunctions::print(get_name_str(), ": will respawn in ", respawn_delay, "s");
                    Timer* t = memnew(Timer);
                    t->set_one_shot(true);
                    add_child(t);
                    t->connect("timeout", callable_mp(this, &Animatronic::do_spawn));
                    t->start(respawn_delay);
                } else {
                    UtilityFunctions::print(get_name_str(), ": door blocked -- retreating to next cam");
                    std::vector<int> safe;
                    for (int c : all_cams) {
                        bool is_atk = false;
                        for (int ac : attack_cams) if (c == ac) { is_atk = true; break; }
                        if (!is_atk) safe.push_back(c);
                    }
                    if (!safe.empty())
                        current_cam = safe[rng->randi_range(0, (int)safe.size() - 1)];
                    state = AnimatronicState::IDLE;
                    schedule_next_move();
                }
            } else if (door_timer <= 0.0f) {
                UtilityFunctions::print(get_name_str(), ": door open -- JUMPSCARE");
                waiting_at_door = false;
                trigger_jumpscare();
            }
        }
        return;
    }

    if (state == AnimatronicState::JUMPSCARING) {
        jumpscare_timer -= dt;
        if (jumpscare_timer <= 0.0f) end_jumpscare();
        return;
    }

    troll_timer -= dt;
    if (troll_timer <= 0.0f) {
        if (troll_audio && troll_audio->is_inside_tree() && !troll_audio->is_playing())
            troll_audio->play();
        troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);
    }
}

// ===========================================================================
// Animatronic (base) -- camera overlay
// ===========================================================================

bool Animatronic::is_watched_on_cam() const {
    return camera_manager &&
           camera_manager->is_open() &&
           camera_manager->get_camera_index() == current_cam;
}

void Animatronic::refresh_cam_overlay() {
    if (!cam_overlay) return;
    is_watched_on_cam() ? cam_overlay->show() : cam_overlay->hide();
}

void Animatronic::hide_cam_overlay() {
    if (cam_overlay) cam_overlay->hide();
}

// ===========================================================================
// Animatronic (base) -- movement
// ===========================================================================

void Animatronic::schedule_next_move() {
    float t = rng->randf_range(move_interval_min, move_interval_max);
    move_timer->start(t);
    UtilityFunctions::print(get_name_str(), ": [CAM ", current_cam, "] next move in ", t, "s");
}

void Animatronic::move_to_next_cam() {
    if (power_out || state == AnimatronicState::JUMPSCARING) return;
    if (state == AnimatronicState::INACTIVE)                 return;
    if (!can_move()) { schedule_next_move(); return; }

    on_move();

    std::vector<int> choices;
    for (int c : all_cams)
        if (c != current_cam) choices.push_back(c);
    if (choices.empty()) { schedule_next_move(); return; }

    current_cam = choices[rng->randi_range(0, (int)choices.size() - 1)];
    UtilityFunctions::print(get_name_str(), ": moved to CAM ", current_cam);

    bool is_attack = false;
    for (int ac : attack_cams) if (current_cam == ac) { is_attack = true; break; }

    if (is_attack) { attack_door(); return; }

    state = AnimatronicState::MOVING;
    schedule_next_move();
}

void Animatronic::attack_door() {
    state       = AnimatronicState::AT_DOOR;
    target_door = -1;
    for (int i = 0; i < (int)attack_cams.size(); ++i)
        if (current_cam == attack_cams[i]) { target_door = i; break; }
    if (target_door == -1) target_door = 0;

    const char* side = (target_door == 0) ? "LEFT" : "RIGHT";
    UtilityFunctions::print(get_name_str(), ": AT DOOR -- ", side,
                            " -- player has ", door_reaction_time, "s");
    door_timer      = door_reaction_time;
    waiting_at_door = true;
}

// ===========================================================================
// Animatronic (base) -- external events
// ===========================================================================

void Animatronic::notify_power_out() {
    power_out = true;
    if (move_timer) move_timer->stop();
    hide_cam_overlay();
}

void Animatronic::set_power_out() { notify_power_out(); }

float Animatronic::get_game_timer() const {
    return shared_game_timer ? *shared_game_timer : 0.0f;
}

void Animatronic::play_troll() {
    if (!troll_audio || !troll_audio->is_inside_tree()) return;
    if (!troll_audio->is_playing()) troll_audio->play();
}

void Animatronic::notify_light_on(bool left_side) {
    if (!waiting_at_door)                return;
    if (!is_repelled_by_light(left_side)) return;
    bool matches = (left_side && target_door == 0) || (!left_side && target_door == 1);
    if (!matches) return;

    UtilityFunctions::print(get_name_str(), ": repelled by light -- retreating");
    waiting_at_door = false;
    std::vector<int> safe;
    for (int c : all_cams) {
        bool is_atk = false;
        for (int ac : attack_cams) if (c == ac) { is_atk = true; break; }
        if (!is_atk) safe.push_back(c);
    }
    if (!safe.empty())
        current_cam = safe[rng->randi_range(0, (int)safe.size() - 1)];
    state = AnimatronicState::IDLE;
    schedule_next_move();
}

// ===========================================================================
// Animatronic (base) -- jumpscare / end-state
// ===========================================================================

void Animatronic::trigger_jumpscare() {
    state           = AnimatronicState::JUMPSCARING;
    jumpscare_timer = jumpscare_duration;
    hide_cam_overlay();
    if (jumpscare_image) jumpscare_image->show();
    if (jumpscare_audio && jumpscare_audio->is_inside_tree())
        jumpscare_audio->play();
    UtilityFunctions::print(get_name_str(), ": JUMPSCARE!");
}

void Animatronic::end_jumpscare() {
    if (jumpscare_image) jumpscare_image->hide();
    if (move_timer) move_timer->stop();
    trigger_game_over();
}

void Animatronic::trigger_game_over() {
    game_over = true;
    state     = AnimatronicState::GAME_OVER;
    if (move_timer) move_timer->stop();
    UtilityFunctions::print(get_name_str(), ": GAME OVER");
}

void Animatronic::trigger_you_win() {
    game_won = true;
    state    = AnimatronicState::YOU_WIN;
    if (move_timer) move_timer->stop();
    hide_cam_overlay();
    UtilityFunctions::print(get_name_str(), ": night survived!");
}

// ===========================================================================
// Animatronic (base) -- helpers for subclasses
// ===========================================================================

void Animatronic::emit_power_drained(float amount) {
    emit_signal("power_drained", amount);
}

void Animatronic::set_static_overlay(TextureRect* overlay, float duration) {
    // Register the pointer — never show here. Visibility is managed entirely
    // by _process (camera-open check) and on_move (explicit show).
    if (overlay) static_overlay = overlay;
    if (duration > 0.0f) static_timer = duration;
    // Do NOT call show() here — caller decides when to show.
}

// ===========================================================================
// Animatronic (base) -- _bind_methods
// ===========================================================================

void Animatronic::_bind_methods() {
    ClassDB::bind_method(D_METHOD("activate"),                      &Animatronic::activate);
    ClassDB::bind_method(D_METHOD("deactivate"),                    &Animatronic::deactivate);
    ClassDB::bind_method(D_METHOD("reset_for_next_night"),          &Animatronic::reset_for_next_night);
    ClassDB::bind_method(D_METHOD("notify_power_out"),              &Animatronic::notify_power_out);
    ClassDB::bind_method(D_METHOD("do_spawn"),                      &Animatronic::do_spawn);
    ClassDB::bind_method(D_METHOD("set_power_out"),                 &Animatronic::set_power_out);
    ClassDB::bind_method(D_METHOD("play_troll"),                    &Animatronic::play_troll);
    ClassDB::bind_method(D_METHOD("notify_light_on", "left_side"),  &Animatronic::notify_light_on);
    ClassDB::bind_method(D_METHOD("is_game_over"),                  &Animatronic::is_game_over);
    ClassDB::bind_method(D_METHOD("is_game_won"),                   &Animatronic::is_game_won);
    ClassDB::bind_method(D_METHOD("get_current_cam"),               &Animatronic::get_current_cam);
    ClassDB::bind_method(D_METHOD("get_game_timer"),                &Animatronic::get_game_timer);
    ClassDB::bind_method(D_METHOD("set_gameover_image", "img"),     &Animatronic::set_gameover_image);
    ClassDB::bind_method(D_METHOD("set_youwin_image",   "img"),     &Animatronic::set_youwin_image);

    ADD_SIGNAL(MethodInfo("power_drained",
        PropertyInfo(Variant::FLOAT, "amount")));
}

// ===========================================================================
// Dean -- Night 1
// ===========================================================================

void Dean::setup() {
    all_cams           = {0, 1, 2, 3, 4};
    attack_cams        = {0};
    move_interval_min  = 10.0f;
    move_interval_max  = 18.0f;
    door_reaction_time = 5.0f;
    cam_layer          = 10;
    jumpscare_layer    = 15;
}

// ===========================================================================
// Student -- Night 2
// ===========================================================================

void Student::setup() {
    all_cams           = {1, 2, 3, 4};
    attack_cams        = {4};
    move_interval_min  = 9.0f;
    move_interval_max  = 16.0f;
    door_reaction_time = 4.5f;
    cam_layer          = 10;
    jumpscare_layer    = 16;
}

// ===========================================================================
// Librarian -- Night 2
// ===========================================================================

void Librarian::_ready() {
    Animatronic::_ready();

    Node* root = get_parent() ? get_parent()->get_parent() : nullptr;
    if (!root) return;

    TextureRect* s = memnew(TextureRect);
    s->set_anchors_preset(Control::PRESET_FULL_RECT);
    s->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
    {
        Ref<Texture2D> tex = ResourceLoader::get_singleton()->load("res://assets/images/static.png");
        if (!tex.is_null()) s->set_texture(tex);
        else UtilityFunctions::printerr("Librarian: static.png not found!");
    }
    s->hide();
    my_static_overlay = s;
    set_static_overlay(s, 0.0f); // register pointer only; duration set on first move

    CanvasLayer* sl = memnew(CanvasLayer);
    sl->set_layer(cam_layer + 1); // just above camera overlay
    sl->add_child(s);
    root->call_deferred("add_child", sl);
}

void Librarian::setup() {
    all_cams           = {0, 1, 2, 3, 4};
    attack_cams        = {0, 4};
    move_interval_min  = 12.0f;
    move_interval_max  = 20.0f;
    door_reaction_time = 5.0f;
    cam_layer          = 11;
    jumpscare_layer    = 17;
}

void Librarian::on_move() {
    if (!my_static_overlay) return;

    UtilityFunctions::print("Librarian: moved to CAM ", current_cam);
    UtilityFunctions::print("Librarian: attacked CAM ", current_cam, "!");

    static_timer = camera_blind_duration;

    if (is_watched_on_cam()) {
        my_static_overlay->show();
        UtilityFunctions::print("Librarian: static shown -- player was watching CAM ", current_cam);
    } else {
        UtilityFunctions::print("Librarian: static armed -- player not on CAM ", current_cam, " yet");
    }
}

// ===========================================================================
// Janitor -- Night 3
// ===========================================================================

void Janitor::setup() {
    all_cams           = {0, 1, 2, 3, 4};
    attack_cams        = {0, 4};
    move_interval_min  = 8.0f;
    move_interval_max  = 14.0f;
    door_reaction_time = 5.0f;
    cam_layer          = 10;
    jumpscare_layer    = 18;
}

void Janitor::on_move() {
    emit_power_drained(power_drain_amount);
    UtilityFunctions::print("Janitor: drained ", power_drain_amount, " power");
}

// ===========================================================================
// Oble -- Night 4
// ===========================================================================

void Oble::setup() {
    all_cams           = {0, 1, 2, 3, 4};
    attack_cams        = {0, 4};
    move_interval_min  = 6.0f;
    move_interval_max  = 10.0f;
    door_reaction_time = 3.0f;
    cam_layer          = 10;
    jumpscare_layer    = 19;
}

bool Oble::can_move() {
    return !is_watched_on_cam();
}

// ===========================================================================
// RyanAnimatronic -- Night 5
// ===========================================================================

void RyanAnimatronic::setup() {
    all_cams           = {0, 1, 2, 3, 4};
    attack_cams        = {0, 4};
    move_interval_min  = 3.0f;
    move_interval_max  = 6.0f;
    door_reaction_time = 3.0f;
    cam_layer          = 10;
    jumpscare_layer    = 20;
}

bool RyanAnimatronic::can_move() {
    if (is_watched_on_cam()) return false;
    move_interval_min = 1.5f;
    move_interval_max = 3.0f;
    return true;
}

bool RyanAnimatronic::is_repelled_by_light(bool /*left*/) {
    return true;
}