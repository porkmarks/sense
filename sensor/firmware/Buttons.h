#pragma once

enum class Button
{
    BUTTON1 = 2 //the pin!!!
};

void init_buttons();
bool is_pressed(Button b);
void wait_for_press(Button b);
void wait_for_release(Button b);

