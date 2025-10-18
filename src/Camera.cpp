#include <GL/glew.h>
#include "../include/Config.h"
#include "../include/Camera.h"

namespace camera {

void setCamera(const float rot_x, const float rot_y, const float cameraDistance) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)config::SCREEN_WIDTH / config::SCREEN_HEIGHT, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0,0,cameraDistance, 0,0,0, 0,1,0);

    glRotatef(rot_x, 1, 0, 0);
    glRotatef(rot_y, 0, 1, 0);
}

}