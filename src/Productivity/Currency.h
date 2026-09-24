#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <cwchar>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace nexus {
// Currency conversion from the European Central Bank's daily reference rates
// (https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml: free, public, no
// key). Everything here is pure; fetching happens in CommandService, only when the
// user has turned conversion on.
struct CurrencyInfo {const wchar_t* code;const wchar_t* symbol;const wchar_t* name;const wchar_t* words;int decimals;};
// Names are matched whole; symbols only when unambiguous ($ is taken as US dollars, ¥ as yen).
inline constexpr CurrencyInfo currencies[]={
    {L"EUR",L"€",L"euros",L"euro|euros",2},{L"USD",L"$",L"US dollars",L"dollar|dollars|us dollar|us dollars|buck|bucks",2},
    {L"GBP",L"£",L"British pounds",L"pound|pounds|sterling|quid|british pound|british pounds",2},{L"INR",L"₹",L"Indian rupees",L"rupee|rupees|indian rupee|indian rupees",2},
    {L"JPY",L"¥",L"Japanese yen",L"yen|japanese yen",0},{L"CNY",L"CN¥",L"Chinese yuan",L"yuan|renminbi|rmb|chinese yuan",2},
    {L"CHF",L"CHF ",L"Swiss francs",L"franc|francs|swiss franc|swiss francs",2},{L"AUD",L"A$",L"Australian dollars",L"australian dollar|australian dollars",2},
    {L"CAD",L"C$",L"Canadian dollars",L"canadian dollar|canadian dollars",2},{L"NZD",L"NZ$",L"New Zealand dollars",L"new zealand dollar|new zealand dollars",2},
    {L"HKD",L"HK$",L"Hong Kong dollars",L"hong kong dollar|hong kong dollars",2},{L"SGD",L"S$",L"Singapore dollars",L"singapore dollar|singapore dollars",2},
    {L"KRW",L"₩",L"South Korean won",L"won|korean won",0},{L"SEK",L"SEK ",L"Swedish kronor",L"swedish krona|swedish kronor",2},
    {L"NOK",L"NOK ",L"Norwegian kroner",L"norwegian krone|norwegian kroner",2},{L"DKK",L"DKK ",L"Danish kroner",L"danish krone|danish kroner",2},
    {L"PLN",L"zł ",L"Polish zloty",L"zloty|zlotys|złoty",2},{L"CZK",L"Kč ",L"Czech koruna",L"koruna|czech koruna",2},
    {L"HUF",L"HUF ",L"Hungarian forints",L"forint|forints",0},{L"RON",L"RON ",L"Romanian lei",L"leu|lei|romanian leu",2},
    {L"ISK",L"ISK ",L"Icelandic kronur",L"icelandic krona|icelandic kronur",0},{L"TRY",L"₺",L"Turkish lira",L"lira|turkish lira",2},
    {L"ILS",L"₪",L"Israeli shekels",L"shekel|shekels",2},{L"BRL",L"R$",L"Brazilian reais",L"real|reais|brazilian real",2},
    {L"MXN",L"MX$",L"Mexican pesos",L"mexican peso|mexican pesos",2},{L"ZAR",L"ZAR ",L"South African rand",L"rand|south african rand",2},
    {L"IDR",L"Rp ",L"Indonesian rupiah",L"rupiah",0},{L"MYR",L"RM ",L"Malaysian ringgit",L"ringgit",2},
    {L"PHP",L"₱",L"Philippine pesos",L"philippine peso|philippine pesos",2},{L"THB",L"฿",L"Thai baht",L"baht",2}};
inline const CurrencyInfo* currencyByCode(std::wstring_view code){for(auto& c:currencies)if(code==c.code)return &c;return nullptr;}
// Per-euro rates for one ECB publication date.
struct ExchangeRates {std::wstring date;std::map<std::wstring,double> perEuro;
    std::optional<double> rate(const std::wstring& code)const{if(code==L"EUR")return 1.;auto it=perEuro.find(code);if(it==perEuro.end()||!(it->second>0))return std::nullopt;return it->second;}
    bool empty()const{return perEuro.empty();}};
// Reads the ECB daily XML: <Cube time='2026-09-24'> ... <Cube currency='USD' rate='1.0845'/>.
inline std::optional<ExchangeRates> parseEcbRates(std::string_view xml){
    ExchangeRates r;auto attribute=[&](size_t from,size_t limit,std::string_view name)->std::optional<std::string>{
        for(size_t at=xml.find(name,from);at!=std::string_view::npos&&at<limit;at=xml.find(name,at+1)){size_t q=at+name.size();while(q<limit&&(xml[q]==' '||xml[q]=='='))++q;if(q>=limit||(xml[q]!='\''&&xml[q]!='"'))continue;const char quote=xml[q];size_t end=xml.find(quote,q+1);if(end==std::string_view::npos||end>limit)return std::nullopt;return std::string(xml.substr(q+1,end-q-1));}
        return std::nullopt;};
    for(size_t at=xml.find("<Cube");at!=std::string_view::npos;at=xml.find("<Cube",at+5)){
        const size_t end=xml.find('>',at);if(end==std::string_view::npos)break;
        if(auto t=attribute(at,end,"time")){if(t->size()==10)r.date=std::wstring(t->begin(),t->end());continue;}
        auto c=attribute(at,end,"currency"),v=attribute(at,end,"rate");if(!c||!v||c->size()!=3)continue;
        char* stop=nullptr;const double value=std::strtod(v->c_str(),&stop);if(!stop||*stop||!(value>0)||!std::isfinite(value))continue;
        r.perEuro[std::wstring(c->begin(),c->end())]=value;
    }
    if(r.perEuro.empty()||r.date.empty())return std::nullopt;return r;
}
struct CurrencyQuery {double amount=1;std::wstring from,to;bool explicitAmount=false;};
namespace currencydetail {
inline std::wstring lower(std::wstring s){for(auto& c:s)c=wchar_t(std::towlower(c));return s;}
// Longest currency named at the start of `words` (a code, a symbol or a spoken name); returns words used.
inline size_t currencyAt(const std::vector<std::wstring>& ws,size_t i,std::wstring& code){
    size_t best=0;for(auto& c:currencies){
        std::wstring_view names=c.words;std::vector<std::wstring> options{lower(c.code)};
        for(size_t a=0;a<=names.size();){size_t b=names.find(L'|',a);if(b==std::wstring_view::npos)b=names.size();options.emplace_back(names.substr(a,b-a));a=b+1;}
        std::wstring symbol=c.symbol;while(!symbol.empty()&&symbol.back()==L' ')symbol.pop_back();
        if(!symbol.empty()&&!std::iswalpha(symbol[0])&&symbol!=L"R$"&&symbol!=L"A$"&&symbol!=L"C$")options.push_back(symbol);
        for(auto& o:options){std::vector<std::wstring> parts;std::wstring w;for(wchar_t ch:o){if(ch==L' '){if(!w.empty())parts.push_back(w);w.clear();}else w+=ch;}if(!w.empty())parts.push_back(w);
            if(parts.empty()||i+parts.size()>ws.size()||parts.size()<=best)continue;bool same=true;for(size_t k=0;k<parts.size();++k)if(ws[i+k]!=parts[k]){same=false;break;}
            if(same){best=parts.size();code=c.code;}}
    }
    return best;
}
// "1,234.5" "12,5" "1.5k" "2m": a positive amount, or nothing.
inline std::optional<double> amountOf(std::wstring s){
    double scale=1;if(!s.empty()&&(s.back()==L'k'||s.back()==L'm')){scale=s.back()==L'k'?1e3:1e6;s.pop_back();}
    if(s.empty()||s.size()>18)return std::nullopt;size_t commas=0,dots=0;for(wchar_t c:s){if(c==L',')++commas;else if(c==L'.')++dots;else if(c<L'0'||c>L'9')return std::nullopt;}
    // A lone comma followed by one or two digits is a decimal comma ("12,5"); otherwise commas group thousands.
    if(dots==0&&commas==1&&s.size()-s.find(L',')-1<=2)s[s.find(L',')]=L'.';else{std::wstring t;for(wchar_t c:s)if(c!=L',')t+=c;s=t;}
    if(std::count(s.begin(),s.end(),L'.')>1||s==L".")return std::nullopt;
    const double v=std::wcstod(s.c_str(),nullptr)*scale;if(!(v>0)||!std::isfinite(v)||v>1e13)return std::nullopt;return v;
}
}
// "100 usd to inr", "$50 in €", "convert 20 pounds into euros", "usd inr" (1 unit), "20 eur"
// (to `home`). Anything else is not a currency question, so ordinary commands are never taken.
inline std::optional<CurrencyQuery> parseCurrency(const std::wstring& typed,const std::wstring& home){
    using namespace currencydetail;std::wstring t=lower(typed),spaced;
    // Separate digits from letters and symbols: "100usd" -> "100 usd", "$100" -> "$ 100".
    for(size_t i=0;i<t.size();++i){const wchar_t c=t[i];const bool digit=(c>=L'0'&&c<=L'9')||c==L'.'||c==L',';
        if(!spaced.empty()&&spaced.back()!=L' '){const wchar_t p=spaced.back();const bool pd=(p>=L'0'&&p<=L'9')||p==L'.'||p==L',';
            // "1.5k" and "2m" keep their suffix when a separator or the end follows.
            const bool suffix=pd&&(c==L'k'||c==L'm')&&(i+1==t.size()||t[i+1]==L' ');
            if(!suffix&&pd!=digit&&c!=L' ')spaced+=L' ';}
        spaced+=c==L'\t'?L' ':c;}
    std::vector<std::wstring> ws;{std::wstring w;for(wchar_t c:spaced){if(c==L' '){if(!w.empty())ws.push_back(w);w.clear();}else w+=c;}if(!w.empty())ws.push_back(w);}
    if(ws.empty())return std::nullopt;size_t i=0;if(ws[0]==L"convert"||ws[0]==L"exchange")i=1;
    CurrencyQuery q;std::wstring code;
    // Left side: [symbol|code] amount [currency]  or  currency
    if(i<ws.size()){if(size_t n=currencyAt(ws,i,code)){q.from=code;i+=n;}}
    if(i<ws.size()){if(auto a=amountOf(ws[i])){q.amount=*a;q.explicitAmount=true;++i;}}
    if(q.from.empty()&&i<ws.size()){if(size_t n=currencyAt(ws,i,code)){q.from=code;i+=n;}}
    if(q.from.empty())return std::nullopt;
    if(i==ws.size()){if(!q.explicitAmount)return std::nullopt;q.to=home==q.from?(q.from==L"USD"?L"EUR":L"USD"):home;return q;}
    if(ws[i]==L"to"||ws[i]==L"in"||ws[i]==L"into"||ws[i]==L"as"||ws[i]==L"="||ws[i]==L"->"||ws[i]==L"→")++i;
    if(i>=ws.size())return std::nullopt;
    const size_t n=currencyAt(ws,i,code);if(!n||i+n!=ws.size())return std::nullopt;q.to=code;
    // "usd inr" without a joining word is fine; a lone code on the left needs one ("usd to").
    return q;
}
// 8345.123 -> "8,345.12"; tiny values keep four significant figures.
inline std::wstring groupedNumber(double v,int decimals){
    if(!std::isfinite(v))return L"—";const bool negative=v<0;v=std::abs(v);
    if(v>0&&v<.01&&decimals>0)decimals=std::min(8,2+int(std::ceil(-std::log10(v))));
    wchar_t buf[64];swprintf(buf,64,L"%.*f",decimals,v);std::wstring s=buf;const size_t dot=s.find(L'.');std::wstring whole=s.substr(0,dot),frac=dot==std::wstring::npos?L"":s.substr(dot);
    std::wstring grouped;for(size_t k=0;k<whole.size();++k){if(k&&(whole.size()-k)%3==0)grouped+=L',';grouped+=whole[k];}
    return (negative?L"-":L"")+grouped+frac;
}
// "2026-09-24" -> "24 Sep".
inline std::wstring rateDay(const std::wstring& iso){
    static constexpr const wchar_t* months[]={L"Jan",L"Feb",L"Mar",L"Apr",L"May",L"Jun",L"Jul",L"Aug",L"Sep",L"Oct",L"Nov",L"Dec"};
    if(iso.size()!=10||iso[4]!=L'-'||iso[7]!=L'-')return iso;const int m=(iso[5]-L'0')*10+(iso[6]-L'0'),d=(iso[8]-L'0')*10+(iso[9]-L'0');
    if(m<1||m>12||d<1||d>31)return iso;return std::to_wstring(d)+L" "+months[m-1];
}
struct CurrencyAnswer {std::wstring answer,plain,detail;double value=0;};
// Converts through the euro, the ECB's base currency.
inline std::optional<CurrencyAnswer> convertCurrency(const CurrencyQuery& q,const ExchangeRates& rates){
    auto from=rates.rate(q.from),to=rates.rate(q.to);if(!from||!to)return std::nullopt;
    const auto* f=currencyByCode(q.from);const auto* t=currencyByCode(q.to);if(!f||!t)return std::nullopt;
    CurrencyAnswer a;a.value=q.amount/(*from)*(*to);std::wstring symbol=t->symbol;a.answer=symbol+groupedNumber(a.value,t->decimals);
    wchar_t plain[64];swprintf(plain,64,L"%.*f",a.value<.01&&t->decimals?6:t->decimals,a.value);a.plain=plain;
    a.detail=groupedNumber(q.amount,q.amount==std::floor(q.amount)?0:2)+L" "+f->code+L" in "+t->name+L"  ·  ECB rate of "+rateDay(rates.date);
    return a;
}
}
