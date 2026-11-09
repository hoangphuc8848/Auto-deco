#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace gdautobot::builder {
enum class Segment { Run, Jump, Orb, Platform, Transition, WaveCorridor };
struct Piece { Segment type; float length; int intensity; std::string note; };
struct Section { int difficulty=5; float totalLength=900.f; std::vector<Piece> pieces; };
inline Section generate(int difficulty=5, bool includeWave=false, float length=900.f) {
 Section s; s.difficulty=std::clamp(difficulty,1,10); s.totalLength=std::max(300.f,length);
 int d=s.difficulty;
 s.pieces={{Segment::Run,110.f,1,"safe entry; establish sync"},
           {Segment::Jump,120.f,d/2,"single readable obstacle"},
           {Segment::Orb,100.f,d/2,"one optional timing interaction"},
           {Segment::Platform,140.f,d,"short platform sequence"},
           {Segment::Transition,70.f,2,"clear mode-change space"}};
 if (includeWave) s.pieces.push_back({Segment::WaveCorridor,180.f,d,"wide entry, then gradual narrowing; avoid blind turns"});
 s.pieces.push_back({Segment::Jump,110.f,d,"repeat the motif with one variation"});
 s.pieces.push_back({Segment::Run,90.f,1,"recovery and exit"});
 float sum=0.f; for (auto const& p:s.pieces) sum+=p.length;
 float scale=s.totalLength/sum; for (auto& p:s.pieces) p.length*=scale;
 return s;
}
}
