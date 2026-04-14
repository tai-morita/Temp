//#include "../Common/tstring.h"
#ifndef __TSTRING_H_20061202__
#define __TSTRING_H_20061202__
#if defined(_here_) && !defined(SUPPRESS_READINGHERE)
	#pragma message(_here_ "Reading")
#endif
#include <string>
#if defined (_MSC_VER) && !defined(USE_CRT_WCMB)
//#include <winnls.h>
#endif

#ifdef _WIN32
#include <TCHAR.h>
#include <sstream>
#else
typedef char TCHAR;
#endif


typedef std::basic_string       <TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstring       ;
typedef std::basic_ios          <TCHAR, std::char_traits<TCHAR> >					  tios          ;
typedef std::basic_streambuf    <TCHAR, std::char_traits<TCHAR> >	                  tstreambuf    ;
typedef std::basic_istream      <TCHAR, std::char_traits<TCHAR> >					  tistream      ;
typedef std::basic_ostream      <TCHAR, std::char_traits<TCHAR> >					  tostream      ;
typedef std::basic_iostream     <TCHAR, std::char_traits<TCHAR> >					  tiostream     ;
typedef std::basic_ostringstream<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tostringstream;
typedef std::basic_istringstream<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tistringstream;
typedef std::basic_stringbuf    <TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstringbuf    ;
typedef std::basic_stringstream <TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstringstream ;
typedef std::basic_filebuf      <TCHAR, std::char_traits<TCHAR> >					  tfilebuf      ;
typedef std::basic_ifstream     <TCHAR, std::char_traits<TCHAR> >					  tifstream     ;
typedef std::basic_ofstream     <TCHAR, std::char_traits<TCHAR> >					  tofstream     ;
typedef std::basic_fstream      <TCHAR, std::char_traits<TCHAR> >					  tfstream      ;

#if defined(UNICODE) || defined(_UNICODE)
#define tcout std::wcout
#define tcin std::wcin
#define tcerr std::wcerr
#define tclog std::wclog
#else
#define tcout std::cout
#define tcin std::cin
#define tcerr std::cerr
#define tclog std::clog
#endif

#if _MSC_VER > 1300	// VC7++ 用
	#define TSTRCPY_S(dest, len, source) _tcscpy_s(dest, len, source)
#elif defined(_MSC_VER)	// VC6++ 用
	#define TSTRCPY_S(dest, len, source) _tcscpy(dest, source)
#else

#endif

#if _MSC_VER > 1300
	#define WSTRCPY_S(dest, len, source) wcscpy_s(dest, len, source);
#elif defined(_MSC_VER)	// VC6++ 用
	#define WSTRCPY_S(dest, len, source) wcscpy(dest, source);
#else
#endif

#if _MSC_VER > 1300
	#define STRCPY_S(dest, len, source) strcpy_s(dest, len, source);
#elif defined(_MSC_VER)	// VC6++ 用
	#define STRCPY_S(dest, len, source) strcpy(dest, source);
#else
#endif


/****************************
	Unicode/Unicode.h から移動
*****************************/
#if defined (_MSC_VER) && !defined(USE_CRT_WCMB) && !defined(TSTRING_DISABLE_UNICODE_UTILS)
#include <winnls.h>

// *.cpp は、作りたくないので、オブジェクトとして実装する。

struct WString2String : public std::string {
	virtual ~WString2String () {}
	WString2String () {};
	WString2String(const std::string& rwstrInput) {
		assign(rwstrInput);
	}
	WString2String(const std::wstring& rwstrInput) {
		Assign(rwstrInput);
	}
protected:
	void Assign(const std::wstring& rwstrInput, UINT uiCodePage = CP_THREAD_ACP) {
		size_t uiSize = rwstrInput.length();
		if (uiSize) {
			int iLen = ::WideCharToMultiByte(uiCodePage, 0, rwstrInput.c_str(), -1, NULL, 0, NULL, NULL);
//			TRACE_HERE("iLen = %d\r\n", iLen);
			std::string::resize(iLen);
			char *pcOutput = new char[iLen+1];
			::WideCharToMultiByte(uiCodePage, 0, rwstrInput.c_str(), -1, pcOutput, iLen, NULL, NULL);
			assign(pcOutput);
			delete[] pcOutput;
		}
	}
};

struct String2WString : public std::wstring {
	virtual ~String2WString () {}
	String2WString () {}
	String2WString(const std::wstring& rwstrInput) {
		assign(rwstrInput);
	}
	String2WString(const std::string& rstrInput) {
		Assign(rstrInput);
	}
protected:
	void Assign(const std::string& rstrInput, UINT uiCodePage = CP_THREAD_ACP) {
		size_t uiSize = rstrInput.length();
		if (uiSize) {
			int iLen = ::MultiByteToWideChar(uiCodePage, 0, rstrInput.c_str(), -1, NULL, 0);
//			TRACE_HERE("iLen = %d\r\n", iLen);
			std::wstring::resize(iLen);
			wchar_t *pwcOutput = new wchar_t[iLen+1];
			::MultiByteToWideChar(uiCodePage, 0, rstrInput.c_str(), -1, pwcOutput, iLen);
			assign(pwcOutput);
			delete[] pwcOutput;
		}
	}
};
// TCHAR 対応
#ifdef _UNICODE 
	typedef      WString2String TString2String;
	typedef      String2WString String2TString;
	typedef std::wstring        TString2WString;
	typedef std::wstring        WString2TString;
#else
	typedef std::string         TString2String;
	typedef std::string         String2TString;
	typedef      String2WString TString2WString;
	typedef      WString2String WString2TString;
#endif // _UNICODE

struct Utf16le2CodePage : public WString2String {
	Utf16le2CodePage(const std::wstring& rwstrInput, uint16_t usCodePage) {
		Assign(rwstrInput, usCodePage);
	}
};
struct CodePage2Utf16le : public String2WString {
	CodePage2Utf16le(const std::string& rstrInput, uint16_t usCodePage) {
		Assign(rstrInput, usCodePage);
	}
};

struct Utf16leToUtf8 : public WString2String {
	Utf16leToUtf8(const std::wstring& rstrInput) {
		Assign(rstrInput, CP_UTF8);
	}
};
struct Utf8ToUtf16le : public String2WString {
	Utf8ToUtf16le(const std::string& rstrInput) {
		Assign(rstrInput, CP_UTF8);
	}
};

struct Unicode2SJis : public Utf16le2CodePage {
	Unicode2SJis (const std::wstring& rwstrInput): Utf16le2CodePage(rwstrInput, 932) {}
};

struct SJis2Unicode : public CodePage2Utf16le {
	SJis2Unicode (const std::string& rstrInput): CodePage2Utf16le(rstrInput, 932) {}
};

typedef WString2String _A;
typedef String2WString _W;
typedef TString2String  _T2A;
typedef String2TString  _A2T;
typedef TString2WString _T2W;
typedef WString2TString _W2T;



#else	// #if defined (_MSC_VER) && !defined(USE_CRT_WCMB)
struct WString2String : public std::string {
	WString2String(const std::wstring& rwstrInput) {
		int iSize = rwstrInput.length();
		if (iSize) {
			int iLen = iSize;
//			TRACE_HERE("iLen = %d\r\n", iLen);
			std::string::resize(iLen);
			size_t uiBufferSize = sizeof(wchar_t) * (iLen + 1);
//			TRACE_HERE("uiBufferSize = %d\r\n", uiBufferSize);
			char* pcOutput = new char[uiBufferSize];
			std::fill_n(pcOutput, iLen, 0);
			// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=msvc-170
			size_t uiRet = 0;
			errno_t errRet = wcstombs_s(&uiRet, pcOutput, uiBufferSize, rwstrInput.c_str(), iLen * sizeof(wchar_t));
//			TRACE_HERE("uiRet = %d\r\n", uiRet);
			assign(pcOutput);
			delete[] pcOutput; pcOutput = 0;
		}
	}
};

struct String2WString : public std::wstring {
	String2WString(const std::string& rstrInput) {
		size_t uiPos = 0;
		size_t uiLetterCount = 0;
		while (rstrInput[uiPos] != '\0') {
			size_t uiLen = mblen(&rstrInput[uiPos], MB_CUR_MAX);
			if (uiLen == (size_t)-1) {
				break;
			}
			uiPos += uiLen;
			++uiLetterCount;
		}
		size_t uiSize = rstrInput.length();
//		TRACE_HERE("uiLetterCount = %ld\r\n", uiLetterCount);
		if (uiSize > 0) {
			std::wstring::resize(uiLetterCount);
			wchar_t *pwcOutput = new wchar_t[uiLetterCount+1];
			std::fill_n(pwcOutput, uiLetterCount, 0);
			// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/mbstowcs-s-mbstowcs-s-l?view=msvc-170
			size_t uiResult = 0;
			errno_t err = mbstowcs_s(&uiResult, pwcOutput, uiLetterCount + 1, rstrInput.c_str(), uiLetterCount);
//			printf("uiResult = %ld\n", uiResult);
//			wprintf(L"pwcOutput = %S\n", pwcOutput);
			assign(pwcOutput, uiLetterCount);
			delete[] pwcOutput; pwcOutput = 0;
		}
	}
};

typedef WString2String Unicode2SJis;
typedef String2WString SJis2Unicode;
#endif	// __APPLE__ か _MSC_VER

#endif //__TSTRING_H_20061202__













