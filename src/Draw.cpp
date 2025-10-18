#include <GL/glew.h>
#include "../include/Draw.h"
#include "../include/Particle.h"
#include "../include/Slope.h"
#include "../include/Config.h"


namespace draw {

void drawParticles(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, geometry::Cell>& cellMap){
    glBegin(GL_POINTS);
    for(int i = 0; i < particles.size(); i += 50) {
        particle::Particle& p = particles[i];
        if (cellMap[p.grid_index].end_index-cellMap[p.grid_index].start_index < 5'000) continue;
        glColor3f(p.r, p.g, p.b);
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
}

void drawArrows(std::vector<particle::Particle>& particles,ska::flat_hash_map<size_t, geometry::Cell>& cellMap) {
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (auto& [key, cell] : cellMap) {
        geometry::Slope& plane = cellMap[key].plane;
        if (!plane.valid || plane.len < 1e-6f) continue;



        // color by slope magnitude
        glColor3f(plane.color, 0.0f, 1.0f - plane.color);

        // draw line segment
        glVertex3f(plane.xBar, plane.yBar + config::Y_OFFSET, plane.zBar);
        glVertex3f(plane.endX, plane.endY + config::Y_OFFSET, plane.endZ);

        // optional: small arrowhead (two small lines)
        glVertex3f(plane.endX, plane.endY + config::Y_OFFSET, plane.endZ);
        glVertex3f(
            plane.endX - plane.dx*config::ARROW_SIZE + plane.dz*config::ARROW_SIZE*0.5f, 
            plane.endY + config::Y_OFFSET, 
            plane.endZ - plane.dz*config::ARROW_SIZE - plane.dx*config::ARROW_SIZE*0.5f
        );

        glVertex3f(plane.endX, plane.endY + config::Y_OFFSET, plane.endZ);
        glVertex3f(
            plane.endX - plane.dx*config::ARROW_SIZE - plane.dz*config::ARROW_SIZE*0.5f, 
            plane.endY + config::Y_OFFSET, 
            plane.endZ - plane.dz*config::ARROW_SIZE + plane.dx*config::ARROW_SIZE*0.5f
        );
    }
    glEnd();
}


}
