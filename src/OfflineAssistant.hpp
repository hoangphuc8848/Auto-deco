#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace gdautobot::offline {
enum class Intent { Help, Decorate, BuildGameplay, SuggestPalette, Unknown };
struct Request { Intent intent=Intent::Unknown; std::string theme; int difficulty=5; bool glow=false; bool pulse=false; };

inline std::string lower(std::string s) {
 for (auto& c:s) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
 return s;
}
inline bool has(std::string const& s, std::string const& word) { return s.find(word)!=std::string::npos; }

// Fully local keyword/phrase parser. No network, API key, telemetry, or model download.
inline Request parse(std::string prompt) {
 auto s=lower(std::move(prompt)); Request r;
 if (has(s,"help")||has(s,"giup")||has(s,"lệnh")||has(s,"lenh")) r.intent=Intent::Help;
 else if (has(s,"gameplay")||has(s,"layout")||has(s,"đường")||has(s,"duong")||has(s,"build")) r.intent=Intent::BuildGameplay;
 else if (has(s,"palette")||has(s,"màu")||has(s,"mau")||has(s,"color")) r.intent=Intent::SuggestPalette;
 else if (has(s,"deco")||has(s,"decorate")||has(s,"trang trí")||has(s,"trang tri")) r.intent=Intent::Decorate;
 if (has(s,"nine circles")||has(s,"nc")) r.theme="nine-circles";
 else if (has(s,"hell")||has(s,"địa ngục")||has(s,"dia nguc")) r.theme="hell";
 else if (has(s,"tech")||has(s,"neon")) r.theme="tech";
 else if (has(s,"space")||has(s,"vũ trụ")||has(s,"vu tru")) r.theme="space";
 else if (has(s,"forest")||has(s,"rừng")||has(s,"rung")) r.theme="forest";
 r.glow=has(s,"glow")||has(s,"phát sáng")||has(s,"phat sang");
 r.pulse=has(s,"pulse")||has(s,"nhịp")||has(s,"nhip");
 if (has(s,"easy")) r.difficulty=3;
 if (has(s,"hard")||has(s,"khó")||has(s,"kho")) r.difficulty=8;
 return r;
}
inline std::vector<std::string> help() {
 return {"deco <theme> - create a decoration plan","gameplay <easy|hard> - create a gameplay section plan","palette <theme> - suggest two-color palette","themes: tech, hell, space, forest, nine circles","Offline parser: understands supported keywords; not a general-purpose LLM."};
}
}
