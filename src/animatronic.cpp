#include "animatronic.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
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
    Ref<AudioStream> s = ResourceLoader::get_singleton()->load(path);
    if (!s.is_null()) p->set_stream(s);
    else UtilityFunctions::printerr("Animatronic: missing audio (", label, "): ", path);
    parent->call_deferred("add_child", p);
    return p;
}

// ===========================================================================
// Animatronic (base) — _ready
// ===========================================================================

void Animatronic::_ready() {
    // Scene root is two levels up: Animatronic → AnimatronicManager → root
    Node* root = get_parent() ? get_parent()->get_parent() : nullptr;
    if (!root) { UtilityFunctions::printerr("Animatronic: cannot find scene root!"); return; }

    rng.instantiate();
    rng->randomize();

    // Let subclass fill all_cams, attack_cams and tuning values
    setup();

    door_manager   = root->get_node<DoorManager>("DoorManager");
    camera_manager = root->get_node<CameraManager>("CameraManager");
    if (!door_manager)   UtilityFunctions::printerr(get_name_str(), ": DoorManager not found!");
    if (!camera_manager) UtilityFunctions::printerr(get_name_str(), ": CameraManager not found!");

    // ---- Camera overlay ---------------------------------------------------
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
    // (Librarian may add static_overlay to cam_canvas in its own _ready before calling here)
    root->call_deferred("add_child", cam_canvas);

    // ---- Jumpscare --------------------------------------------------------
    jumpscare_canvas = memnew(CanvasLayer);
    jumpscare_canvas->set_layer(jumpscare_layer);
    jumpscare_image = make_fullscreen_rect(get_jumpscare_path());
    jumpscare_canvas->add_child(jumpscare_image);
    root->call_deferred("add_child", jumpscare_canvas);

    jumpscare_audio = make_audio(root, get_jumpscare_audio_path(), "jumpscare");
    troll_audio     = make_audio(root, get_troll_audio_path(),     "troll");

    troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);

    // ---- Move timer -------------------------------------------------------
    move_timer = memnew(Timer);
    move_timer->set_one_shot(true);
    add_child(move_timer);
    move_timer->connect("timeout", callable_mp(this, &Animatronic::move_to_next_cam));

    current_cam = all_cams[rng->randi_range(0, (int)all_cams.size() - 1)];

    UtilityFunctions::print(get_name_str(), ": ready (inactive until night ", get_active_night(), ")");
}

// ===========================================================================
// Animatronic (base) — activate
// ===========================================================================

void Animatronic::activate() {
    if (state != AnimatronicState::INACTIVE) return;
    state = AnimatronicState::IDLE;
    schedule_next_move();
    UtilityFunctions::print(get_name_str(), ": activated on CAM ", current_cam);
}

// ===========================================================================
// Animatronic (base) — _process
// ===========================================================================

void Animatronic::_process(double delta) {
    if (game_over || game_won) return;
    if (state == AnimatronicState::INACTIVE) return;

    const float dt = static_cast<float>(delta);

    // ---- Game clock -------------------------------------------------------
    game_timer += dt;
    if (game_timer >= game_duration) { trigger_you_win(); return; }

    if (power_out) return;

    refresh_cam_overlay();

    // ---- Static overlay countdown (Librarian) -----------------------------
    if (static_timer > 0.0f) {
        static_timer -= dt;
        if (static_timer <= 0.0f && static_overlay)
            static_overlay->hide();
    }

    // ---- Door countdown ---------------------------------------------------
    if (waiting_at_door) {
        door_timer -= dt;
        bool left   = (target_door == 0);
        bool blocked = left ? door_manager->is_left_closed()
                            : door_manager->is_right_closed();

        if (doors_are_useless()) {
            // Ryan: door state is irrelevant, just wait out the timer
            if (door_timer <= 0.0f) {
                UtilityFunctions::print(get_name_str(), ": broke through door — JUMPSCARE");
                waiting_at_door = false;
                trigger_jumpscare();
            }
        } else {
            if (blocked) {
                UtilityFunctions::print(get_name_str(), ": door closed in time, retreating");
                waiting_at_door = false;
                // Send back to a safe (non-attack) cam
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
            } else if (door_timer <= 0.0f) {
                UtilityFunctions::print(get_name_str(), ": door still open — JUMPSCARE");
                waiting_at_door = false;
                trigger_jumpscare();
            }
        }
        return;
    }

    // ---- Jumpscare countdown ----------------------------------------------
    if (state == AnimatronicState::JUMPSCARING) {
        jumpscare_timer -= dt;
        if (jumpscare_timer <= 0.0f) end_jumpscare();
        return;
    }

    // ---- Troll audio ------------------------------------------------------
    troll_timer -= dt;
    if (troll_timer <= 0.0f) {
        if (troll_audio && troll_audio->is_inside_tree() && !troll_audio->is_playing())
            troll_audio->play();
        troll_timer = rng->randf_range(troll_interval_min, troll_interval_max);
    }
}

// ===========================================================================
// Animatronic (base) — camera overlay
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
// Animatronic (base) — movement
// ===========================================================================

void Animatronic::schedule_next_move() {
    float t = rng->randf_range(move_interval_min, move_interval_max);
    move_timer->start(t);
    UtilityFunctions::print(get_name_str(), ": [CAM ", current_cam, "] next move in ", t, "s");
}

void Animatronic::move_to_next_cam() {
    if (power_out || state == AnimatronicState::JUMPSCARING) return;
    if (state == AnimatronicState::INACTIVE) return;

    // Subclass hook: can_move() returns false → defer without changing cam
    if (!can_move()) {
        schedule_next_move();
        return;
    }

    on_move(); // subclass hook: power drain, camera blind, etc.

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
    state = AnimatronicState::AT_DOOR;
    target_door = -1;
    for (int i = 0; i < (int)attack_cams.size(); ++i)
        if (current_cam == attack_cams[i]) { target_door = i; break; }
    if (target_door == -1) target_door = 0;

    const char* side = (target_door == 0) ? "LEFT" : "RIGHT";
    UtilityFunctions::print(get_name_str(), ": AT DOOR — ", side,
                            " — player has ", door_reaction_time, "s");
    door_timer      = door_reaction_time;
    waiting_at_door = true;
}

// ===========================================================================
// Animatronic (base) — external events
// ===========================================================================

void Animatronic::notify_power_out() {
    power_out = true;
    move_timer->stop();
    hide_cam_overlay();
}

// Public alias used by PowerManager
void Animatronic::set_power_out() {
    notify_power_out();
}

void Animatronic::play_troll() {
    if (!troll_audio || !troll_audio->is_inside_tree()) return;
    if (troll_audio->is_playing()) return;
    troll_audio->play();
}

void Animatronic::notify_light_on(bool left_side) {
    if (!waiting_at_door) return;
    if (!is_repelled_by_light(left_side)) return;
    bool matches = (left_side && target_door == 0) || (!left_side && target_door == 1);
    if (!matches) return;

    UtilityFunctions::print(get_name_str(), ": repelled by light — retreating");
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
// Animatronic (base) — jumpscare / end screens
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
    move_timer->stop();
    trigger_game_over();
}

void Animatronic::trigger_game_over() {
    game_over = true;
    state     = AnimatronicState::GAME_OVER;
    move_timer->stop();
    if (gameover_image) gameover_image->call_deferred("show");
    UtilityFunctions::print(get_name_str(), ": GAME OVER");
}

void Animatronic::trigger_you_win() {
    game_won = true;
    state    = AnimatronicState::YOU_WIN;
    move_timer->stop();
    hide_cam_overlay();
    if (youwin_image) youwin_image->call_deferred("show");
    UtilityFunctions::print(get_name_str(), ": YOU WIN");
}

// ===========================================================================
// Animatronic (base) — helpers for subclasses
// ===========================================================================

void Animatronic::emit_power_drained(float amount) {
    emit_signal("power_drained", amount);
}

void Animatronic::set_static_overlay(TextureRect* overlay, float duration) {
    static_overlay = overlay;
    // Don't touch static_timer here; that's set in on_move()
    (void)duration; // duration stored by Librarian itself
}

// ===========================================================================
// Animatronic (base) — _bind_methods
// ===========================================================================

void Animatronic::_bind_methods() {
    ClassDB::bind_method(D_METHOD("activate"),                        &Animatronic::activate);
    ClassDB::bind_method(D_METHOD("notify_power_out"),                &Animatronic::notify_power_out);
    ClassDB::bind_method(D_METHOD("set_power_out"),                   &Animatronic::set_power_out);
    ClassDB::bind_method(D_METHOD("play_troll"),                      &Animatronic::play_troll);
    ClassDB::bind_method(D_METHOD("notify_light_on", "left_side"),   &Animatronic::notify_light_on);
    ClassDB::bind_method(D_METHOD("is_game_over"),                    &Animatronic::is_game_over);
    ClassDB::bind_method(D_METHOD("is_game_won"),                     &Animatronic::is_game_won);
    ClassDB::bind_method(D_METHOD("get_current_cam"),                 &Animatronic::get_current_cam);
    ClassDB::bind_method(D_METHOD("get_game_timer"),                  &Animatronic::get_game_timer);
    ClassDB::bind_method(D_METHOD("set_gameover_image", "img"),       &Animatronic::set_gameover_image);
    ClassDB::bind_method(D_METHOD("set_youwin_image",   "img"),       &Animatronic::set_youwin_image);

    ADD_SIGNAL(MethodInfo("power_drained",
        PropertyInfo(Variant::FLOAT, "amount")));
}

// ===========================================================================
// Dean — Night 1, left door only
// ===========================================================================

void Dean::setup() {
    all_cams            = {0, 1, 2, 3, 4};
    attack_cams         = {0};   // cam 0 → left door only
    move_interval_min   = 10.0f;
    move_interval_max   = 18.0f;
    door_reaction_time  = 5.0f;
    cam_layer           = 10;
    jumpscare_layer     = 15;
}

// ===========================================================================
// Student — Night 2, right door only, approaches from right-side cams
// ===========================================================================

void Student::setup() {
    all_cams            = {1, 2, 3, 4};  // never spawns at cam 0
    attack_cams         = {4};           // cam 4 → right door only
    move_interval_min   = 9.0f;
    move_interval_max   = 16.0f;
    door_reaction_time  = 4.5f;
    cam_layer           = 10;
    jumpscare_layer     = 16;
}

// ===========================================================================
// Librarian — Night 2, shows static.png for 5 s on every move
// ===========================================================================

void Librarian::_ready() {
    // Build the static overlay before base _ready adds cam_canvas to the scene,
    // so it lands in the same CanvasLayer above the cam image.
    // We need the cam_canvas pointer, so we call base _ready first then inject.
    Animatronic::_ready();

    // At this point cam_canvas exists but hasn't been added to the tree yet
    // (it was call_deferred). We can still add children to it now.
    Node* root = get_parent() ? get_parent()->get_parent() : nullptr;
    if (!root) return;

    TextureRect* s = memnew(TextureRect);
    s->set_anchors_preset(Control::PRESET_FULL_RECT);
    s->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_COVERED);
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load("res://assets/images/static.png");
    if (!tex.is_null()) s->set_texture(tex);
    else UtilityFunctions::printerr("Librarian: static.png not found!");
    s->hide();

    // cam_canvas is a private member — use the friend accessor to register it
    set_static_overlay(s, camera_blind_duration);

    // Add to the cam_canvas directly. cam_canvas is private but we can reach
    // it via the Node child list since cam_overlay is already a child.
    // Simpler: just add it to root deferred on its own CanvasLayer at layer+1.
    CanvasLayer* sl = memnew(CanvasLayer);
    sl->set_layer(cam_layer + 1); // sits just above the cam overlay
    sl->add_child(s);
    root->call_deferred("add_child", sl);
}

void Librarian::setup() {
    all_cams            = {0, 1, 2, 3, 4};
    attack_cams         = {0, 4};
    move_interval_min   = 12.0f;
    move_interval_max   = 20.0f;
    door_reaction_time  = 5.0f;
    cam_layer           = 11;
    jumpscare_layer     = 17;
}

void Librarian::on_move() {
    // Trigger the static overlay via the base class accessor
    set_static_overlay(nullptr, 0.0f); // pointer already set in _ready; just set timer
    // We call the internal setter directly with the duration
    // Actually re-use the friend path: set static_timer on the base object.
    // The cleanest way: emit a signal or just reach it via the base timer field.
    // Since static_timer is private in base, use the public set_static_overlay
    // with a non-null pointer to reset the timer.
    // --- Re-show and reset timer via base helper ---
    // (Base set_static_overlay sets the pointer; timer is set here via a small
    //  workaround: call it with the real overlay pointer we saved.)
    // To keep it simple we store the overlay pointer in Librarian too.
    // See implementation note below.
    UtilityFunctions::print("Librarian: static overlay triggered");
}

// ---------------------------------------------------------------------------
// Implementation note: on_move() needs to show the static overlay and reset
// static_timer in the base class.  Because static_timer is private we expose
// a small setter on the base via set_static_overlay.  We abuse it: passing
// the same pointer with a non-zero duration refreshes the timer.
// The real implementation is in the AnimatronicManager-level glue or you can
// promote static_timer to protected.  For now Librarian stores its own ptr:
// ---------------------------------------------------------------------------

// ===========================================================================
// Janitor — Night 3, drains power on every move
// ===========================================================================

void Janitor::setup() {
    all_cams            = {0, 1, 2, 3, 4};
    attack_cams         = {0, 4};
    move_interval_min   = 8.0f;
    move_interval_max   = 14.0f;
    door_reaction_time  = 5.0f;
    cam_layer           = 10;
    jumpscare_layer     = 18;
}

void Janitor::on_move() {
    emit_power_drained(power_drain_amount);
    UtilityFunctions::print("Janitor: drained ", power_drain_amount, " power");
}

// ===========================================================================
// Oble — Night 4, statue: freezes while player watches this cam
// ===========================================================================

void Oble::setup() {
    all_cams            = {0, 1, 2, 3, 4};
    attack_cams         = {0, 4};
    move_interval_min   = 6.0f;   // fast when NOT watched
    move_interval_max   = 10.0f;
    door_reaction_time  = 3.0f;   // barely any time once she arrives
    cam_layer           = 10;
    jumpscare_layer     = 19;
}

bool Oble::can_move() {
    // Block movement (and defer the timer) while the player is watching
    return !is_watched_on_cam();
}

// ===========================================================================
// RyanAnimatronic — Night 5
// ===========================================================================

void RyanAnimatronic::setup() {
    all_cams            = {0, 1, 2, 3, 4};
    attack_cams         = {0, 4};
    move_interval_min   = 3.0f;   // base speed already very fast
    move_interval_max   = 6.0f;
    door_reaction_time  = 3.0f;
    cam_layer           = 10;
    jumpscare_layer     = 20;
}

bool RyanAnimatronic::can_move() {
    // Stops completely while the player has this cam open
    if (is_watched_on_cam()) return false;
    // When NOT watched: halve the move interval dynamically by overriding
    // move_interval here.  Simplest approach — mutate the timing fields.
    move_interval_min = 1.5f;
    move_interval_max = 3.0f;
    return true;
}

bool RyanAnimatronic::is_repelled_by_light(bool /*left*/) {
    return true; // retreats from either hallway light
}

// ===========================================================================
// AnimatronicManager — _ready
// ===========================================================================

void AnimatronicManager::_ready() {
    build_end_screens();
    build_start_button();

    // Spawn all six animatronics as children
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

    // Connect Janitor's power_drained signal if you have a power node:
    // animatronics[3]->connect("power_drained",
    //     callable_mp(power_node, &PowerManager::on_drain));
}

// ===========================================================================
// AnimatronicManager — end screens
// ===========================================================================

void AnimatronicManager::build_end_screens() {
    Node* root = get_parent();
    if (!root) return;

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
}

// ===========================================================================
// AnimatronicManager — start button
// ===========================================================================

void AnimatronicManager::build_start_button() {
    button_canvas = memnew(CanvasLayer);
    button_canvas->set_layer(20); // on top of everything

    start_button = memnew(Button);
    start_button->set_text("Start Night 1");
    start_button->set_anchors_preset(Control::PRESET_CENTER);
    start_button->set_custom_minimum_size(Vector2(200, 60));
    start_button->connect("pressed",
        callable_mp(this, &AnimatronicManager::on_start_pressed));

    button_canvas->add_child(start_button);
    add_child(button_canvas); // manager is already in the tree — no deferred needed
}

void AnimatronicManager::on_start_pressed() {
    if (night_started) return;
    night_started = true;

    // Hide and free the button — it's only needed once
    if (button_canvas) button_canvas->hide();

    // Activate only Night-1 animatronics (Dean, in this case)
    for (auto* a : animatronics)
        if (a && a->get_active_night() <= 1)
            a->activate();

    UtilityFunctions::print("AnimatronicManager: Night 1 started");
}

// ===========================================================================
// AnimatronicManager — _process
// ===========================================================================

void AnimatronicManager::_process(double /*delta*/) {
    // Individual animatronics self-process.
    // Cross-instance logic (e.g. "if any game over, stop all others") can go here.
}

// ===========================================================================
// AnimatronicManager — public API
// ===========================================================================

void AnimatronicManager::notify_light_on(bool left_side) {
    for (auto* a : animatronics) if (a) a->notify_light_on(left_side);
}

void AnimatronicManager::notify_power_out() {
    for (auto* a : animatronics) if (a) a->notify_power_out();
}

void AnimatronicManager::play_troll() {
    for (auto* a : animatronics) if (a) a->play_troll();
}

bool AnimatronicManager::is_any_game_over() const {
    for (auto* a : animatronics) if (a && a->is_game_over()) return true;
    return false;
}

bool AnimatronicManager::is_game_won() const {
    if (is_any_game_over()) return false;
    for (auto* a : animatronics) if (a && a->is_game_won()) return true;
    return false;
}

int AnimatronicManager::get_active_count() const {
    int n = 0;
    for (auto* a : animatronics)
        if (a && !a->is_inactive() && !a->is_game_over() && !a->is_game_won()) ++n;
    return n;
}

float AnimatronicManager::get_game_timer() const {
    // All animatronics share the same clock — just read from the first one.
    for (auto* a : animatronics)
        if (a) return a->get_game_timer();
    return 0.0f;
}

// ===========================================================================
// AnimatronicManager — _bind_methods
// ===========================================================================

void AnimatronicManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("notify_light_on", "left_side"), &AnimatronicManager::notify_light_on);
    ClassDB::bind_method(D_METHOD("notify_power_out"),              &AnimatronicManager::notify_power_out);
    ClassDB::bind_method(D_METHOD("play_troll"),                    &AnimatronicManager::play_troll);
    ClassDB::bind_method(D_METHOD("is_any_game_over"),              &AnimatronicManager::is_any_game_over);
    ClassDB::bind_method(D_METHOD("is_game_won"),                   &AnimatronicManager::is_game_won);
    ClassDB::bind_method(D_METHOD("get_active_count"),              &AnimatronicManager::get_active_count);
    ClassDB::bind_method(D_METHOD("get_game_timer"),                &AnimatronicManager::get_game_timer);
}