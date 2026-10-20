// Compile-only shim for the documented UI-kit API. NOT the actual PS5 library.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
namespace hui {
enum class Action {back,menu,north,west,confirm,page_prev,page_next,jump_prev,jump_next,touch,l3};
enum class Direction {none,left,right,up,down};
struct InputFrame { bool is_pressed(Action) const;Direction nav=Direction::none;bool connected=false;};
namespace audio { enum class SoundSet {glass};enum class Cue {back,open,select,error,launch,tab,focus,favorite_on};}
namespace gfx {
struct Color {static Color rgb(std::uint32_t,float=1.0f);Color with_alpha(float) const;};
struct Rect {float x,y,w,h;};
constexpr Rect kFullUv{0,0,1,1};
enum class BackdropMode {aurora};
struct BackdropSpec {BackdropMode mode;Color colors[4];float time;};
struct DrawList {
 void rounded_rect(Rect,float,Color);
 void gradient_rect(Rect,float,Color,Color);
 void shadow(Rect,float,float,Color);
 void glow(Rect,float,float,Color);
 void image(std::uint32_t,Rect,Rect,Color,float=0);
 void star(float,float,float,Color);
 void bordered_rect(Rect,float,Color,float,Color);
};
}
namespace ui {
struct FontRef {};
struct Fonts {FontRef regular,semibold,display;};
void text(gfx::DrawList&,const FontRef&,const std::string&,float,float,float,gfx::Color);
enum class Button {cross,circle,l1,r1,l2,r2,left_stick,dpad,square,triangle,options,touchpad};
struct Hint {Button button;const char* label;Button second=Button::cross;};
struct GlyphStyle {static GlyphStyle dark();};
void draw_button(gfx::DrawList&,const Fonts&,const GlyphStyle&,Button,float,float,float);
struct HintLayout {float cy=0,size=0,text_size=0,item_gap=0;};
float measure_hints(const Fonts&,const Hint*,int,const HintLayout&);
void draw_hints(gfx::DrawList&,const Fonts&,const GlyphStyle&,const Hint*,int,float,bool,const HintLayout&);
}
namespace app {
struct Feedback {void play(audio::Cue);};
struct Context {const ui::Fonts &fonts;};
struct Frame {gfx::BackdropSpec backdrop;gfx::DrawList scene,overlay;bool glass=false;};
struct ConceptInfo {
 const char *id,*name,*tagline,*source;audio::SoundSet sounds;gfx::Color accent;
 std::span<const char*const> techniques;
};
class Concept {public:virtual ~Concept()=default;virtual const ConceptInfo& info() const=0;
 virtual void enter(){};virtual void update(const InputFrame&,float,Feedback&)=0;
 virtual void draw(Frame&)const=0;};
}
namespace concepts {std::unique_ptr<app::Concept> make_universal_launcher(app::Context&);}
}
