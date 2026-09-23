#pragma once
#include <algorithm>
#include <vector>
#include <cmath>
namespace nexus {
struct PointD{double x,y;};
inline PointD bodyOrigin(double w,double h,double canvasW,double canvasH,int edge){return edge?PointD{canvasW-w,(canvasH-h)/2}:PointD{(canvasW-w)/2,0};}
// Concave shoulders carry an attached island out of the screen edge. Each is
// twice as wide as it is deep and flows from the edge (horizontal tangent) into
// the island's side (vertical tangent). The drawn wings and the input region
// both use these control points, so they always agree.
struct ShoulderShape {double width,depth;};
inline ShoulderShape shoulderShape(double radius){return {2*radius,radius};}
constexpr double shoulderEdgeControl=.5;// fraction of the width, from the edge, where the edge tangent ends
constexpr double shoulderSideControl=.42;// fraction of the depth, from the edge, where the side tangent ends
// One connected outline, used for real input regions and numerical QA.
inline std::vector<PointD> dockOutline(double width,double height,double radius,int edge,bool attached){
    double w=edge?height:width,h=edge?width:height,r=std::min({radius,w/2,h/2});std::vector<PointD> result;
    auto point=[&](double x,double y){result.push_back(edge?PointD{width-y,x}:PointD{x,y});};
    auto curve=[&](PointD a,PointD b,PointD c,PointD d){for(int i=1;i<=12;++i){double t=i/12.,q=1-t;point(q*q*q*a.x+3*q*q*t*b.x+3*q*t*t*c.x+t*t*t*d.x,q*q*q*a.y+3*q*q*t*b.y+3*q*t*t*c.y+t*t*t*d.y);}};
    const auto sh=shoulderShape(r);const double sw=sh.width,sd=sh.depth;
    if(attached){point(-sw,0);curve({-sw,0},{-sw*(1-shoulderEdgeControl),0},{0,sd*shoulderSideControl},{0,sd});}
    else{point(r,0);curve({r,0},{.448*r,0},{0,.448*r},{0,r});}
    if(!attached||sd<h-r)point(0,h-r);curve({0,h-r},{0,h-.448*r},{.448*r,h},{r,h});point(w-r,h);curve({w-r,h},{w-.448*r,h},{w,h-.448*r},{w,h-r});point(w,attached?sd:r);
    if(attached)curve({w,sd},{w,sd*shoulderSideControl},{w+sw*(1-shoulderEdgeControl),0},{w+sw,0});else curve({w,r},{w,.448*r},{w-.448*r,0},{w-r,0});
    return result;
}
}
