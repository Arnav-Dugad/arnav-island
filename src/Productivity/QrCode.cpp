#include "QrCode.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
namespace nexus {
namespace {
// Error correction level M, versions 1 to 10: codewords of error correction per block, and the number of blocks.
constexpr int eccPerBlock[11]={0,10,16,26,18,24,16,18,22,22,26};
constexpr int blocksOf[11]={0,1,1,1,2,2,4,4,4,5,5};
constexpr int formatLevelM=0;
// Modules left for data and error correction once the function patterns are placed.
int rawDataModules(int version){
    int result=(16*version+128)*version+64;
    if(version>=2){const int align=version/7+2;result-=(25*align-10)*align-55;if(version>=7)result-=36;}
    return result;
}
int dataCodewords(int version){return rawDataModules(version)/8-eccPerBlock[version]*blocksOf[version];}
// Multiplication in GF(2^8) modulo x^8 + x^4 + x^3 + x^2 + 1.
uint8_t gfMultiply(uint8_t x,uint8_t y){int z=0;for(int i=7;i>=0;--i){z=(z<<1)^((z>>7)*0x11D);z^=((y>>i)&1)*x;}return uint8_t(z);}
std::vector<uint8_t> rsDivisor(int degree){
    std::vector<uint8_t> result(size_t(degree),0);result.back()=1;uint8_t root=1;
    for(int i=0;i<degree;++i){for(size_t j=0;j<result.size();++j){result[j]=gfMultiply(result[j],root);if(j+1<result.size())result[j]^=result[j+1];}root=gfMultiply(root,0x02);}
    return result;
}
std::vector<uint8_t> rsRemainder(const std::vector<uint8_t>& data,const std::vector<uint8_t>& divisor){
    std::vector<uint8_t> result(divisor.size(),0);
    for(uint8_t b:data){const uint8_t factor=b^result[0];result.erase(result.begin());result.push_back(0);for(size_t i=0;i<result.size();++i)result[i]^=gfMultiply(divisor[i],factor);}
    return result;
}
struct Grid{
    int size;std::vector<uint8_t> dark,function;
    explicit Grid(int s):size(s),dark(size_t(s*s),0),function(size_t(s*s),0){}
    uint8_t& at(int x,int y){return dark[size_t(y*size+x)];}
    bool isFunction(int x,int y)const{return function[size_t(y*size+x)]!=0;}
    void set(int x,int y,bool on){dark[size_t(y*size+x)]=on;function[size_t(y*size+x)]=1;}
    void finder(int cx,int cy){for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx){const int d=std::max(std::abs(dx),std::abs(dy)),x=cx+dx,y=cy+dy;if(x>=0&&x<size&&y>=0&&y<size)set(x,y,d!=2&&d!=4);}}
    void alignment(int cx,int cy){for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)set(cx+dx,cy+dy,std::max(std::abs(dx),std::abs(dy))!=1);}
    void formatBits(int mask){
        const int data=formatLevelM<<3|mask;int rem=data;for(int i=0;i<10;++i)rem=(rem<<1)^((rem>>9)*0x537);
        const int bits=(data<<10|rem)^0x5412;auto bit=[&](int i){return ((bits>>i)&1)!=0;};
        for(int i=0;i<=5;++i)set(8,i,bit(i));
        set(8,7,bit(6));set(8,8,bit(7));set(7,8,bit(8));
        for(int i=9;i<15;++i)set(14-i,8,bit(i));
        for(int i=0;i<8;++i)set(size-1-i,8,bit(i));
        for(int i=8;i<15;++i)set(8,size-15+i,bit(i));
        set(8,size-8,true);
    }
    void versionBits(int version){
        if(version<7)return;int rem=version;for(int i=0;i<12;++i)rem=(rem<<1)^((rem>>11)*0x1F25);
        const long bits=long(version)<<12|rem;
        for(int i=0;i<18;++i){const bool on=((bits>>i)&1)!=0;const int a=size-11+i%3,b=i/3;set(a,b,on);set(b,a,on);}
    }
    void applyMask(int mask){
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            bool invert=false;
            switch(mask){case 0:invert=(x+y)%2==0;break;case 1:invert=y%2==0;break;case 2:invert=x%3==0;break;case 3:invert=(x+y)%3==0;break;
                case 4:invert=(x/3+y/2)%2==0;break;case 5:invert=x*y%2+x*y%3==0;break;case 6:invert=(x*y%2+x*y%3)%2==0;break;default:invert=((x+y)%2+x*y%3)%2==0;break;}
            if(invert&&!isFunction(x,y))at(x,y)^=1;}
    }
    // The penalty rules of the standard (runs, blocks, finder-like patterns, balance), as reference encoders apply them.
    void addHistory(int run,int history[7])const{if(history[0]==0)run+=size;for(int i=6;i>0;--i)history[i]=history[i-1];history[0]=run;}
    int countPatterns(const int h[7])const{const int n=h[1];const bool core=n>0&&h[2]==n&&h[3]==n*3&&h[4]==n&&h[5]==n;return (core&&h[0]>=n*4&&h[6]>=n?1:0)+(core&&h[6]>=n*4&&h[0]>=n?1:0);}
    int terminate(bool color,int run,int history[7])const{if(color){addHistory(run,history);run=0;}run+=size;addHistory(run,history);return countPatterns(history);}
    long penalty()const{
        long result=0;
        for(int pass=0;pass<2;++pass)for(int a=0;a<size;++a){
            bool color=false;int run=0;int history[7]={0,0,0,0,0,0,0};
            for(int b=0;b<size;++b){const bool d=pass==0?dark[size_t(a*size+b)]!=0:dark[size_t(b*size+a)]!=0;
                if(d==color){++run;if(run==5)result+=3;else if(run>5)++result;}
                else{addHistory(run,history);if(!color)result+=countPatterns(history)*40;color=d;run=1;}}
            result+=terminate(color,run,history)*40;}
        for(int y=0;y<size-1;++y)for(int x=0;x<size-1;++x){const uint8_t c=dark[size_t(y*size+x)];if(c==dark[size_t(y*size+x+1)]&&c==dark[size_t((y+1)*size+x)]&&c==dark[size_t((y+1)*size+x+1)])result+=3;}
        long darkCount=0;for(uint8_t d:dark)darkCount+=d;const long total=long(size)*size;
        const long k=(std::labs(darkCount*20-total*10)+total-1)/total-1;result+=k*10;
        return result;
    }
};
}

QrCode qrEncode(const std::string& text,int mask){
    const size_t length=text.size();int version=0;
    for(int v=1;v<=10;++v){const int countBits=v<10?8:16;if(length<(size_t(1)<<countBits)&&4+countBits+8*length<=size_t(dataCodewords(v))*8){version=v;break;}}
    if(!version)return {};
    // The bits: byte mode, the length, the bytes, a terminator, then padding to the capacity.
    std::vector<bool> bits;auto append=[&](unsigned value,int count){for(int i=count-1;i>=0;--i)bits.push_back(((value>>i)&1)!=0);};
    append(4,4);append(unsigned(length),version<10?8:16);for(unsigned char c:text)append(c,8);
    const size_t capacity=size_t(dataCodewords(version))*8;
    append(0,int(std::min<size_t>(4,capacity-bits.size())));append(0,int((8-bits.size()%8)%8));
    for(unsigned pad=0xEC;bits.size()<capacity;pad^=0xEC^0x11)append(pad,8);
    std::vector<uint8_t> data(bits.size()/8,0);for(size_t i=0;i<bits.size();++i)if(bits[i])data[i>>3]|=uint8_t(1<<(7-(i&7)));
    // Split into blocks, each with its error correction, then interleaved.
    const int blocks=blocksOf[version],ecc=eccPerBlock[version],raw=rawDataModules(version)/8,shortBlocks=blocks-raw%blocks,shortLength=raw/blocks;
    const auto divisor=rsDivisor(ecc);std::vector<std::vector<uint8_t>> all;
    for(int i=0,k=0;i<blocks;++i){
        const int take=shortLength-ecc+(i<shortBlocks?0:1);std::vector<uint8_t> block(data.begin()+k,data.begin()+k+take);k+=take;
        const auto check=rsRemainder(block,divisor);if(i<shortBlocks)block.push_back(0);block.insert(block.end(),check.begin(),check.end());all.push_back(block);}
    std::vector<uint8_t> codewords;
    for(size_t i=0;i<all[0].size();++i)for(int j=0;j<blocks;++j)if(i!=size_t(shortLength-ecc)||j>=shortBlocks)codewords.push_back(all[size_t(j)][i]);
    // The function patterns: timing, finders, alignment, and room for the format (and version) bits.
    Grid g(version*4+17);const int size=g.size;
    for(int i=0;i<size;++i){g.set(6,i,i%2==0);g.set(i,6,i%2==0);}
    g.finder(3,3);g.finder(size-4,3);g.finder(3,size-4);
    if(version>=2){const int count=version/7+2,step=(version*4+count*2+1)/(count*2-2)*2;std::vector<int> at{6};
        for(int p=size-7;int(at.size())<count;p-=step)at.insert(at.begin()+1,p);
        for(size_t i=0;i<at.size();++i)for(size_t j=0;j<at.size();++j){if((i==0&&j==0)||(i==0&&j==at.size()-1)||(i==at.size()-1&&j==0))continue;g.alignment(at[i],at[j]);}}
    g.formatBits(0);g.versionBits(version);
    // The codewords, in the zigzag from the bottom right.
    size_t i=0;
    for(int right=size-1;right>=1;right-=2){if(right==6)right=5;
        for(int vert=0;vert<size;++vert)for(int j=0;j<2;++j){const int x=right-j;const bool upward=((right+1)&2)==0;const int y=upward?size-1-vert:vert;
            if(!g.isFunction(x,y)&&i<codewords.size()*8){g.at(x,y)=((codewords[i>>3]>>(7-(i&7)))&1)!=0;++i;}}}
    // The mask with the least penalty (or the one asked for).
    int chosen=mask;
    if(chosen<0||chosen>7){long best=LONG_MAX;
        for(int m=0;m<8;++m){g.applyMask(m);g.formatBits(m);const long p=g.penalty();if(p<best){best=p;chosen=m;}g.applyMask(m);}}
    g.applyMask(chosen);g.formatBits(chosen);
    QrCode code;code.size=size;code.modules=g.dark;return code;
}
}
