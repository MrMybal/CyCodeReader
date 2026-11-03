#include "Branding.h"
#include "cycode/Paths.h"
#include <SDL3/SDL_opengl.h>
#include <stdexcept>

namespace cycode::reader {
BrandSurface loadBrandSurface(const std::filesystem::path &installation) {
    BrandSurface source(SDL_LoadPNG(pathUtf8(installation/"icons/CyCodeReader.png").c_str()),SDL_DestroySurface);
    if (!source) throw std::runtime_error("Cannot load CyCodeReader application logo: "+std::string(SDL_GetError()));
    if (source->w<=0 || source->h<=0 || source->w>4096 || source->h>4096) throw std::runtime_error("Invalid application logo dimensions");
    BrandSurface result(SDL_ConvertSurface(source.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    if (!result) throw std::runtime_error("Cannot decode CyCodeReader application logo");
    return result;
}
Branding::Branding(const std::filesystem::path &installation):surface_(loadBrandSurface(installation)) {
    GLint previous=0,alignment=0,rowLength=0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);
    glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,surface_->pitch/4);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,surface_->w,surface_->h,0,GL_RGBA,GL_UNSIGNED_BYTE,surface_->pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previous));
    updateWindowIcons();
}
Branding::~Branding() { if (texture_) glDeleteTextures(1,&texture_); }
void Branding::updateWindowIcons() {
    int count=0;auto native=SDL_GetWindows(&count);std::set<SDL_WindowID> current;
    for (int i=0;i<count;++i) {
        const auto id=SDL_GetWindowID(native[i]);current.insert(id);
        if (!windows_.contains(id)) SDL_SetWindowIcon(native[i],surface_.get());
    }
    SDL_free(native);windows_=std::move(current);
}
}
