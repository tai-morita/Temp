//#include "../Common/Sprintf.h"
#ifndef __STRSPRINTF_H__
#define __STRSPRINTF_H__
#if defined(_here_) && !defined(SUPPRESS_READINGHERE)
	#pragma message(_here_ "Reading")
#endif

//////////////////////////////////////////////////
// string を直接返す sprintf()
// 次のように使用する。
// AfxMessageBoxId(MBID_ERROR, "zlqk", Sprintf("error code = %d", iError).c_str());
// Sprintf は _UNICODEが定義されているとwstringを継承し、定義されていなければstringを継承する。(sadakane 20061103)
// SprintfA は stringを継承する。(sadakane 20130121)
// SprintfW は wstringを継承する。(sadakane 20130121)

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <sstream>
#include "tstring.h"

template<class T_string>
struct _ToUpperT : public T_string {
	_ToUpperT(T_string& rstr) {
		{for(typename T_string::iterator it = rstr.begin(); it!=rstr.end(); ++it){
			append(1, toupper(*it));
		}}
	}
};

template<class T_string>
struct _ToLowerT : public T_string {
	_ToLowerT(T_string& rstr) {
		{for(typename T_string::iterator it = rstr.begin(); it!=rstr.end(); ++it){
			append(1, tolower(*it));
		}}
	}
};

typedef _ToUpperT<     tstring>			ToUpper ;
typedef _ToUpperT<std::string >			ToUpperA;
typedef _ToUpperT<std::wstring>			ToUpperW;
typedef _ToLowerT<     tstring>			ToLower ;
typedef _ToLowerT<std::string >			ToLowerA;
typedef _ToLowerT<std::wstring>			ToLowerW;


template<
	class T_string, 
	class value_type, 
	int (*fnVsprintf_s)(value_type *, size_t, const value_type *, va_list),
	int (*fn_vscprintf)(const value_type *, va_list),
	int T_iMaxLen = 5000
>
class _SprintfT : public T_string {
public:
	_SprintfT() {}
	_SprintfT(const value_type* pcFormat, ...) {
		va_list argpointer;
		va_start(argpointer, pcFormat);
		FormatV(pcFormat, argpointer);
		va_end(argpointer);
	}
	operator value_type* () {
		return &*T_string::begin();
	}
	_SprintfT& FormatV(const T_string& rstrFormat, va_list argpointer) {
		VSprintf(rstrFormat, argpointer);
		return *this;
	}
private:
	void VSprintf(const T_string& rstrFormat, va_list argpointer) {
#if _MSC_VER >= 1400
		// vsprintf_sに異常がある時でもプログラムを停止させない
		_invalid_parameter_handler fnOldHandler;
		fnOldHandler = _set_invalid_parameter_handler(InvalidParameterHandlerForVSprintf);
		int iOldReportMode = _CrtSetReportMode(_CRT_ASSERT, 0);
		value_type acBuffer[T_iMaxLen];
		int iResult = fnVsprintf_s(acBuffer, T_iMaxLen, rstrFormat.c_str(), argpointer);
		if (iResult!=-1) {
			this->assign(acBuffer);
		} else {
			int iStringLength = fn_vscprintf(rstrFormat.c_str(), argpointer); // 必要なバッファ長を取得する
			iStringLength++; // '\0'の分を追加
			value_type *pcBuffer = new value_type[iStringLength];
			if(pcBuffer) {
				fnVsprintf_s(pcBuffer, iStringLength, rstrFormat.c_str(), argpointer);
				this->assign(pcBuffer);
				delete[] pcBuffer;
			}
		}
		_CrtSetReportMode(_CRT_ASSERT, iOldReportMode);
		_set_invalid_parameter_handler(fnOldHandler);
#else
		value_type acBuffer[T_iMaxLen];
		_vstprintf(acBuffer, rstrFormat.c_str(), argpointer);
		assign(acBuffer);
#endif
	}
#if _MSC_VER >= 1400
	// _s関数で異常が発生した時のエラーハンドラ
	static void InvalidParameterHandlerForVSprintf(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved) {}
#endif
};

typedef _SprintfT<     tstring, TCHAR, _vstprintf_s, _vsctprintf>	Sprintf ;
typedef _SprintfT<std::string , char , vsprintf_s  , _vscprintf >	SprintfA;
typedef _SprintfT<std::wstring, wchar_t, vswprintf_s , _vscwprintf>	SprintfW;

typedef SprintfA CstrSprintf;


//******** デバグ用プリントのインデントを制御するための文字列 ********
struct IndentA : public std::string {
	IndentA(int iIndent) { std::string::assign(SprintfA(SprintfA("%%%ds", 4 * iIndent).c_str(), "")); }
};
struct IndentW : public std::wstring {
	IndentW(int iIndent) { std::wstring::assign(SprintfW(SprintfW(L"%%%ds", 4 * iIndent).c_str(), L"")); }
};
#ifdef _UNICODE
typedef IndentW Indent ;
#else
typedef IndentA Indent ;
#endif

//#pragma warning(pop)			// エラーレベルを元に戻す

#endif //__STRSPRINTF_H__
