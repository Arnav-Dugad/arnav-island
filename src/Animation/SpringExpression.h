#pragma once
#include "Animation/MotionEngine.h"
#include <string>
#include <cwchar>
#include <algorithm>
#include <cmath>
namespace nexus {
// Composition expression text for a spring re-based at a local clock. Numbers are
// parenthesized so negative constants never form ambiguous operator pairs.
// Always plain decimal notation, about nine significant digits: the expression parser rejects exponents ("2e-05"),
// which a nearly settled spring's terms produce; smaller than 1e-9, a term is 0.
inline std::wstring expressionNumber(double value){
    if(!std::isfinite(value)||std::abs(value)<1e-9)return L"(0)";
    // Composition math is 32-bit floating point: nothing real is this large (and it keeps the text short).
    value=std::clamp(value,-1e12,1e12);
    const int magnitude=int(std::floor(std::log10(std::abs(value))));const int decimals=std::clamp(8-magnitude,0,17);
    wchar_t b[96];swprintf(b,96,L"%.*f",decimals,value);std::wstring s=b;
    if(s.find(L'.')!=std::wstring::npos){while(s.back()==L'0')s.pop_back();if(s.back()==L'.')s.pop_back();}
    if(s==L"-0")s=L"0";return L"("+s+L")";
}
inline std::wstring springExpression(const SpringTerms& s,double factor,const wchar_t* clock=L"p.t"){
    // Expressions have no Exp(); Pow(e,x) is the documented equivalent.
    const std::wstring t=clock;std::wstring e;
    auto exp=[&](double rate){return L"Pow(2.718281828459045,"+expressionNumber(rate)+L"*"+t+L")";};
    auto times=[&](double value){return expressionNumber(value)+L"*"+t;};
    switch(s.regime){
    case SpringTerms::Regime::Critical:e=expressionNumber(s.target)+L"+("+expressionNumber(s.x)+L"+"+times(s.b)+L")*"+exp(-s.a);break;
    case SpringTerms::Regime::Under:e=expressionNumber(s.target)+L"+"+exp(-s.a)+L"*("+expressionNumber(s.x)+L"*Cos("+times(s.w)+L")+"+expressionNumber(s.b)+L"*Sin("+times(s.w)+L"))";break;
    case SpringTerms::Regime::Over:e=expressionNumber(s.target)+L"+"+expressionNumber(s.c1)+L"*"+exp(s.r1)+L"+"+expressionNumber(s.c2)+L"*"+exp(s.r2);break;
    default:e=expressionNumber(s.target);break;
    }
    return L"("+e+L")*"+expressionNumber(factor);
}
}
