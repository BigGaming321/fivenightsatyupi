#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <vector>

using namespace godot;

class MainMenu : public Control {
    GDCLASS(MainMenu, Control)

private:
    int current_page = 0;
    std::vector<Ref<Texture2D>> guide_pages;

    void update_tutorial_page();
    AudioStreamPlayer* get_audio_node(const String& name);
    void play_toggle_sound(bool toggled_on);

protected:
    static void _bind_methods();

public:
    void _ready() override;

    void play_click();
    void on_toggle_sound(bool toggled_on);

    void on_start_pressed();
    void on_settings_pressed();
    void on_exit_pressed();
    void on_back_pressed();

    void _on_fade_finished(StringName anim_name);
    void on_how_to_play_pressed();
    void on_prev_pressed();
    void on_next_pressed();
    void on_tutorial_back_pressed();

    void on_fullscreen_toggled(bool toggled_on);
    void on_master_volume_changed(double value);
    void on_sfx_volume_changed(double value);
};

#endif