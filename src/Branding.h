#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include <filesystem>
#include <memory>
#include <set>

namespace cycode::reader {
using BrandSurface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
BrandSurface loadBrandSurface(const std::filesystem::path &installation);

// Owned by the SDL/OpenGL shell; Reader receives only the shared texture ID.
// Release before destroying the GL context. No editor/core service depends on it.
class Branding {
  public:
    explicit Branding(const std::filesystem::path &installation);
    ~Branding();
    Branding(const Branding &)=delete;
    Branding &operator=(const Branding &)=delete;
    ImTextureID texture() const { return static_cast<ImTextureID>(texture_); }
    void updateWindowIcons();
  private:
    BrandSurface surface_{nullptr,SDL_DestroySurface};
    unsigned texture_=0;
    std::set<SDL_WindowID> windows_;
};
}
