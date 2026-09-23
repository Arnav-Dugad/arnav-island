#pragma once
#include "Animation/MotionEngine.h"
#include <string>
#include <cwchar>
namespace nexus {
// Composition expression text for a spring re-based at a local clock. Numbers are
// parenthesized so negative constants never form ambiguous operator pairs.
inline std::wstring expressionNumber(double value){wchar_t b[48];swprintf(b,48,L"(%.9g)",std::isfinite(value)?value:0.);return b;}
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
