#pragma once
#include "Common/Win32.h"
#include <d2d1.h>
#include <cmath>
namespace nexus {
enum class Icon {Home,Music,Stats,Focus,Shelf,Audio,Settings,Play,Pause,Previous,Next,Pin,Close,Plus,Minus,Muted,Volume,Battery,Processor,Memory,Download,Upload,Disk,Clock,File,Text,Check,Chevron,ArrowLeft,ArrowRight,Reset,Sun,Spark,Power,Link,Island,Info,Sliders,ArrowUp,ArrowDown,Brightness,Apps,Earbuds,Speaker,Phone,Keyboard,Mouse,Gamepad,Watch,Bluetooth,Bolt,Heart,Gauge,Shield,Camera,Microphone,Location,Clipboard,Search,Lock,Image,Workspace,MicOff,Lyrics,Snip,Eyedropper,Folder,Copy,Archive,Resize,Convert,Trash,External,Moon,Wifi,Plane,Exchange};
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
    case Icon::Exchange:line(4,8.5f,19,8.5f);path({{15.5f,5},{19,8.5f},{15.5f,12}});line(20,15.5f,5,15.5f);path({{8.5f,12},{5,15.5f},{8.5f,19}});break;
    case Icon::Trash:line(4,6.5f,20,6.5f);path({{9,6.5f},{9.6f,4},{14.4f,4},{15,6.5f}});path({{6,6.5f},{7,20},{17,20},{18,6.5f}});line(10,10,10,16.5f);line(14,10,14,16.5f);break;
    case Icon::Workspace:rect(3,3.5f,10.5f,10.5f,2);rect(13.5f,3.5f,21,10.5f,2);rect(3,13.5f,10.5f,20.5f,2);rect(13.5f,13.5f,21,20.5f,2);break;
    case Icon::Bolt:path({{13,2},{5,13},{11,13},{10,22},{19,10},{13,10},{13,2}},true,true);break;
    case Icon::Heart:path({{12,20},{4,12},{3.2f,8},{5,5},{8.5f,4.5f},{12,8},{15.5f,4.5f},{19,5},{20.8f,8},{20,12},{12,20}},true);break;
    case Icon::Gauge:path({{4,17},{3,13},{4.5f,8.5f},{8,5.5f},{12,4.5f},{16,5.5f},{19.5f,8.5f},{21,13},{20,17}});line(12,14,16,9);dot(12,14,1.4f);break;
    case Icon::Apps:rect(4,4,10,10,2);rect(14,4,20,10,2);rect(4,14,10,20,2);rect(14,14,20,20,2);break;
    }
    rt->SetTransform(saved);
}
inline void drawRing(ID2D1RenderTarget* rt,ID2D1Factory* factory,float x,float y,float radius,float thickness,double progress,UINT32 foreground,UINT32 track){
    ComPtr<ID2D1SolidColorBrush> brush;rt->CreateSolidColorBrush(D2D1::ColorF(track),&brush);rt->DrawEllipse(D2D1::Ellipse({x,y},radius,radius),brush.Get(),thickness);progress=std::clamp(progress,0.,1.);if(progress<=0)return;brush->SetColor(D2D1::ColorF(foreground));if(progress>=.9999){rt->DrawEllipse(D2D1::Ellipse({x,y},radius,radius),brush.Get(),thickness);return;}
    double angle=progress*6.28318530718;ComPtr<ID2D1PathGeometry> path;factory->CreatePathGeometry(&path);ComPtr<ID2D1GeometrySink> sink;path->Open(&sink);sink->BeginFigure({x,y-radius},D2D1_FIGURE_BEGIN_HOLLOW);D2D1_ARC_SEGMENT arc{{x+float(std::sin(angle))*radius,y-float(std::cos(angle))*radius},{radius,radius},0,D2D1_SWEEP_DIRECTION_CLOCKWISE,progress>.5?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL};sink->AddArc(arc);sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->Close();ComPtr<ID2D1StrokeStyle> stroke;auto style=D2D1::StrokeStyleProperties();style.startCap=style.endCap=D2D1_CAP_STYLE_ROUND;factory->CreateStrokeStyle(style,nullptr,0,&stroke);rt->DrawGeometry(path.Get(),brush.Get(),thickness,stroke.Get());
}
}
