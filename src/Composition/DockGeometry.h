#pragma once
#include <algorithm>
#include <vector>
#include <cmath>
namespace nexus {
struct PointD{double x,y;};
inline PointD bodyOrigin(double w,double h,double canvasW,double canvasH,int edge){return edge?PointD{canvasW-w,(canvasH-h)/2}:PointD{(canvasW-w)/2,0};}
// One connected outline, used for real input regions and numerical QA.
inline std::vector<PointD> dockOutline(double width,double height,double radius,int edge,bool attached){
    double w=edge?height:width,h=edge?width:height,r=std::min({radius,w/2,h/2});std::vector<PointD> result;
    auto point=[&](double x,double y){result.push_back(edge?PointD{width-y,x}:PointD{x,y});};
    auto curve=[&](PointD a,PointD b,PointD c,PointD d){for(int i=1;i<=12;++i){double t=i/12.,q=1-t;point(q*q*q*a.x+3*q*q*t*b.x+3*q*t*t*c.x+t*t*t*d.x,q*q*q*a.y+3*q*q*t*b.y+3*q*t*t*c.y+t*t*t*d.y);}};
    if(attached){point(-r,0);curve({-r,0},{-.2*r,0},{0,.2*r},{0,r});}
    else{point(r,0);curve({r,0},{.448*r,0},{0,.448*r},{0,r});}
    point(0,h-r);curve({0,h-r},{0,h-.448*r},{.448*r,h},{r,h});point(w-r,h);curve({w-r,h},{w-.448*r,h},{w,h-.448*r},{w,h-r});point(w,r);
    if(attached)curve({w,r},{w,.2*r},{w+.2*r,0},{w+r,0});else curve({w,r},{w,.448*r},{w-.448*r,0},{w-r,0});
    return result;
}
}
