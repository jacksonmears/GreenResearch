#include "Ball.h"

void Ball::render(SDL_Renderer* renderer) {
    // Simple filled circle (approximation)
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for(int w = 0; w < radius * 2; w++) {
        for(int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if((dx*dx + dy*dy) <= (radius * radius)) {
                SDL_RenderPoint(renderer, x + dx, y + dy);
            }
        }
    }
}
