#include "render/graphics.hpp"

#ifdef __vita__
#include <vitaGL.h>
#include <psp2/gxm.h>
#endif

namespace wormhole {

bool Graphics::init() {
#ifdef __vita__
    vglUseTripleBuffering(GL_FALSE);
    vglWaitVblankStart(GL_TRUE);
    if (vglInitExtended(0, 960, 544, 0x1800000, SCE_GXM_MULTISAMPLE_NONE) == GL_FALSE)
        return false;
#endif
    initialized_ = true;
    return true;
}

void Graphics::shutdown() {
#ifdef __vita__
    // vitaGL does not expose a shutdown routine; resources are released on process exit.
#endif
    initialized_ = false;
}

void Graphics::beginFrame(const Color& clear) {
    if (!initialized_) return;
#ifdef __vita__
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glClear(GL_COLOR_BUFFER_BIT);
#endif
}

void Graphics::endFrame() {
    if (!initialized_) return;
#ifdef __vita__
    vglSwapBuffers(GL_FALSE);
#endif
}

void Graphics::fillRect(float x, float y, float w, float h, const Color& color) {
    if (!initialized_) return;
#ifdef __vita__
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
#else
    (void)x; (void)y; (void)w; (void)h; (void)color;
#endif
}

void Graphics::drawLine(float x1, float y1, float x2, float y2, const Color& color) {
    if (!initialized_) return;
#ifdef __vita__
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
#else
    (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
#endif
}

}
