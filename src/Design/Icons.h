#pragma once
#include "Common/Win32.h"
#include <d2d1.h>
#include <cmath>
#include <array>
#include <optional>
namespace nexus {
enum class Icon {Home,Music,Stats,Focus,Shelf,Audio,Settings,Play,Pause,Previous,Next,Pin,Close,Plus,Minus,Muted,Volume,Battery,Processor,Memory,Download,Upload,Disk,Clock,File,Text,Check,Chevron,ArrowLeft,ArrowRight,Reset,Sun,Spark,Power,Link,Island,Info,Sliders,ArrowUp,ArrowDown,Brightness,Apps,Earbuds,Speaker,Phone,Keyboard,Mouse,Gamepad,Watch,Bluetooth,Bolt,Heart,Gauge,Shield,Camera,Microphone,Location,Clipboard,Search,Lock,Image,Workspace,MicOff,Lyrics,Snip,Eyedropper,Folder,Copy,Archive,Resize,Convert,Trash,External,Moon,Wifi,Plane,Exchange,
    // Phase 5F: weather skies.
    Cloud,Rain,Snow,Storm,Fog,PartlyCloudy,
    // Phase 5F: sharing with your own PCs.
    Laptop,Send,
    // Phase 5G: the music library, shuffle, and continuing music on another PC.
    Library,Shuffle,Handoff};
// Original 24-unit optical grid. Rounded stroke ends are consistent at every DPI.
inline void drawIcon(ID2D1RenderTarget* rt,ID2D1Factory* factory,Icon icon,float x,float y,float size,UINT32 color,float opacity=1){
    D2D1_MATRIX_3X2_F saved;rt->GetTransform(&saved);rt->SetTransform(D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y)*saved);
    ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(color,opacity),&brush);ComPtr<ID2D1StrokeStyle> stroke;auto style=D2D1::StrokeStyleProperties();style.startCap=style.endCap=style.dashCap=D2D1_CAP_STYLE_ROUND;style.lineJoin=D2D1_LINE_JOIN_ROUND;factory->CreateStrokeStyle(style,nullptr,0,&stroke);
    auto line=[&](float a,float b,float c,float d){rt->DrawLine({a,b},{c,d},brush.Get(),1.65f,stroke.Get());};
    auto rect=[&](float a,float b,float c,float d,float r=3){rt->DrawRoundedRectangle(D2D1::RoundedRect({a,b,c,d},r,r),brush.Get(),1.65f,stroke.Get());};
    auto circle=[&](float a,float b,float r){rt->DrawEllipse(D2D1::Ellipse({a,b},r,r),brush.Get(),1.65f,stroke.Get());};
    auto dot=[&](float a,float b,float r){rt->FillEllipse(D2D1::Ellipse({a,b},r,r),brush.Get());};
    auto path=[&](std::initializer_list<D2D1_POINT_2F> points,bool close=false,bool fill=false){ComPtr<ID2D1PathGeometry> geometry;factory->CreatePathGeometry(&geometry);ComPtr<ID2D1GeometrySink> sink;geometry->Open(&sink);auto it=points.begin();sink->BeginFigure(*it++,fill?D2D1_FIGURE_BEGIN_FILLED:D2D1_FIGURE_BEGIN_HOLLOW);for(;it!=points.end();++it)sink->AddLine(*it);sink->EndFigure(close?D2D1_FIGURE_END_CLOSED:D2D1_FIGURE_END_OPEN);sink->Close();if(fill)rt->FillGeometry(geometry.Get(),brush.Get());else rt->DrawGeometry(geometry.Get(),brush.Get(),1.65f,stroke.Get());};
    switch(icon){
    case Icon::Home:path({{3,10},{12,3},{21,10}});path({{5,9},{5,20},{10,20},{10,14},{14,14},{14,20},{19,20},{19,9}});break;
    case Icon::Music:path({{9,17},{9,6},{20,3},{20,15}});line(9,10,20,7);rt->FillEllipse(D2D1::Ellipse({6.5f,18},3,2.4f),brush.Get());rt->FillEllipse(D2D1::Ellipse({17.5f,16},3,2.4f),brush.Get());break;
    case Icon::Stats:line(5,18,5,12);line(12,18,12,5);line(19,18,19,9);line(3,22,21,22);break;
    case Icon::Focus:circle(12,13,8);line(10,2,14,2);line(12,5,12,2);line(18,6,20,4);line(12,13,15,10);break;
    case Icon::Shelf:path({{4,5},{20,5},{22,18},{2,18},{4,5}},true);path({{2,14},{8,14},{9,17},{15,17},{16,14},{22,14}});break;
    case Icon::Audio:path({{4,13},{4,9},{6,5},{10,3},{14,3},{18,5},{20,9},{20,13}});rect(3,12,7,20,2);rect(17,12,21,20,2);break;
    case Icon::Settings:circle(12,12,4);for(int i=0;i<8;++i){float a=i*3.14159265f/4;line(12+std::cos(a)*8,12+std::sin(a)*8,12+std::cos(a)*10,12+std::sin(a)*10);}circle(12,12,8);break;
    case Icon::Play:path({{8,4},{20,12},{8,20}},true,true);break;
    case Icon::Pause:rt->FillRoundedRectangle(D2D1::RoundedRect({6,4,10,20},1,1),brush.Get());rt->FillRoundedRectangle(D2D1::RoundedRect({14,4,18,20},1,1),brush.Get());break;
    case Icon::Previous:path({{18,5},{7,12},{18,19}},true,true);line(5,5,5,19);break;
    case Icon::Next:path({{6,5},{17,12},{6,19}},true,true);line(19,5,19,19);break;
    case Icon::Pin:path({{8,3},{16,3},{15,10},{19,14},{5,14},{9,10},{8,3}});line(12,14,12,22);break;
    case Icon::Close:line(6,6,18,18);line(6,18,18,6);break;
    case Icon::Plus:line(5,12,19,12);line(12,5,12,19);break;
    case Icon::Minus:line(5,12,19,12);break;
    case Icon::Volume:case Icon::Muted:path({{3,9},{7,9},{12,5},{12,19},{7,15},{3,15}},true);if(icon==Icon::Muted){line(17,9,22,15);line(17,15,22,9);}else{path({{16,8},{18,10},{18,14},{16,16}});path({{19,5},{22,9},{22,15},{19,19}});}break;
    case Icon::Battery:rect(2,6,20,18,3);line(22,10,22,14);line(6,9,6,15);line(10,9,10,15);line(14,9,14,15);break;
    case Icon::Processor:rect(6,6,18,18,2);rect(9,9,15,15,1);for(float n:{8,12,16}){line(n,3,n,6);line(n,18,n,21);line(3,n,6,n);line(18,n,21,n);}break;
    case Icon::Memory:rect(3,7,21,17,2);for(float n:{7,12,17}){line(n,10,n,14);line(n,17,n,20);}break;
    case Icon::Download:case Icon::Upload:line(12,3,12,16);if(icon==Icon::Download)path({{7,11},{12,16},{17,11}});else path({{7,8},{12,3},{17,8}});path({{4,17},{4,21},{20,21},{20,17}});break;
    case Icon::Disk:rect(3,5,21,19,3);line(3,14,21,14);dot(17,16.5f,1);break;
    case Icon::Clock:circle(12,12,9);path({{12,6},{12,12},{16,14}});break;
    case Icon::File:path({{5,3},{14,3},{19,8},{19,21},{5,21}},true);path({{14,3},{14,8},{19,8}});line(8,12,16,12);line(8,16,14,16);break;
    case Icon::Text:line(4,5,20,5);line(12,5,12,20);line(8,20,16,20);break;
    case Icon::Check:path({{4,12},{9,17},{20,6}});break;
    case Icon::Chevron:case Icon::ArrowRight:path({{9,5},{16,12},{9,19}});break;
    case Icon::ArrowLeft:path({{15,5},{8,12},{15,19}});break;
    case Icon::Reset:path({{5,9},{8,5},{15,4},{20,9},{20,16},{15,20},{8,19}});path({{3,4},{3,10},{9,10}});break;
    case Icon::Sun:circle(12,12,4);for(int i=0;i<8;++i){float a=i*3.14159265f/4;line(12+std::cos(a)*8,12+std::sin(a)*8,12+std::cos(a)*10,12+std::sin(a)*10);}break;
    case Icon::Spark:path({{12,2},{15,9},{22,12},{15,15},{12,22},{9,15},{2,12},{9,9}},true);break;
    case Icon::Power:line(12,2,12,12);path({{6,5},{3,9},{3,15},{7,20},{17,20},{21,15},{21,9},{18,5}});break;
    case Icon::Link:rect(2,8,14,16,4);rect(10,8,22,16,4);break;
    case Icon::Island:rect(3,8,21,16,4);dot(16.5f,12,1.4f);line(7,12,12,12);break;
    case Icon::Info:circle(12,12,9);line(12,11,12,17);dot(12,7.5f,1.1f);break;
    case Icon::Sliders:line(4,7,20,7);line(4,17,20,17);circle(9,7,2.4f);circle(15,17,2.4f);break;
    case Icon::ArrowUp:path({{5,15},{12,8},{19,15}});break;
    case Icon::ArrowDown:path({{5,9},{12,16},{19,9}});break;
    case Icon::Brightness:circle(12,12,3.6f);for(int i=0;i<8;++i){float a=i*3.14159265f/4;line(12+std::cos(a)*6.6f,12+std::sin(a)*6.6f,12+std::cos(a)*9.2f,12+std::sin(a)*9.2f);}break;
    case Icon::Earbuds:path({{7,4},{9.5f,4},{10.5f,6},{10.5f,10},{9,12},{9,19}});circle(7,8,3);path({{17,4},{14.5f,4},{13.5f,6},{13.5f,10},{15,12},{15,19}});circle(17,8,3);break;
    case Icon::Speaker:rect(6,2,18,22,3);circle(12,15,3.5f);dot(12,7,1.3f);break;
    case Icon::Phone:rect(7,2,17,22,3);line(10.5f,18.5f,13.5f,18.5f);break;
    case Icon::Keyboard:rect(2,6,22,18,3);for(float x:{6.f,9.f,12.f,15.f,18.f})dot(x,10,.9f);line(7,14,17,14);break;
    case Icon::Mouse:rect(7,3,17,21,5);line(12,6,12,10);break;
    case Icon::Gamepad:path({{7,7},{17,7},{21,15},{19.5f,18},{16,17},{14,14},{10,14},{8,17},{4.5f,18},{3,15},{7,7}},true);line(7,10,7,12.5f);line(5.8f,11.2f,8.2f,11.2f);dot(16.5f,10.5f,.9f);dot(18,12.5f,.9f);break;
    case Icon::Watch:rect(6,6,18,18,4);path({{8,6},{9,2},{15,2},{16,6}});path({{8,18},{9,22},{15,22},{16,18}});path({{12,9},{12,12},{14,13}});break;
    case Icon::Bluetooth:path({{7,7},{17,17},{12,21},{12,3},{17,7},{7,17}});break;
    case Icon::Shield:path({{12,2.5f},{19.5f,5.5f},{19.5f,11},{18,15.5f},{12,21.5f},{6,15.5f},{4.5f,11},{4.5f,5.5f},{12,2.5f}},true);path({{8.5f,12},{11,14.5f},{15.5f,9.5f}});break;
    case Icon::Camera:rect(2.5f,7,15.5f,17,3);path({{15.5f,10.5f},{21.5f,7.5f},{21.5f,16.5f},{15.5f,13.5f}},true);break;
    case Icon::Microphone:rect(9,2.5f,15,14,3);path({{5.5f,11},{6.5f,15},{9,17.6f},{12,18.5f},{15,17.6f},{17.5f,15},{18.5f,11}});line(12,18.5f,12,21.5f);line(8.5f,21.5f,15.5f,21.5f);break;
    case Icon::Location:path({{12,21.5f},{7,15},{5.5f,11},{6.5f,6.5f},{9,4},{12,3},{15,4},{17.5f,6.5f},{18.5f,11},{17,15},{12,21.5f}},true);circle(12,10.5f,2.6f);break;
    case Icon::Clipboard:rect(5,4,19,21.5f,2.5f);rect(9,2.5f,15,6,1.5f);line(8.5f,11,15.5f,11);line(8.5f,15,13.5f,15);break;
    case Icon::Search:circle(10.5f,10.5f,6.5f);line(15.3f,15.3f,20.5f,20.5f);break;
    case Icon::Lock:rect(5,10.5f,19,21,2.5f);path({{8,10.5f},{8,7.5f},{9.5f,4.5f},{12,3.5f},{14.5f,4.5f},{16,7.5f},{16,10.5f}});dot(12,15.5f,1.2f);break;
    case Icon::Image:rect(3,5,21,19,2.5f);circle(8.5f,9.8f,1.8f);path({{3.5f,17},{9,12},{13,15.5f},{16,13},{20.5f,17}});break;
    case Icon::MicOff:rect(9,2.5f,15,14,3);path({{5.5f,11},{6.5f,15},{9,17.6f},{12,18.5f},{15,17.6f},{17.5f,15},{18.5f,11}});line(12,18.5f,12,21.5f);line(8.5f,21.5f,15.5f,21.5f);line(3.5f,3,20.5f,21);break;
    // Lines of text beside a note: lyrics.
    case Icon::Lyrics:line(3.5f,6,13,6);line(3.5f,11,11,11);line(3.5f,16,9,16);path({{16,18},{16,5},{21,6.5f}});rt->FillEllipse(D2D1::Ellipse({13.8f,18.4f},2.6f,2.1f),brush.Get());break;
    // Phase 5C: capture and Shelf actions.
    case Icon::Snip:path({{3,8.5f},{3,3},{8.5f,3}});path({{15.5f,3},{21,3},{21,8.5f}});path({{21,15.5f},{21,21},{15.5f,21}});path({{8.5f,21},{3,21},{3,15.5f}});dot(12,12,1.6f);break;
    case Icon::Eyedropper:line(4.5f,19.5f,12.5f,11.5f);line(10,9,15,14);line(12.5f,11.5f,17.5f,6.5f);circle(18.6f,5.4f,2.4f);break;
    case Icon::Folder:path({{3,6},{9.5f,6},{11.5f,8.5f},{21,8.5f},{21,19},{3,19}},true);break;
    case Icon::Copy:rect(8.5f,8.5f,20,20,2.5f);path({{15.5f,8.5f},{15.5f,4},{4,4},{4,15.5f},{8.5f,15.5f}});break;
    case Icon::Archive:rect(4,3.5f,20,20.5f,2.5f);line(12,3.5f,12,5.5f);line(12,7.5f,12,9.5f);line(12,11.5f,12,13);rect(10.2f,13,13.8f,17,1);break;
    case Icon::Resize:rect(3.5f,11,12.5f,20,2);path({{13.5f,3.5f},{20.5f,3.5f},{20.5f,10.5f}});line(20.5f,3.5f,14.5f,9.5f);break;
    case Icon::Convert:line(4.5f,8.5f,18,8.5f);path({{14.5f,5},{18,8.5f},{14.5f,12}});line(19.5f,15.5f,6,15.5f);path({{9.5f,12},{6,15.5f},{9.5f,19}});break;
    case Icon::External:path({{13.5f,4},{20,4},{20,10.5f}});line(20,4,11.5f,12.5f);path({{17,14},{17,19},{5,19},{5,7},{10,7}});break;
    // Phase 5D: dark mode and sleep, Wi-Fi, airplane mode, currency.
    case Icon::Moon:{ComPtr<ID2D1PathGeometry> g;factory->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> s;g->Open(&s);s->BeginFigure({15,3.6f},D2D1_FIGURE_BEGIN_HOLLOW);
        s->AddArc(D2D1::ArcSegment({20.4f,15},D2D1::SizeF(8.8f,8.8f),0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_LARGE));s->AddArc(D2D1::ArcSegment({15,3.6f},D2D1::SizeF(7.4f,7.4f),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
        s->EndFigure(D2D1_FIGURE_END_CLOSED);s->Close();rt->DrawGeometry(g.Get(),brush.Get(),1.65f,stroke.Get());break;}
    case Icon::Wifi:{for(float r:{3.6f,7.6f,11.6f}){ComPtr<ID2D1PathGeometry> g;factory->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> s;g->Open(&s);const float k=.7071f;
        s->BeginFigure({12-r*k,19.5f-r*k},D2D1_FIGURE_BEGIN_HOLLOW);s->AddArc(D2D1::ArcSegment({12+r*k,19.5f-r*k},D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));s->EndFigure(D2D1_FIGURE_END_OPEN);s->Close();rt->DrawGeometry(g.Get(),brush.Get(),1.65f,stroke.Get());}
        dot(12,19.5f,1.5f);break;}
    case Icon::Plane:path({{12,2.5f},{13.6f,4.4f},{13.6f,9.4f},{21,13.8f},{21,15.8f},{13.6f,13.6f},{13.6f,18.4f},{16.2f,20.4f},{16.2f,21.6f},{12,20.6f},{7.8f,21.6f},{7.8f,20.4f},{10.4f,18.4f},{10.4f,13.6f},{3,15.8f},{3,13.8f},{10.4f,9.4f},{10.4f,4.4f}},true);break;
    case Icon::Laptop:rect(4.5f,5,19.5f,15.5f,2);line(2,19,22,19);break;
    case Icon::Send:path({{3,11},{21,3},{14,21},{11,13}},true);line(11,13,21,3);break;
    case Icon::Library:line(3,6,15,6);line(3,11,15,11);line(3,16,10.5f,16);path({{18.5f,18},{18.5f,8.5f},{22,7.5f}});rt->FillEllipse(D2D1::Ellipse({16.2f,18.4f},2.5f,2.1f),brush.Get());break;
    case Icon::Shuffle:path({{3,7},{7,7},{15,17},{20.5f,17}});path({{3,17},{7,17},{9.6f,13.6f}});path({{12.4f,10.4f},{15,7},{20.5f,7}});path({{17.8f,4.3f},{20.5f,7},{17.8f,9.7f}});path({{17.8f,14.3f},{20.5f,17},{17.8f,19.7f}});break;
    case Icon::Handoff:rect(2.5f,9.5f,14.5f,17.5f,1.5f);line(1,20.5f,16,20.5f);line(13.5f,11,21,3.5f);path({{16,3.5f},{21,3.5f},{21,8.5f}});break;
    case Icon::Exchange:line(4,8.5f,19,8.5f);path({{15.5f,5},{19,8.5f},{15.5f,12}});line(20,15.5f,5,15.5f);path({{8.5f,12},{5,15.5f},{8.5f,19}});break;
    case Icon::Trash:line(4,6.5f,20,6.5f);path({{9,6.5f},{9.6f,4},{14.4f,4},{15,6.5f}});path({{6,6.5f},{7,20},{17,20},{18,6.5f}});line(10,10,10,16.5f);line(14,10,14,16.5f);break;
    case Icon::Workspace:rect(3,3.5f,10.5f,10.5f,2);rect(13.5f,3.5f,21,10.5f,2);rect(3,13.5f,10.5f,20.5f,2);rect(13.5f,13.5f,21,20.5f,2);break;
    case Icon::Bolt:path({{13,2},{5,13},{11,13},{10,22},{19,10},{13,10},{13,2}},true,true);break;
    case Icon::Heart:path({{12,20},{4,12},{3.2f,8},{5,5},{8.5f,4.5f},{12,8},{15.5f,4.5f},{19,5},{20.8f,8},{20,12},{12,20}},true);break;
    case Icon::Gauge:path({{4,17},{3,13},{4.5f,8.5f},{8,5.5f},{12,4.5f},{16,5.5f},{19.5f,8.5f},{21,13},{20,17}});line(12,14,16,9);dot(12,14,1.4f);break;
    case Icon::Apps:rect(4,4,10,10,2);rect(14,4,20,10,2);rect(4,14,10,20,2);rect(14,14,20,20,2);break;
    // Weather: a cloud outline (raised when something falls from it), the sun peeking behind one, fog in bands.
    case Icon::Cloud:case Icon::Rain:case Icon::Snow:case Icon::Storm:case Icon::PartlyCloudy:{const float lift=icon==Icon::Cloud?0.f:icon==Icon::PartlyCloudy?-.5f:-4.f,sx=icon==Icon::PartlyCloudy?-1.f:0.f;
        if(icon==Icon::PartlyCloudy){circle(16,8,3.2f);for(int i=0;i<5;++i){float a=-3.14159265f*(.95f-i*.24f);line(16+std::cos(a)*5.4f,8+std::sin(a)*5.4f,16+std::cos(a)*7.2f,8+std::sin(a)*7.2f);}}
        path({{6.5f+sx,18.5f+lift},{4+sx,17+lift},{3.5f+sx,14.5f+lift},{5+sx,12.3f+lift},{7.5f+sx,11.6f+lift},{8.6f+sx,8.6f+lift},{11.5f+sx,6.6f+lift},{15+sx,6.8f+lift},{17.6f+sx,9.2f+lift},{18.3f+sx,11.6f+lift},{20.3f+sx,12.6f+lift},{21+sx,15+lift},{20+sx,17.6f+lift},{17.5f+sx,18.5f+lift}},true);
        if(icon==Icon::Rain){line(8,17.5f,6.8f,21);line(12,17.5f,10.8f,21);line(16,17.5f,14.8f,21);}
        if(icon==Icon::Snow){dot(8,19,1.1f);dot(12,20.5f,1.1f);dot(16,19,1.1f);}
        if(icon==Icon::Storm)path({{12.5f,15},{10.2f,18.6f},{13,18.6f},{11.2f,22.2f}});break;}
    case Icon::Fog:line(4,8,20,8);line(3,12,17,12);line(6,16,21,16);line(5,20,14,20);break;
    }
    rt->SetTransform(saved);
}
// ---- Phase 5F: icons in motion ---------------------------------------------------------------
// Draws `icon` with a transform `a` in its own 24-unit grid (rotate or bounce it about its centre).
inline void drawIconTransformed(ID2D1RenderTarget* rt,ID2D1Factory* factory,Icon icon,const D2D1::Matrix3x2F& a,float x,float y,float size,UINT32 color,float opacity=1){
    D2D1::Matrix3x2F saved;rt->GetTransform(&saved);auto place=D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y);auto back=place;if(!back.Invert()){drawIcon(rt,factory,icon,x,y,size,color,opacity);return;}
    rt->SetTransform(back*a*place*saved);drawIcon(rt,factory,icon,x,y,size,color,opacity);rt->SetTransform(saved);
}
inline float easeOutCubic(float t){t=std::clamp(t,0.f,1.f);return 1-(1-t)*(1-t)*(1-t);}
inline float easeOutBack(float t){t=std::clamp(t,0.f,1.f);const float c=1.70158f;return 1+(c+1)*std::pow(t-1,3.f)+c*std::pow(t-1,2.f);}
// One frame (t from 0 to 1) of an icon changing into another. Play and pause share a shape
// (two quads: the bars fold into the triangle); the speaker keeps its body while its waves
// give way to a cross; the microphone's slash draws across; anything else turns and blends.
inline void drawMorph(ID2D1RenderTarget* rt,ID2D1Factory* factory,Icon from,Icon to,float t,float x,float y,float size,UINT32 color){
    if(t<=0){drawIcon(rt,factory,from,x,y,size,color);return;}if(t>=1||from==to){drawIcon(rt,factory,to,x,y,size,color);return;}
    D2D1_MATRIX_3X2_F saved;rt->GetTransform(&saved);ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(color),&brush);
    ComPtr<ID2D1StrokeStyle> stroke;auto style=D2D1::StrokeStyleProperties();style.startCap=style.endCap=D2D1_CAP_STYLE_ROUND;style.lineJoin=D2D1_LINE_JOIN_ROUND;factory->CreateStrokeStyle(style,nullptr,0,&stroke);
    auto grid=[&]{rt->SetTransform(D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y)*saved);};
    auto quads=[](Icon i)->std::optional<std::array<D2D1_POINT_2F,8>>{
        if(i==Icon::Pause)return std::array<D2D1_POINT_2F,8>{{{6,4},{10,4},{10,20},{6,20},{14,4},{18,4},{18,20},{14,20}}};
        if(i==Icon::Play)return std::array<D2D1_POINT_2F,8>{{{8,4},{14,7.5f},{14,16.5f},{8,20},{14,7.5f},{20,12},{20,12},{14,16.5f}}};return std::nullopt;};
    const float e=easeOutCubic(t);
    if(auto a=quads(from),b=quads(to);a&&b){grid();
        for(int q=0;q<2;++q){ComPtr<ID2D1PathGeometry> g;factory->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> s;g->Open(&s);
            for(int k=0;k<4;++k){auto pa=(*a)[size_t(q*4+k)],pb=(*b)[size_t(q*4+k)];D2D1_POINT_2F p{pa.x+(pb.x-pa.x)*e,pa.y+(pb.y-pa.y)*e};if(k==0)s->BeginFigure(p,D2D1_FIGURE_BEGIN_FILLED);else s->AddLine(p);}
            s->EndFigure(D2D1_FIGURE_END_CLOSED);s->Close();rt->FillGeometry(g.Get(),brush.Get());}
        rt->SetTransform(saved);return;}
    auto speaker=[](Icon i){return i==Icon::Volume||i==Icon::Muted;};
    if(speaker(from)&&speaker(to)){const float cross=to==Icon::Muted?e:1-e;grid();
        ComPtr<ID2D1PathGeometry> g;factory->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> s;g->Open(&s);s->BeginFigure({3,9},D2D1_FIGURE_BEGIN_HOLLOW);for(auto p:{D2D1_POINT_2F{7,9},{12,5},{12,19},{7,15},{3,15}})s->AddLine(p);s->EndFigure(D2D1_FIGURE_END_CLOSED);s->Close();rt->DrawGeometry(g.Get(),brush.Get(),1.65f,stroke.Get());
        // The waves shrink back into the speaker as the cross grows out of the same point.
        brush->SetOpacity(1-cross);for(auto [x0,x1,x2,y0]:{std::array<float,4>{16,18,16,8},std::array<float,4>{19,22,19,5}}){const float k=1-cross*.6f;
            ComPtr<ID2D1PathGeometry> w;factory->CreatePathGeometry(&w);ComPtr<ID2D1GeometrySink> ws;w->Open(&ws);ws->BeginFigure({13+(x0-13)*k,12+(y0-12)*k},D2D1_FIGURE_BEGIN_HOLLOW);ws->AddLine({13+(x1-13)*k,12+((y0+2)-12)*k});ws->AddLine({13+(x1-13)*k,12+((24-y0-2)-12)*k});ws->AddLine({13+(x2-13)*k,12+((24-y0)-12)*k});ws->EndFigure(D2D1_FIGURE_END_OPEN);ws->Close();rt->DrawGeometry(w.Get(),brush.Get(),1.65f,stroke.Get());}
        brush->SetOpacity(1);if(cross>0){const float r=3*cross;rt->DrawLine({19.5f-r,12-r},{19.5f+r,12+r},brush.Get(),1.65f,stroke.Get());rt->DrawLine({19.5f-r,12+r},{19.5f+r,12-r},brush.Get(),1.65f,stroke.Get());}
        rt->SetTransform(saved);return;}
    auto mic=[](Icon i){return i==Icon::Microphone||i==Icon::MicOff;};
    if(mic(from)&&mic(to)){drawIcon(rt,factory,Icon::Microphone,x,y,size,color);const float k=to==Icon::MicOff?e:1-e;if(k>0){grid();rt->DrawLine({3.5f,3},{3.5f+17*k,3+18*k},brush.Get(),1.65f,stroke.Get());rt->SetTransform(saved);}return;}
    // Otherwise the old icon turns away and shrinks as the new one turns in.
    drawIconTransformed(rt,factory,from,D2D1::Matrix3x2F::Rotation(40*e,{12,12})*D2D1::Matrix3x2F::Scale(1-.4f*e,1-.4f*e,{12,12}),x,y,size,color,1-e);
    drawIconTransformed(rt,factory,to,D2D1::Matrix3x2F::Rotation(-40*(1-e),{12,12})*D2D1::Matrix3x2F::Scale(.6f+.4f*e,.6f+.4f*e,{12,12}),x,y,size,color,e);
}
// One frame (phase 0 to 1) of an icon's own little animation, played when its page is chosen
// or its switch turns on. Each has a character: the house hops, bars grow, the gear turns,
// the stopwatch hand sweeps, sliders glide, Wi-Fi arcs light outward, the plane takes off.
inline void drawIconAnimated(ID2D1RenderTarget* rt,ID2D1Factory* factory,Icon icon,float phase,float x,float y,float size,UINT32 color){
    const float p=std::clamp(phase,0.f,1.f),hump=std::sin(p*3.14159265f),e=easeOutCubic(p);
    auto T=[](float dx,float dy){return D2D1::Matrix3x2F::Translation(dx,dy);};auto R=[](float a){return D2D1::Matrix3x2F::Rotation(a,{12,12});};auto S=[](float k){return D2D1::Matrix3x2F::Scale(k,k,{12,12});};
    D2D1_MATRIX_3X2_F saved;rt->GetTransform(&saved);ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(color),&brush);
    ComPtr<ID2D1StrokeStyle> stroke;auto style=D2D1::StrokeStyleProperties();style.startCap=style.endCap=D2D1_CAP_STYLE_ROUND;style.lineJoin=D2D1_LINE_JOIN_ROUND;factory->CreateStrokeStyle(style,nullptr,0,&stroke);
    auto grid=[&]{rt->SetTransform(D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y)*saved);};
    auto line=[&](float a,float b,float c,float d){rt->DrawLine({a,b},{c,d},brush.Get(),1.65f,stroke.Get());};
    switch(icon){
    case Icon::Home:drawIconTransformed(rt,factory,icon,T(0,-2.6f*hump)*S(1+.06f*hump),x,y,size,color);break;
    case Icon::Music:drawIconTransformed(rt,factory,icon,R(-12*hump)*T(0,-1.6f*hump),x,y,size,color);break;
    case Icon::Settings:drawIconTransformed(rt,factory,icon,R(90*e),x,y,size,color);break;
    case Icon::Shelf:drawIconTransformed(rt,factory,icon,T(0,2.2f*hump),x,y,size,color);break;
    case Icon::Audio:drawIconTransformed(rt,factory,icon,S(1+.14f*hump),x,y,size,color);break;
    case Icon::Moon:case Icon::Sun:drawIconTransformed(rt,factory,icon,R((icon==Icon::Moon?-30:45)*(1-e))*S(.7f+.3f*easeOutBack(p)),x,y,size,color);break;
    case Icon::Plane:drawIconTransformed(rt,factory,icon,T(3*hump,-3*hump)*R(8*hump),x,y,size,color);break;
    case Icon::Bluetooth:case Icon::Microphone:case Icon::Focus:case Icon::Snip:drawIconTransformed(rt,factory,icon,S(.72f+.28f*easeOutBack(p)),x,y,size,color);break;
    case Icon::Stats:{grid();const float tops[]={12,5,9};for(int i=0;i<3;++i){const float h=easeOutBack(std::clamp(p*1.7f-i*.3f,0.f,1.f));const float cx=5+7.f*i;if(h>0)line(cx,18,cx,18-(18-tops[i])*h);}line(3,22,21,22);rt->SetTransform(saved);break;}
    case Icon::Sliders:{grid();line(4,7,20,7);line(4,17,20,17);const float k=hump*4;rt->DrawEllipse(D2D1::Ellipse({9+k,7},2.4f,2.4f),brush.Get(),1.65f,stroke.Get());rt->DrawEllipse(D2D1::Ellipse({15-k,17},2.4f,2.4f),brush.Get(),1.65f,stroke.Get());rt->SetTransform(saved);break;}
    case Icon::Wifi:{grid();rt->FillEllipse(D2D1::Ellipse({12,19.5f},1.5f,1.5f),brush.Get());int n=0;for(float r:{3.6f,7.6f,11.6f}){const float a=std::clamp(p*3.2f-n*.7f,0.f,1.f);++n;if(a<=0)continue;brush->SetOpacity(a);
        ComPtr<ID2D1PathGeometry> g;factory->CreatePathGeometry(&g);ComPtr<ID2D1GeometrySink> s;g->Open(&s);const float k=.7071f;s->BeginFigure({12-r*k,19.5f-r*k},D2D1_FIGURE_BEGIN_HOLLOW);s->AddArc(D2D1::ArcSegment({12+r*k,19.5f-r*k},D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));s->EndFigure(D2D1_FIGURE_END_OPEN);s->Close();rt->DrawGeometry(g.Get(),brush.Get(),1.65f,stroke.Get());}
        rt->SetTransform(saved);break;}
    default:drawIconTransformed(rt,factory,icon,S(1+.16f*hump),x,y,size,color);break;
    }
}
inline void drawRing(ID2D1RenderTarget* rt,ID2D1Factory* factory,float x,float y,float radius,float thickness,double progress,UINT32 foreground,UINT32 track){
    ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(track),&brush);rt->DrawEllipse(D2D1::Ellipse({x,y},radius,radius),brush.Get(),thickness);progress=std::clamp(progress,0.,1.);if(progress<=0)return;brush->SetColor(D2D1::ColorF(foreground));if(progress>=.9999){rt->DrawEllipse(D2D1::Ellipse({x,y},radius,radius),brush.Get(),thickness);return;}
    double angle=progress*6.28318530718;ComPtr<ID2D1PathGeometry> path;factory->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;path->Open(&sink);sink->BeginFigure({x,y-radius},D2D1_FIGURE_BEGIN_HOLLOW);D2D1_ARC_SEGMENT arc{{x+float(std::sin(angle))*radius,y-float(std::cos(angle))*radius},{radius,radius},0,D2D1_SWEEP_DIRECTION_CLOCKWISE,progress>.5?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL};sink->AddArc(arc);sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();ComPtr<ID2D1StrokeStyle> stroke;auto style=D2D1::StrokeStyleProperties();style.startCap=style.endCap=D2D1_CAP_STYLE_ROUND;factory->CreateStrokeStyle(style,nullptr,0,&stroke);rt->DrawGeometry(path.Get(),brush.Get(),thickness,stroke.Get());
}
}
