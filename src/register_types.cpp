// register_types.cpp
#include "register_types.h"
#include "main_menu.h"
#include "camera_manager.h"
#include "game_manager.h"
#include "power_manager.h"
#include "power_ui.h"
#include "door_manager.h"
#include "night_manager.h"
#include "animatronic.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_game_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;

    // MAIN MENU
    ClassDB::register_class<MainMenu>();

    // GAMEPLAY
    ClassDB::register_class<GameManager>();
    ClassDB::register_class<CameraManager>();
    ClassDB::register_class<PowerManager>();
    ClassDB::register_class<PowerUI>();
    ClassDB::register_class<DoorManager>();

    GDREGISTER_ABSTRACT_CLASS(Animatronic);
    ClassDB::register_class<Dean>();
    ClassDB::register_class<Student>();
    ClassDB::register_class<Librarian>();
    ClassDB::register_class<Janitor>();
    ClassDB::register_class<Oble>();
    ClassDB::register_class<RyanAnimatronic>();
    ClassDB::register_class<NightManager>();
}

void uninitialize_game_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

extern "C" {

GDExtensionBool GDE_EXPORT game_manager_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(
        p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_game_module);
    init_obj.register_terminator(uninitialize_game_module);
    init_obj.set_minimum_library_initialization_level(
        MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}

}