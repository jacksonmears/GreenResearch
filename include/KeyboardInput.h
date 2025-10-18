#pragma once
#include <SDL3/SDL.h>

namespace input {

void handleEvents(const SDL_Event& event, bool& running, bool& mouseDown, int& lastMouseX, int& lastMouseY, float& rotX, float& rotY, float& cameraDistance);

}
