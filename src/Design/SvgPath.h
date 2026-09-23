#pragma once
#include <cmath>
#include <cstdlib>
#include <cctype>
namespace nexus {
// Minimal SVG path-data reader (M L H V C S Q T A Z, absolute and relative).
// It reports absolute segments to a sink, so rendering and tests share it.
struct SvgSink {
    virtual ~SvgSink()=default;
    virtual void move(float x,float y)=0;virtual void line(float x,float y)=0;
    virtual void cubic(float x1,float y1,float x2,float y2,float x,float y)=0;
    virtual void quad(float x1,float y1,float x,float y)=0;
    virtual void arc(float rx,float ry,float rotation,bool large,bool sweep,float x,float y)=0;
    virtual void close()=0;
};
class SvgPathReader {
    const char* p_;bool ok_=true;
    void space(){while(*p_&&(std::isspace(static_cast<unsigned char>(*p_))||*p_==','))++p_;}
    bool number(float& out){space();char* end=nullptr;double v=std::strtod(p_,&end);if(end==p_){ok_=false;return false;}p_=end;out=float(v);return std::isfinite(out);}
    bool flag(bool& out){space();if(*p_=='0'||*p_=='1'){out=*p_=='1';++p_;return true;}ok_=false;return false;}
    bool more(){space();return *p_&&(std::isdigit(static_cast<unsigned char>(*p_))||*p_=='-'||*p_=='+'||*p_=='.');}
public:
    explicit SvgPathReader(const char* d):p_(d){}
    bool read(SvgSink& sink){
        float cx=0,cy=0,sx=0,sy=0,lx=0,ly=0;char previous=0,command=0;
        for(;;){
            space();if(!*p_)break;
            if(std::isalpha(static_cast<unsigned char>(*p_)))command=*p_++;else if(!command)return false;
            const bool rel=std::islower(static_cast<unsigned char>(command))!=0;const char c=char(std::toupper(static_cast<unsigned char>(command)));
            float ox=rel?cx:0,oy=rel?cy:0;
            switch(c){
            case 'Z':sink.close();cx=sx;cy=sy;previous='Z';command=0;continue;
            case 'M':{float x,y;if(!number(x)||!number(y))return false;cx=ox+x;cy=oy+y;sx=cx;sy=cy;sink.move(cx,cy);command=rel?'l':'L';previous='M';continue;}
            case 'L':{float x,y;if(!number(x)||!number(y))return false;cx=ox+x;cy=oy+y;sink.line(cx,cy);break;}
            case 'H':{float x;if(!number(x))return false;cx=ox+x;sink.line(cx,cy);break;}
            case 'V':{float y;if(!number(y))return false;cy=(rel?cy:0)+y;sink.line(cx,cy);break;}
            case 'C':{float a[6];for(float& v:a)if(!number(v))return false;sink.cubic(ox+a[0],oy+a[1],ox+a[2],oy+a[3],ox+a[4],oy+a[5]);lx=ox+a[2];ly=oy+a[3];cx=ox+a[4];cy=oy+a[5];previous='C';if(!more())command=0;continue;}
            case 'S':{float a[4];for(float& v:a)if(!number(v))return false;float x1=previous=='C'?2*cx-lx:cx,y1=previous=='C'?2*cy-ly:cy;sink.cubic(x1,y1,ox+a[0],oy+a[1],ox+a[2],oy+a[3]);lx=ox+a[0];ly=oy+a[1];cx=ox+a[2];cy=oy+a[3];previous='C';if(!more())command=0;continue;}
            case 'Q':{float a[4];for(float& v:a)if(!number(v))return false;sink.quad(ox+a[0],oy+a[1],ox+a[2],oy+a[3]);lx=ox+a[0];ly=oy+a[1];cx=ox+a[2];cy=oy+a[3];previous='Q';if(!more())command=0;continue;}
            case 'T':{float a[2];for(float& v:a)if(!number(v))return false;float x1=previous=='Q'?2*cx-lx:cx,y1=previous=='Q'?2*cy-ly:cy;sink.quad(x1,y1,ox+a[0],oy+a[1]);lx=x1;ly=y1;cx=ox+a[0];cy=oy+a[1];previous='Q';if(!more())command=0;continue;}
            case 'A':{float rx,ry,rot,x,y;bool large,sweep;if(!number(rx)||!number(ry)||!number(rot)||!flag(large)||!flag(sweep)||!number(x)||!number(y))return false;cx=ox+x;cy=oy+y;sink.arc(std::abs(rx),std::abs(ry),rot,large,sweep,cx,cy);break;}
            default:return false;
            }
            previous=c;if(!more())command=0;
        }
        return ok_;
    }
};
}
