#ifndef POWER_UI_H
#define POWER_UI_H

#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/label.hpp>
#include "power_manager.h"

using namespace godot;

class Animatronic;  // forward declare to avoid circular include

class PowerUI : public CanvasLayer {
    GDCLASS(PowerUI, CanvasLayer);

private:
    PowerManager* power_manager      = nullptr;
    Animatronic*  animatronic        = nullptr;  // new

    ColorRect*    bar_bg             = nullptr;
    ColorRect*    bar_fill           = nullptr;
    ColorRect*    dim_overlay        = nullptr;
    Label*        pct_label          = nullptr;
    Label*        timer_label        = nullptr;  // new

    bool blackout_triggered = false;

    void update_bar(float pct);
    void update_timer();        // new
    void trigger_blackout();

public:
    void _ready()  override;
    void _process(double delta) override;

protected:
    static void _bind_methods();
};

#endif