#include "render/graphics.hpp"
#include "platform/runtime_diagnostics.hpp"
#include <vector>
#ifdef __vita__
#include <vitaGL.h>
#include <psp2/gxm.h>
#endif
#include <string>
namespace wormhole {
bool Graphics::init() {
    RuntimeDiagnostics::checkpoint("graphics_init_begin");
#ifdef __vita__
    vglSetCircularPoolSize(3 * 1024);
    const GLboolean vglResult = vglInitExtended(0, 960, 544, 0x1800000, SCE_GXM_MULTISAMPLE_NONE);
    RuntimeDiagnostics::checkpoint("vgl_init_returned","result=" + std::to_string(static_cast<int>(vglResult)));
    if (vglResult != GL_FALSE) { RuntimeDiagnostics::checkpoint("graphics_init_failed","vitaGL rejected 960x544"); return false; }
    RuntimeDiagnostics::checkpoint("vita_render_state_begin");
    glViewport(0, 0, 960, 544);
    RuntimeDiagnostics::checkpoint("vita_viewport_complete");
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.f, 960.f, 544.f, 0.f, -1.f, 1.f);
    RuntimeDiagnostics::checkpoint("vita_projection_complete");
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    RuntimeDiagnostics::checkpoint("vita_modelview_complete");
    vglUseTripleBuffering(GL_FALSE);
    vglWaitVblankStart(GL_TRUE);
#endif
    initialized_ = true; RuntimeDiagnostics::checkpoint("graphics_ready"); return true;
}
void Graphics::shutdown() { initialized_ = false; }
void Graphics::beginFrame(const Color& clear) {
    if (!initialized_) return;
#ifdef __vita__
    glClearColor(clear.r, clear.g, clear.b, clear.a);
    glClear(GL_COLOR_BUFFER_BIT);
#else
    (void)clear;
#endif
}
void Graphics::endFrame() {
    if (!initialized_) return;
#ifdef __vita__
    vglSwapBuffers(GL_FALSE);
#endif
}
void Graphics::fillRect(float x,float y,float w,float h,const Color& c) {
    if (!initialized_) return;
#ifdef __vita__
    RuntimeDiagnostics::checkpoint("fill_rect_color_begin");
    glColor4f(c.r,c.g,c.b,c.a);
    RuntimeDiagnostics::checkpoint("fill_rect_color_complete");
    RuntimeDiagnostics::checkpoint("fill_rect_begin_begin");
    glBegin(GL_TRIANGLE_STRIP);
    RuntimeDiagnostics::checkpoint("fill_rect_begin_complete");
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_1_begin");
    glVertex2f(x,y);
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_1_complete");
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_2_begin");
    glVertex2f(x+w,y);
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_2_complete");
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_3_begin");
    glVertex2f(x,y+h);
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_3_complete");
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_4_begin");
    glVertex2f(x+w,y+h);
    RuntimeDiagnostics::checkpoint("fill_rect_vertex_4_complete");
    glEnd();
    RuntimeDiagnostics::checkpoint("fill_rect_end_complete");
#else
    (void)x;(void)y;(void)w;(void)h;(void)c;
#endif
}
void Graphics::drawLine(float x1,float y1,float x2,float y2,const Color& c) {
    if (!initialized_) return;
#ifdef __vita__
    glColor4f(c.r,c.g,c.b,c.a);
    glBegin(GL_LINES);
    glVertex2f(x1,y1);
    glVertex2f(x2,y2);
    glEnd();
#else
    (void)x1;(void)y1;(void)x2;(void)y2;(void)c;
#endif
}
Texture Graphics::createTexture(const std::vector<std::uint8_t>& rgba,int width,int height) {
    if (!initialized_ || width<=0 || height<=0 || rgba.size()!=static_cast<std::size_t>(width)*height*4) return {};
#ifdef __vita__
    GLuint handle=0; glGenTextures(1,&handle); if (!handle) return {};
    glBindTexture(GL_TEXTURE_2D,handle);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    glBindTexture(GL_TEXTURE_2D,0);
    return Texture(width,height,handle);
#else
    (void)rgba;(void)width;(void)height; return {};
#endif
}
void Graphics::destroyTexture(Texture& texture) {
#ifdef __vita__
    if (texture.handle()) { GLuint h=static_cast<GLuint>(texture.handle()); glDeleteTextures(1,&h); }
#endif
    texture={};
}
void Graphics::drawTexture(const Texture& texture,float x,float y,float w,float h,const Color& c) {
    if (!initialized_ || !texture.valid()) return;
#ifdef __vita__
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture.handle()));
    glColor4f(c.r,c.g,c.b,c.a); glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0,0); glVertex2f(x,y); glTexCoord2f(1,0); glVertex2f(x+w,y);
    glTexCoord2f(0,1); glVertex2f(x,y+h); glTexCoord2f(1,1); glVertex2f(x+w,y+h);
    glEnd(); glBindTexture(GL_TEXTURE_2D,0); glDisable(GL_TEXTURE_2D);
#else
    (void)texture;(void)x;(void)y;(void)w;(void)h;(void)c;
#endif
}
void Graphics::drawTextureRegion(const Texture& texture,float x,float y,float w,float h,float u0,float v0,float u1,float v1,const Color& c) {
    if (!initialized_ || !texture.valid()) return;
#ifdef __vita__
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(texture.handle()));
    glColor4f(c.r,c.g,c.b,c.a); glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(u0,v0); glVertex2f(x,y); glTexCoord2f(u1,v0); glVertex2f(x+w,y);
    glTexCoord2f(u0,v1); glVertex2f(x,y+h); glTexCoord2f(u1,v1); glVertex2f(x+w,y+h);
    glEnd(); glBindTexture(GL_TEXTURE_2D,0); glDisable(GL_TEXTURE_2D);
#else
    (void)texture;(void)x;(void)y;(void)w;(void)h;(void)u0;(void)v0;(void)u1;(void)v1;(void)c;
#endif
}
}