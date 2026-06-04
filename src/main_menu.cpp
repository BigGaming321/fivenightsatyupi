#include "main_menu.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/variant/vector2i.hpp>

using namespace godot;

void MainMenu::_bind_methods() {
    ClassDB::bind_method(D_METHOD("on_start_pressed"), &MainMenu::on_start_pressed);
    ClassDB::bind_method(D_METHOD("on_settings_pressed"), &MainMenu::on_settings_pressed);
    ClassDB::bind_method(D_METHOD("on_exit_pressed"), &MainMenu::on_exit_pressed);
    ClassDB::bind_method(D_METHOD("on_back_pressed"), &MainMenu::on_back_pressed);
    ClassDB::bind_method(D_METHOD("on_toggle_sound", "toggled_on"), &MainMenu::on_toggle_sound);

    ClassDB::bind_method(D_METHOD("on_how_to_play_pressed"), &MainMenu::on_how_to_play_pressed);
    ClassDB::bind_method(D_METHOD("on_prev_pressed"), &MainMenu::on_prev_pressed);
    ClassDB::bind_method(D_METHOD("on_next_pressed"), &MainMenu::on_next_pressed);
    ClassDB::bind_method(D_METHOD("on_tutorial_back_pressed"), &MainMenu::on_tutorial_back_pressed);

    ClassDB::bind_method(D_METHOD("on_fullscreen_toggled", "toggled_on"), &MainMenu::on_fullscreen_toggled);
    ClassDB::bind_method(D_METHOD("on_master_volume_changed", "value"), &MainMenu::on_master_volume_changed);
    ClassDB::bind_method(D_METHOD("on_sfx_volume_changed", "value"), &MainMenu::on_sfx_volume_changed);
}

void MainMenu::_ready() {
    ResourceLoader* rl = ResourceLoader::get_singleton();
    guide_pages.push_back(rl->load("res://icon.svg"));
    guide_pages.push_back(rl->load("res://icon.svg"));
    guide_pages.push_back(rl->load("res://icon.svg"));

    get_node<VBoxContainer>("Menu_Buttons")->set_visible(true);
    get_node<Panel>("Settings_Background")->set_visible(false);
    get_node<Panel>("HowtoPlay_Background")->set_visible(false);

    get_node<TextureRect>("HowtoPlay_Background/GuideImage")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/PrevButton")->set_visible(false);

    get_node<Button>("Menu_Buttons/Start")->connect("pressed", callable_mp(this, &MainMenu::on_start_pressed));
    get_node<Button>("Menu_Buttons/HowToPlay")->connect("pressed", callable_mp(this, &MainMenu::on_how_to_play_pressed));
    get_node<Button>("Menu_Buttons/Settings")->connect("pressed", callable_mp(this, &MainMenu::on_settings_pressed));
    get_node<Button>("Menu_Buttons/Exit")->connect("pressed", callable_mp(this, &MainMenu::on_exit_pressed));

    get_node<Button>("Settings_Background/Back")->connect("pressed", callable_mp(this, &MainMenu::on_back_pressed));
    get_node<CheckButton>("Settings_Background/FullScrn_Ctrl")->connect("toggled", callable_mp(this, &MainMenu::on_fullscreen_toggled));
    get_node<HSlider>("Settings_Background/Music_Ctrl")->connect("value_changed", callable_mp(this, &MainMenu::on_master_volume_changed));
    get_node<HSlider>("Settings_Background/SFX_Ctrl2")->connect("value_changed", callable_mp(this, &MainMenu::on_sfx_volume_changed));

    get_node<Button>("HowtoPlay_Background/PrevButton")->connect("pressed", callable_mp(this, &MainMenu::on_prev_pressed));
    get_node<Button>("HowtoPlay_Background/NextButton")->connect("pressed", callable_mp(this, &MainMenu::on_next_pressed));
    get_node<Button>("HowtoPlay_Background/Back(2)")->connect("pressed", callable_mp(this, &MainMenu::on_tutorial_back_pressed));
}

// SOUND SYSTEM

AudioStreamPlayer* MainMenu::get_audio_node(const String &name) {
    Object* obj = get_node_or_null(NodePath(name));
    if (!obj) {
        UtilityFunctions::printerr("Missing audio node: ", name);
        return nullptr;
    }
    return Object::cast_to<AudioStreamPlayer>(obj);
}

void MainMenu::play_click() {
    AudioStreamPlayer* clk = get_audio_node("clk");
    if (clk) {
        clk->play();
    }
}

void MainMenu::play_toggle_sound(bool toggled_on) {
    if (toggled_on) {
        AudioStreamPlayer* snd = get_audio_node("tggl_on");
        if (snd) {
            snd->play();
        }
    } else {
        AudioStreamPlayer* snd = get_audio_node("tggl_off");
        if (snd) {
            snd->play();
        }
    }
}

void MainMenu::on_toggle_sound(bool toggled_on) {
    play_toggle_sound(toggled_on);
}

// NAVIGATION

void MainMenu::on_start_pressed() {
    play_click();
    UtilityFunctions::print("Starting Game Scene...");
    get_tree()->change_scene_to_file("res://scenes/main.tscn");
}

void MainMenu::on_settings_pressed() {
    play_click();
    get_node<VBoxContainer>("Menu_Buttons")->set_visible(false);
    get_node<Panel>("Settings_Background")->set_visible(true);
}

void MainMenu::on_back_pressed() {
    play_click();
    get_node<VBoxContainer>("Menu_Buttons")->set_visible(true);
    get_node<Panel>("Settings_Background")->set_visible(false);
}

void MainMenu::on_exit_pressed() {
    play_click();
    get_tree()->create_timer(0.2)->connect("timeout", Callable(get_tree(), "quit"));
}

// HOW TO PLAY

void MainMenu::update_tutorial_page() {
    TextureRect* img = get_node<TextureRect>("HowtoPlay_Background/GuideImage");
    if (img && guide_pages[current_page].is_valid()) {
        img->set_texture(guide_pages[current_page]);
    }
}

void MainMenu::on_how_to_play_pressed() {
    play_click();
    get_node<VBoxContainer>("Menu_Buttons")->set_visible(false);
    get_node<Panel>("HowtoPlay_Background")->set_visible(true);

    current_page = 0;
    get_node<TextureRect>("HowtoPlay_Background/GuideImage")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/PrevButton")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/NextButton")->set_visible(true);
}

void MainMenu::on_next_pressed() {
    play_click();

    get_node<TextureRect>("HowtoPlay_Background/GuideImage")->set_visible(true);
    get_node<Button>("HowtoPlay_Background/NextButton")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/PrevButton")->set_visible(true);
    update_tutorial_page();
}

void MainMenu::on_prev_pressed() {
    play_click();

    get_node<TextureRect>("HowtoPlay_Background/GuideImage")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/NextButton")->set_visible(true);
    get_node<Button>("HowtoPlay_Background/PrevButton")->set_visible(false);
}

void MainMenu::on_tutorial_back_pressed() {
    play_click();
    get_node<Panel>("HowtoPlay_Background")->set_visible(false);
    get_node<VBoxContainer>("Menu_Buttons")->set_visible(true);

    current_page = 0;
    get_node<TextureRect>("HowtoPlay_Background/GuideImage")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/PrevButton")->set_visible(false);
    get_node<Button>("HowtoPlay_Background/NextButton")->set_visible(true);
}

// SETTINGS


void MainMenu::on_fullscreen_toggled(bool toggled_on) {

    play_toggle_sound(toggled_on);
    DisplayServer* ds = DisplayServer::get_singleton();

    if (toggled_on) {
        ds->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN);
    } else {
        ds->window_set_mode(DisplayServer::WINDOW_MODE_WINDOWED);
    }
}


void MainMenu::on_master_volume_changed(double value) {
    AudioServer* as = AudioServer::get_singleton();
    int bus_id = as->get_bus_index("Music");
    
    // DEBUG PRINT: See what Godot is actually finding
    UtilityFunctions::print("Music Slider moved! Value: ", value, " | Found Bus Index: ", bus_id);

    if (bus_id != -1) {
        if (value <= 0.0) {
            as->set_bus_volume_db(bus_id, -80.0);
        } else {
            as->set_bus_volume_db(bus_id, UtilityFunctions::linear_to_db(value));
        }
    }
}

void MainMenu::on_sfx_volume_changed(double value) {
    AudioServer* as = AudioServer::get_singleton();
    int bus_id = as->get_bus_index("SFX");
    
    // DEBUG PRINT: See what Godot is actually finding
    UtilityFunctions::print("SFX Slider moved! Value: ", value, " | Found Bus Index: ", bus_id);

    if (bus_id != -1) {
        if (value <= 0.0) {
            as->set_bus_volume_db(bus_id, -80.0);
        } else {
            as->set_bus_volume_db(bus_id, UtilityFunctions::linear_to_db(value));
        }
    }
}