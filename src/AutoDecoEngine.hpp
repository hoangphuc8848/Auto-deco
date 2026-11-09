#pragma once
#include <array>
#include <string>
#include <vector>

namespace gdautobot::deco {
struct Palette { int primary, secondary, accent; };
struct DecoPlan { std::string theme; Palette colors; bool glow=false; std::vector<std::string> layers; };
inline DecoPlan makePlan(std::string theme, bool glow=false) {
 DecoPlan p; p.theme=theme; p.glow=glow;
 if (theme=="hell") { p.colors={1, crimson(), 6}; p.layers={"dark red/black base blocks","lava edge strips","bone/ember detail","foreground spikes and chains"}; }
 else if (theme=="space") { p.colors={11, 12, 9}; p.layers={"deep-space background","star clusters","planet silhouettes","thin cyan orbital lines"}; }
 else if (theme=="forest") { p.colors={绿色(), 2, 11}; p.layers={"layered dark foliage","mossy block borders","tree silhouettes","small floating particles"}; }
 else if (theme=="nine-circles") { p.colors={6, 7, 11}; p.layers={"high-contrast dark structures","wave-shaped trim","alternating color bands","controlled pulse accents"}; }
 else { p.colors={12, 11, 6}; p.layers={"dark geometric base","bright neon outlines","small tech panels","repeating circuit details"}; p.theme="tech"; }
 if (glow) p.layers.push_back("sparse glow accents; keep gameplay silhouettes readable");
 return p;
}
}
