#include "../include/KeyboardInput.h"
#include <SDL3/SDL.h>

namespace input {

void handleEvents(const SDL_Event& event, bool& running, bool& mouseDown, int& lastMouseX, int& lastMouseY, float& rotX, float& rotY, float& cameraDistance) {
    switch (event.type) {
        case SDL_EVENT_QUIT:
            running = false;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_ESCAPE)
                running = false;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                mouseDown = true;
                lastMouseX = event.button.x;
                lastMouseY = event.button.y;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT)
                mouseDown = false;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (mouseDown) {
                int dx = event.motion.x - lastMouseX;
                int dy = event.motion.y - lastMouseY;
                rotY += dx * 0.5f;
                rotX += dy * 0.5f;
                lastMouseX = event.motion.x;
                lastMouseY = event.motion.y;
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            cameraDistance -= event.wheel.y * 0.5f;
            if (cameraDistance < 0.1f) cameraDistance = 0.1f;
            break;
    }
}

}
