#pragma once
// Optional CPU captures of real ImGui draw data. No SDL, GPU or OS window.
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace cycode::reader::test {
inline void preloadCaptureGlyphs() {
    // Legacy no-renderer tests cannot request dynamic glyph uploads. Bake the
    // same icon and French punctuation glyphs the desktop backend loads on demand.
    static const ImWchar ranges[]={0x20,0xff,0x2010,0x2026,0xe700,0xf8ff,0};
    for (auto &source : ImGui::GetIO().Fonts->Sources) source.GlyphRanges=ranges;
}
inline void capture(const char *name) {
    const auto directory=std::getenv("CYCODE_READER_TEST_CAPTURE");if (!directory) return;
    auto *data=ImGui::GetDrawData();const int width=int(data->DisplaySize.x),height=int(data->DisplaySize.y);
    std::vector<unsigned char> image(size_t(width)*height*3,24);
    for (const auto *list : data->CmdLists) for (const auto &command : list->CmdBuffer) {
        if (command.UserCallback) continue;
        const auto texture=command.TexRef._TexData ? command.TexRef._TexData : ImGui::GetIO().Fonts->TexData;
        if (!texture || !texture->Pixels) continue;
        for (unsigned index=0;index<command.ElemCount;index+=3) {
            const ImDrawVert *v[3];for (int k=0;k<3;++k) v[k]=&list->VtxBuffer[command.VtxOffset+list->IdxBuffer[command.IdxOffset+index+k]];
            const auto cross=[](ImVec2 a,ImVec2 b,ImVec2 p) { return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x); };
            const float area=cross(v[0]->pos,v[1]->pos,v[2]->pos);if (std::abs(area)<.00001f) continue;
            const int left=std::max({0,int(std::floor(std::min({v[0]->pos.x,v[1]->pos.x,v[2]->pos.x}))),int(std::ceil(command.ClipRect.x))});
            const int top=std::max({0,int(std::floor(std::min({v[0]->pos.y,v[1]->pos.y,v[2]->pos.y}))),int(std::ceil(command.ClipRect.y))});
            const int right=std::min({width,int(std::ceil(std::max({v[0]->pos.x,v[1]->pos.x,v[2]->pos.x}))),int(std::ceil(command.ClipRect.z))});
            const int bottom=std::min({height,int(std::ceil(std::max({v[0]->pos.y,v[1]->pos.y,v[2]->pos.y}))),int(std::ceil(command.ClipRect.w))});
            for (int y=top;y<bottom;++y) for (int x=left;x<right;++x) {
                const ImVec2 point{x+.5f,y+.5f};const float a=cross(v[1]->pos,v[2]->pos,point)/area,b=cross(v[2]->pos,v[0]->pos,point)/area,c=1-a-b;
                if (a<0 || b<0 || c<0) continue;
                const float u=a*v[0]->uv.x+b*v[1]->uv.x+c*v[2]->uv.x,vv=a*v[0]->uv.y+b*v[1]->uv.y+c*v[2]->uv.y;
                const int tx=std::clamp(int(u*texture->Width),0,texture->Width-1),ty=std::clamp(int(vv*texture->Height),0,texture->Height-1);
                const auto texel=texture->Pixels+(size_t(ty)*texture->Width+tx)*texture->BytesPerPixel;
                float color[4];for (int channel=0;channel<4;++channel) { const auto shift=channel*8;color[channel]=a*((v[0]->col>>shift)&255)+b*((v[1]->col>>shift)&255)+c*((v[2]->col>>shift)&255); }
                const float alpha=color[3]/255.f*(texture->BytesPerPixel==1 ? texel[0] : texel[3])/255.f;
                auto *destination=image.data()+(size_t(y)*width+x)*3;
                for (int channel=0;channel<3;++channel) { const float value=color[channel]*(texture->BytesPerPixel==1 ? 1.f : texel[channel]/255.f);destination[channel]=static_cast<unsigned char>(std::clamp(value*alpha+destination[channel]*(1-alpha),0.f,255.f)); }
            }
        }
    }
    const auto path=std::filesystem::path(directory)/(std::string(name)+".ppm");std::filesystem::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";file.write(reinterpret_cast<const char*>(image.data()),image.size());
}
}
