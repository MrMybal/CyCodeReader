#include "Document.h"
#include "Encoding.h"
#include "cycode/Paths.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace cycode;
using namespace cycode::reader;
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
template<class F> static void refuses(F function) { bool failed=false; try { function(); } catch (...) { failed=true; } require(failed,"Expected refusal"); }
int main() {
    auto root=std::filesystem::temp_directory_path()/utf8Path("cycode-reader-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    struct Cleanup { std::filesystem::path root; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(root,ec); } } cleanup{root};
    try {
        const auto file=root/utf8Path("code é.cpp");
        const std::string original="\xef\xbb\xbf\tint value = 1;\r\n// fin\n";
        writeTextFileAtomic(file,original,std::nullopt);
        auto doc=Document::open(file);
        require(!doc.dirty() && doc.text()=="\tint value = 1;\n// fin\n","Decode BOM/newline/tab");
        doc.save(); require(readTextFileBytes(file)==original,"No-op save changed original bytes");
        auto copy=root/"copy.cpp";
        doc.saveAs(copy,std::nullopt); require(readTextFileBytes(copy)==original,"No-op Save As lost mixed endings");
        doc.replace("\tint value = 2;\n"); doc.save();
        require(readTextFileBytes(copy)=="\xef\xbb\xbf\tint value = 2;\r\n","Save encoding/newlines");
        doc.replace("edited\n"); writeTextFileAtomic(copy,"external\n",readTextFileBytes(copy));
        refuses([&] { doc.save(); }); require(doc.dirty() && doc.text()=="edited\n","Conflict lost buffer");
        refuses([&] { doc.saveAs(copy,std::string("external\n")); });
        require(readTextFileBytes(copy)=="external\n","Save As bypassed same-file conflict");
        Document untitled; untitled.replace("new\n");
        refuses([&] { untitled.saveAs(copy,std::nullopt); });
        refuses([&] { untitled.saveAs(copy,std::string("outdated")); });
        require(untitled.untitled() && untitled.dirty(),"Failed Save As changed identity");
        untitled.saveAs(copy,std::string("external\n")); require(!untitled.dirty(),"Save As remains dirty");
        require(readTextFileBytes(copy)=="new\n","Explicit overwrite failed");
        const auto cr=root/"mac.txt"; writeTextFileAtomic(cr,"a\rb\r",std::nullopt);
        auto oldMac=Document::open(cr); oldMac.replace("c\nd\n"); oldMac.save();
        require(readTextFileBytes(cr)=="c\rd\r","CR newlines lost");
        const auto bad=root/"binary.bin"; writeTextFileAtomic(bad,std::string("a\0b",3),std::nullopt);
        refuses([&] { Document::open(bad); });
        refuses([&] { untitled.replace(std::string("\xc0\xaf",2)); });
        refuses([&] { untitled.replace(std::string(maxTextFileBytes+1,'x')); });
        std::filesystem::remove(copy); refuses([&] { untitled.save(); });
        require(untitled.text()=="new\n","Missing disk lost buffer");
        for (const auto &mode : {"utf-8","utf-8-bom","utf-16le","utf-16be"}) {
            const auto encoded=encodeText("été 🛠\r\n",mode);require(decodeText(encoded).text=="été 🛠\r\n","Encoding round trip");
            const auto encodedFile=root/(std::string(mode)+".txt");writeTextFileAtomic(encodedFile,encoded,std::nullopt);auto converted=Document::open(encodedFile);
            converted.save();require(readTextFileBytes(encodedFile)==encoded,"No-op encoding save changed disk");converted.format("utf-16be","LF");require(converted.dirty(),"Encoding-only change is not dirty");converted.save();require(decodeText(readTextFileBytes(encodedFile)).text=="été 🛠\n","Pending encoding/newline conversion not saved");
        }
        require(decodeText(std::string("\x80\xe9",2),"windows-1252").text=="€é","Windows-1252 decode");
        require(encodeText("€é","windows-1252")==std::string("\x80\xe9",2),"Windows-1252 encode");
        refuses([&] { encodeText("🛠","windows-1252"); });
        refuses([&] { decodeText(std::string("\xff\xfe\x00\xd8",4)); });
        refuses([&] { decodeText(std::string("\xfe\xff\xdc\x00",4)); });
        Document formatGuard;formatGuard.replace("🛠");refuses([&] { formatGuard.format("windows-1252","LF"); });require(std::string(formatGuard.encoding())=="UTF-8","Failed encoding conversion mutated mode");
        for (const auto &entry : std::filesystem::directory_iterator(root)) require(entry.path().filename().string().find(".cycode-save-")==std::string::npos,"Temporary file leaked");
        std::cout<<"Reader document saves, conflicts, Save As, Unicode, BOM, newlines and invalid input passed\n";
        return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
