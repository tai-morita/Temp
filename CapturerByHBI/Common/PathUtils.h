//#include "../Common/PathUtils.h"
#ifndef __SHLWAPIWRAPPER__
#define __SHLWAPIWRAPPER__
#if defined(_here_) && !defined(SUPPRESS_READINGHERE)
	#pragma message(_here_ "Reading")
#endif
#include "../Common/tstring.h"

#if !defined(_WIN32)
// gcc 用は未実装
#else
/*
 255 文字よりも長いパスを使うには PathUtils.h を include する前に
 PATHUTILS_ALLOW_PATH_OVER_255 を定義すること.
 これには副作用がある. Windows 8 以上でしかサポートされていない機能を使う.
 #define PATHUTILS_ALLOW_PATH_OVER_255
 */

#include <shlwapi.h>
#include <shlobj.h>
#include <imagehlp.h>
#include <shellapi.h>	// for SHFileOperationW, SHFileOperationA
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "imagehlp.lib")
#pragma comment(lib, "Shell32.lib")

#include <vector>
#include <string>

#ifdef PATHUTILS_ALLOW_PATH_OVER_255
#pragma message(_here_ "############  WINDOWS 8 以上でしかサポートされない  ##################")
#include <pathcch.h>	// 256 文字以上のパス
#pragma comment(lib, "Pathcch.lib")
#endif

//! SHLWAPI Wrapper Functions をラップする関数群
//! <http://msdn.microsoft.com/en-us/library/bb759844(VS.85).aspx>
namespace PathUtils {
	// 補助関数の宣言
	static std::string _removeLeadingSeparator(const std::string& rstrIn);
	static std::wstring _removeLeadingSeparator(const std::wstring& rstrIn);

	//! パスが存在する場合に true を返す。ファイルかディレクトリかは問わない.
	inline bool CPathFileExists(const tstring& rstrFilePath) {
		return ::PathFileExists(rstrFilePath.c_str()) == TRUE;
	}
	inline bool CPathFileExistsW(const std::wstring& rwstrFilePath) {
		return ::PathFileExistsW(rwstrFilePath.c_str()) == TRUE;
	}
	inline bool CPathFileExistsA(const std::string& rwstrFilePath) {
		return ::PathFileExistsA(rwstrFilePath.c_str()) == TRUE;
	}
	//! パスが UNC パスである場合に true を返す。
	inline bool CPathIsUNC(const tstring& rstrPath) {
#ifdef _UNICODE 
		return ::PathIsUNCW(rstrPath.c_str()) == TRUE;
#else
		return ::PathIsUNCA(rstrPath.c_str()) == TRUE;
#endif
	}
	inline bool CPathIsUNCW(const std::wstring& rwstrPath) {
		return ::PathIsUNCW(rwstrPath.c_str()) == TRUE;
	}
	inline bool CPathIsUNCA(const std::string& rstrPath) {
		return ::PathIsUNCA(rstrPath.c_str()) == TRUE;
	}
	
	//! パス文字列がネットワークリソースを表すかどうかを判定する。アクセス権をとわない.MS-DOS SUBST コマンドや DefineDosDevice 関数でマップしたドライブはネットワークドライブと認識できないので、GetDriveType を使用する.
	inline bool CPathIsNetworkPath(const tstring& rstrFilePath) {
		return ::PathIsNetworkPath(rstrFilePath.c_str()) == TRUE;
	}
	inline bool CPathIsNetworkPathW(const std::wstring& rwstrFilePath) {
		return ::PathIsNetworkPathW(rwstrFilePath.c_str()) == TRUE;
	}
	inline bool CPathIsNetworkPathA(const std::string& rstrFilePath) {
		return ::PathIsNetworkPathA(rstrFilePath.c_str()) == TRUE;
	}

	//! 指定されたドライブの種類を調べます。交換可能か固定か、CD-ROM か RAM ディスクかネットワークドライブかを判別できます。
	//! https://msdn.microsoft.com/ja-jp/library/cc429310.aspx
	//! DRIVE_UNKNOWN, DRIVE_NO_ROOT_DIR, DRIVE_REMOVABLE, DRIVE_FIXED, DRIVE_REMOTE, DRIVE_CDROM, DRIVE_RAMDISK
	inline UINT CGetDriveType(const tstring& rstrFilePath) {
		return ::GetDriveType(rstrFilePath.c_str());
	}
	inline UINT CGetDriveTypeW(const std::wstring& rwstrFilePath) {
		return ::GetDriveTypeW(rwstrFilePath.c_str());
	}
	inline UINT CGetDriveTypeA(const std::string& rstrFilePath) {
		return ::GetDriveTypeA(rstrFilePath.c_str());
	}


	//! ディレクトリ名の後ろのバックスラッシュを消す。
	struct CPathRemoveBackslashW : public std::wstring {
		CPathRemoveBackslashW(const std::wstring& rwstrDirectoryPath) {
#ifdef PATHUTILS_ALLOW_PATH_OVER_255
			const int kiLongMaxPath = 32767;
			wchar_t pBuffer[kiLongMaxPath];
			WSTRCPY_S(pBuffer, kiLongMaxPath, rwstrDirectoryPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			HRESULT hr = ::PathCchRemoveBackslash(lpstrPath, kiLongMaxPath);
#else
			wchar_t pBuffer[MAX_PATH];
			WSTRCPY_S(pBuffer, MAX_PATH, rwstrDirectoryPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			::PathRemoveBackslashW(lpstrPath);
#endif
			assign(lpstrPath);
		}
	};
	struct CPathRemoveBackslashA : public std::string {
		CPathRemoveBackslashA(const std::string& rstrDirectoryPath) {
			char pBuffer[MAX_PATH];
			STRCPY_S(pBuffer, MAX_PATH, rstrDirectoryPath.c_str());
			LPSTR lpstrPath = pBuffer;
			::PathRemoveBackslashA(lpstrPath);
			assign(lpstrPath);
		}
	};
#ifdef _UNICODE
	typedef CPathRemoveBackslashW CPathRemoveBackslash;
#else
	typedef CPathRemoveBackslashA CPathRemoveBackslash;
#endif

	//! ディレクトリ名の後ろのバックスラッシュを追加する。
	struct CPathAddBackslashW: public std::wstring  {
		CPathAddBackslashW(const std::wstring& rwstrDirectoryPath) {
#ifdef PATHUTILS_ALLOW_PATH_OVER_255
			const int kiLongMaxPath = 32767;
			wchar_t pBuffer[kiLongMaxPath];
			WSTRCPY_S(pBuffer, kiLongMaxPath, rwstrDirectoryPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			HRESULT hr = ::PathCchAddBackslash(lpstrPath, kiLongMaxPath);
#else
			wchar_t pBuffer[MAX_PATH];
			WSTRCPY_S(pBuffer, MAX_PATH, rwstrDirectoryPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			::PathAddBackslashW(lpstrPath);
#endif
			assign(lpstrPath);
		}
	};
	struct CPathAddBackslashA: public std::string  {
		CPathAddBackslashA(const std::string& rstrDirectoryPath) {
			char pBuffer[MAX_PATH];
			STRCPY_S(pBuffer, MAX_PATH, rstrDirectoryPath.c_str());
			LPSTR lpstrPath = pBuffer;
			::PathAddBackslashA(lpstrPath);
			assign(lpstrPath);
		}
	};
#ifdef _UNICODE
	typedef CPathAddBackslashW CPathAddBackslash;
#else
	typedef CPathAddBackslashA CPathAddBackslash;
#endif

	//! パスが存在し、ディレクトリであるときに true を返す。
	static bool CPathIsDirectory(const tstring& rstrDirectoryPath) {
		return !!::PathIsDirectory(rstrDirectoryPath.c_str());
	}
	static bool CPathIsDirectoryW(const std::wstring& rwstrDirectoryPath) {
		return !!::PathIsDirectoryW(rwstrDirectoryPath.c_str());
	}
	static bool CPathIsDirectoryA(const std::string& rstrDirectoryPath) {
		return !!::PathIsDirectoryA(rstrDirectoryPath.c_str());
	}

	//! 相対パスであるときに true を返す。存在するかどうかに無頓着
	//! e.g.
	//! true : hoge
	//! true : ../hoge
	//! false: \\\\192.168.17.8\\fuga
	//! false: c:\\hoge
	static bool CPathIsRelative(const tstring& rstrDirectoryPath) {
		return !!::PathIsRelative(rstrDirectoryPath.c_str());
	}
	static bool CPathIsRelativeW(const std::wstring& rwstrDirectoryPath) {
		return !!::PathIsRelativeW(rwstrDirectoryPath.c_str());
	}
	static bool CPathIsRelativeA(const std::string& rstrDirectoryPath) {
		return !!::PathIsRelativeA(rstrDirectoryPath.c_str());
	}

	//! '.' を含む拡張子を返す。C:\TEST\sample3.4.txt -> .txt
	struct CPathFindExtensionW :public std::wstring {
		CPathFindExtensionW(const std::wstring& rwstrFilePath) {
#ifdef PATHUTILS_ALLOW_PATH_OVER_255
			const int kiLongMaxPath = 32767;
			wchar_t pBuffer[kiLongMaxPath];
			WSTRCPY_S(pBuffer, kiLongMaxPath, rwstrFilePath.c_str());
			wchar_t* pcExtension = 0;
			HRESULT hr = PathCchFindExtension(pBuffer, kiLongMaxPath, &pcExtension);
#else
			wchar_t pBuffer[MAX_PATH];
			WSTRCPY_S(pBuffer, MAX_PATH, rwstrFilePath.c_str());
			wchar_t* pcExtension = ::PathFindExtensionW(pBuffer);
#endif
			assign(pcExtension);
		}
	};
	struct CPathFindExtensionA :public std::string {
		CPathFindExtensionA(const std::string& rstrFilePath) {
			char pBuffer[MAX_PATH];
			STRCPY_S(pBuffer, MAX_PATH, rstrFilePath.c_str());
			char* pcExtension = ::PathFindExtensionA(pBuffer);
			assign(pcExtension);
		}
	};
#ifdef _UNICODE
	typedef CPathFindExtensionW CPathFindExtension;
#else
	typedef CPathFindExtensionA CPathFindExtension;
#endif

	//! ファイル名部分を得る.C:\TEST\sample3.4.txt -> sample3.4.txt, C:\TEST\ -> TEST\, c:\ -> c:, path -> path
	// <http://msdn.microsoft.com/en-us/library/bb773589(v=vs.85).aspx>
	struct CPathFindFileName :public tstring {
		CPathFindFileName(const tstring& rstrFilePath) {
			TCHAR* pBuffer = new TCHAR [rstrFilePath.size() + 1];
			TSTRCPY_S(pBuffer, rstrFilePath.size() + 1, rstrFilePath.c_str());
			TCHAR* pcFileName = ::PathFindFileName(pBuffer);
			assign(pcFileName);
			delete[] pBuffer;
		}
	};
	struct CPathFindFileNameW :public std::wstring {
		CPathFindFileNameW(const std::wstring& rwstrFilePath) {
			wchar_t* pBuffer = new wchar_t [rwstrFilePath.size() + 1];
			WSTRCPY_S(pBuffer, rwstrFilePath.size() + 1, rwstrFilePath.c_str());
			wchar_t* pcFileName = ::PathFindFileNameW(pBuffer);
			assign(pcFileName);
			delete[] pBuffer;
		}
	};
	struct CPathFindFileNameA :public std::string {
		CPathFindFileNameA(const std::string& rstrFilePath) {
			char* pBuffer = new char [rstrFilePath.size() + 1];
			STRCPY_S(pBuffer, rstrFilePath.size() + 1, rstrFilePath.c_str());
			char* pcFileName = ::PathFindFileNameA(pBuffer);
			assign(pcFileName);
			delete[] pBuffer;
		}
	};

	//! 拡張子部分を削除する. C:\TEST\sample3.4.txt -> C:\TEST\sample3.4
	struct CPathRemoveExtension : public tstring {
		CPathRemoveExtension(const tstring& rstrPath) {
			TCHAR* pBuffer = new TCHAR [rstrPath.size() + 1];
			TSTRCPY_S(pBuffer, rstrPath.size() + 1, rstrPath.c_str());
			LPTSTR lpstrPath = pBuffer;
			::PathRemoveExtension(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
	struct CPathRemoveExtensionA : public std::string {
		CPathRemoveExtensionA(const std::string& rstrPath) {
			char* pBuffer = new char [rstrPath.size() + 1];
			STRCPY_S(pBuffer, rstrPath.size() + 1, rstrPath.c_str());
			LPSTR lpstrPath = pBuffer;
			::PathRemoveExtensionA(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
	struct CPathRemoveExtensionW : public std::wstring {
		CPathRemoveExtensionW(const std::wstring& rstrPath) {
			wchar_t* pBuffer = new wchar_t [rstrPath.size() + 1];
			WSTRCPY_S(pBuffer, rstrPath.size() + 1, rstrPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			::PathRemoveExtensionW(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
	//! Removes the trailing file name and backslash from a path, if they are present.
	struct CPathRemoveFileSpecA: public std::string {
		CPathRemoveFileSpecA(const std::string& rwstrPath) {
			char* pBuffer = new char [rwstrPath.size() + 1];
			STRCPY_S(pBuffer, rwstrPath.size() + 1, rwstrPath.c_str());
			LPSTR lpstrPath = pBuffer;
			::PathRemoveFileSpecA(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
	struct CPathRemoveFileSpecW: public std::wstring {
		CPathRemoveFileSpecW(const std::wstring& rwstrPath) {
			wchar_t* pBuffer = new wchar_t [rwstrPath.size() + 1];
			WSTRCPY_S(pBuffer, rwstrPath.size() + 1, rwstrPath.c_str());
			LPWSTR lpstrPath = pBuffer;
#ifdef PATHUTILS_ALLOW_PATH_OVER_255
			HRESULT hr = ::PathCchRemoveFileSpec(lpstrPath, rwstrPath.size() + 1);
#else
			::PathRemoveFileSpecW(lpstrPath);
#endif
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
#ifdef _UNICODE
	typedef CPathRemoveFileSpecW CPathRemoveFileSpec;
#else
	typedef CPathRemoveFileSpecA CPathRemoveFileSpec;
#endif

	//! Removes any arguments from a given path.
	struct CPathRemoveArgs: public tstring {
		CPathRemoveArgs(const tstring& rstrCommand){
			TCHAR* pBuffer = new TCHAR [rstrCommand.size() + 1];
			TSTRCPY_S(pBuffer, rstrCommand.size() + 1, rstrCommand.c_str());
			LPTSTR lpstrPath = pBuffer;
			::PathRemoveArgs(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};

	//! Removes quotes from the beginning and end of a path. 
	struct CPathUnquoteSpacesA: public std::string {
		CPathUnquoteSpacesA(const std::string& rwstrPath) {
			char* pBuffer = new char [rwstrPath.size() + 1];
			STRCPY_S(pBuffer, rwstrPath.size() + 1, rwstrPath.c_str());
			LPSTR lpstrPath = pBuffer;
			::PathUnquoteSpacesA(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
	struct CPathUnquoteSpacesW: public std::wstring {
		CPathUnquoteSpacesW(const std::wstring& rwstrPath) {
			wchar_t* pBuffer = new wchar_t [rwstrPath.size() + 1];
			WSTRCPY_S(pBuffer, rwstrPath.size() + 1, rwstrPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			::PathUnquoteSpacesW(lpstrPath);
			assign(lpstrPath);
			delete[] pBuffer;
		}
	};
#ifdef _UNICODE
	typedef CPathUnquoteSpacesW CPathUnquoteSpaces;
#else
	typedef CPathUnquoteSpacesA CPathUnquoteSpaces;
#endif


	//! 引数に含まれる環境変数を %EvnVariables% を展開する。
	struct CExpandEnvironmentStringsW: public std::wstring {
		CExpandEnvironmentStringsW(const std::wstring& rstrPath) {
			const int iBufferLength = 1024 * 10;
			wchar_t* pBuffer = new wchar_t[iBufferLength];
			::ExpandEnvironmentStringsW(rstrPath.c_str(), pBuffer, iBufferLength);
			assign(pBuffer);
			delete[] pBuffer;
		}
	};
	struct CExpandEnvironmentStrings: public tstring {
		CExpandEnvironmentStrings(const tstring& rstrPath) {
			assign(_W2T(CExpandEnvironmentStringsW(_T2W(rstrPath))));
		}
	};
	struct CExpandEnvironmentStringsA: public std::string {
		CExpandEnvironmentStringsA(const tstring& rstrPath) {
			assign(_A(CExpandEnvironmentStringsW(_W(rstrPath))));
		}
	};
	//! パスを表す文字列の拡張子を新しい拡張子に置き換えます。元のパスに拡張子が含まれていない場合は、拡張子を付け加えます。
	//! 拡張子を置き換えた後の文字列が 260 (MAX_PATH) バイト（Unicode 版の場合は 260 (MAX_PATH) 文字）を超える場合は、 "" が割り当てられます。
	//! <http://msdn.microsoft.com/en-us/library/bb773749(VS.85).aspx>
	struct CPathRenameExtensionA: public std::string {
		CPathRenameExtensionA(const std::string& rstrPath, const std::string& rstrNewExt) {
			if (rstrPath.size() >= MAX_PATH) {
				return;
			}
			char* pBuffer = new char[MAX_PATH];
			STRCPY_S(pBuffer, MAX_PATH, rstrPath.c_str());
			if (::PathRenameExtensionA(pBuffer, rstrNewExt.c_str())) {
				assign(pBuffer);
			}
			delete[] pBuffer;
		}
	};
	struct CPathRenameExtensionW: public std::wstring {
		CPathRenameExtensionW(const std::wstring& rstrPath, const std::wstring& rstrNewExt) {
			if (rstrPath.size() >= MAX_PATH) {
				return;
			}
			wchar_t* pBuffer = new wchar_t[MAX_PATH];
			WSTRCPY_S(pBuffer, MAX_PATH, rstrPath.c_str());
			if (::PathRenameExtensionW(pBuffer, rstrNewExt.c_str())) {
				assign(pBuffer);
			}
			delete[] pBuffer;
		}
	};
#ifdef _UNICODE
	typedef CPathRenameExtensionW CPathRenameExtension;
#else
	typedef CPathRenameExtensionA CPathRenameExtension;
#endif

	//! <http://msdn.microsoft.com/ja-jp/library/cc428990.aspx>
	//! rstrDirPath 有効なパス名を指定する。パスの最後のコンポーネントがファイル名ではなくディレクトリである場合、文字列の最後に円記号（\）を記述しなければなりません。
	//! wstring 版の場合、相対パスを指定するとエラーでフォルダは生成されないので注意。
	static bool CMakeSureDirectoryPathExists(const std::string& rstrDirPath) {
		return !!::MakeSureDirectoryPathExists(rstrDirPath.c_str());
	}
	static bool CMakeSureDirectoryPathExists(const std::wstring& rwstrDirPath) {
		if (rwstrDirPath.at(rwstrDirPath.size()-1) == L'\\') {
			if (CPathIsDirectoryW(rwstrDirPath)) {
				return true;
			} else {
				return ERROR_SUCCESS == ::SHCreateDirectoryExW(NULL, rwstrDirPath.c_str(), NULL);
			}
		} else {
			if (CPathIsDirectoryW(CPathRemoveFileSpecW(rwstrDirPath))) {
				return true;
			} else {
				return ERROR_SUCCESS == ::SHCreateDirectoryExW(NULL, PathUtils::CPathRemoveFileSpecW(rwstrDirPath).c_str(), NULL);
			}
		}
	}

	//! 相対パスを絶対パスに
	struct CGetFullPathName: public tstring {
		CGetFullPathName(const tstring& rstrRelativePath) {
			TCHAR* pBuffer = new TCHAR[1024];
			::GetFullPathName(rstrRelativePath.c_str(), 1024, pBuffer, 0);
			assign(pBuffer);
			delete[] pBuffer;
		}
	};
	//! 相対パスを絶対パスに
	struct CGetFullPathNameA: public std::string {
		CGetFullPathNameA(const std::string& rstrRelativePath) {
			char* pBuffer = new char[1024];
			::GetFullPathNameA(rstrRelativePath.c_str(), 1024, pBuffer, 0);
			assign(pBuffer);
			delete[] pBuffer;
		}
	};
	//! 相対パスを絶対パスに
	struct CGetFullPathNameW: public std::wstring {
		CGetFullPathNameW(const std::wstring& rwstrRelativePath) {
			wchar_t* pBuffer = new wchar_t[1024];
			::GetFullPathNameW(rwstrRelativePath.c_str(), 1024, pBuffer, 0);
			assign(pBuffer);
			delete[] pBuffer;
		}
	};
	//! 指定ディレクトリにユニークで重複しない一時的なファイル名を生成しファイルを作成する。
	//! <http://msdn.microsoft.com/ja-jp/library/cc429354.aspx>
	struct CGetTempFileName: public tstring {
		CGetTempFileName(const tstring& rstrDirPath = _T("."), const tstring& rstrPrefix = _T("TMP"), bool bUniqueAndCreate = true) {
			TCHAR cBuffer[MAX_PATH];
			::GetTempFileName(rstrDirPath.c_str(), rstrPrefix.c_str(), bUniqueAndCreate ? 0 : 1, cBuffer);
			//printf("cBuffer = %s\n", cBuffer);
			assign(cBuffer);
		}
	};
	struct CGetTempFileNameW: public std::wstring {
		CGetTempFileNameW(const std::wstring& rstrDirPath = L".", const std::wstring& rstrPrefix = L"TMP", bool bUniqueAndCreate = true) {
			wchar_t cBuffer[MAX_PATH];
			::GetTempFileNameW(rstrDirPath.c_str(), rstrPrefix.c_str(), bUniqueAndCreate ? 0 : 1, cBuffer);
			//wprintf(L"cBuffer = %s\n", cBuffer);
			assign(cBuffer);
		}
	};
	struct CGetTempFileNameA: public std::string {
		CGetTempFileNameA(const std::string& rstrDirPath = ".", const std::string& rstrPrefix = "TMP", bool bUniqueAndCreate = true) {
			char cBuffer[MAX_PATH];
			::GetTempFileNameA(rstrDirPath.c_str(), rstrPrefix.c_str(), bUniqueAndCreate ? 0 : 1, cBuffer);
			//wprintf(L"cBuffer = %s\n", cBuffer);
			assign(cBuffer);
		}
	};
	//! 一時ファイル用のディレクトリのパスを取得します。
	//! この文字列の最後には円記号（\）が付きます.
	//! TMP と TEMP の各環境変数が指定しているディレクトリが存在しているかどうかをチェックしません。
	//! この関数は、次の順番で一時ファイル用のパスを決定します。
	//! 1.TMP 環境変数で指定されているパス
	//! 2.TMP 環境変数が定義されていない場合、TEMP 環境変数で指定されているパス
	//! 3.TMP と TEMP どちらの環境変数も定義されていない場合、Windows のディレクトリ（C:\Windows など）。
	//! <http://msdn.microsoft.com/ja-jp/library/cc429356.aspx>
	struct CGetTempPathA: public std::string {
		CGetTempPathA() {
			char cBuffer[MAX_PATH];
			::GetTempPathA(MAX_PATH, cBuffer);
			assign(cBuffer);
		}
	};
	struct CGetTempPathW: public std::wstring {
		CGetTempPathW() {
			wchar_t cBuffer[MAX_PATH];
			::GetTempPathW(MAX_PATH, cBuffer);
			assign(cBuffer);
		}
	};
#ifdef _UNICODE
	typedef CGetTempPathW CGetTempPath;
#else
	typedef CGetTempPathA CGetTempPath;
#endif

	//! 指定したパスを長いパスに変換します。長いパスが検出されない場合は、指定されたパスがそのまま返ります。
	//! <http://msdn.microsoft.com/ja-jp/library/cc429335.aspx>
	struct CGetLongPathName: public tstring {
		CGetLongPathName(const tstring& rstrShortPath) {
			TCHAR cLongPathName[1024];
			GetLongPathName(rstrShortPath.c_str(), cLongPathName, 1024);
			assign(cLongPathName);
		}
	};
	struct CGetLongPathNameA: public std::string {
		CGetLongPathNameA(const std::string& rstrShortPath) {
			char cLongPathName[1024];
			GetLongPathNameA(rstrShortPath.c_str(), cLongPathName, 1024);
			assign(cLongPathName);
		}
	};
	struct CGetLongPathNameW: public std::wstring {
		CGetLongPathNameW(const std::wstring& rstrShortPath) {
			wchar_t cLongPathName[1024];
			GetLongPathNameW(rstrShortPath.c_str(), cLongPathName, 1024);
			assign(cLongPathName);
		}
	};

	//! SHGetFolderPath のラッパ
	//! <http://msdn.microsoft.com/en-us/library/bb762181(v=vs.85).aspx>
	struct CSHGetFolderPathW: public std::wstring {
		CSHGetFolderPathW(DWORD dwCSIDL) {
#if _MSC_VER >= 1400 && _WIN32_IE >= 0x0500
			wchar_t szKnownFolder[MAX_PATH];
			if ( SUCCEEDED( SHGetFolderPathW( NULL, dwCSIDL, NULL, 0, szKnownFolder ) )) {
				assign(szKnownFolder);
			}
#endif
		}
	};
	struct CSHGetFolderPath: public tstring {
		CSHGetFolderPath(DWORD dwCSIDL) {
			assign(_W2T(CSHGetFolderPathW(dwCSIDL)));
		}
	};
	struct CSHGetFolderPathA: public std::string {
		CSHGetFolderPathA(DWORD dwCSIDL) {
			assign(_A(CSHGetFolderPathW(dwCSIDL)));
		}
	};

	//! 環境変数と CSIDL 番号のついたフォルダ名を解決する。
	//! 環境変数は %ProgramFiles% のように "%" で囲む
	//! CSIDL は <CSIDL_COMMON_APPDATA> のように "<" と ">" で囲んで指定する。末尾に backslash はつかないようである。すべての CSIDL をサポートしているわけではない.必要なものは使用する際に追加すること
	//! 将来 SHGetKnownFolderPath に置き換えることを検討すること.
	struct CSolveKnownFoldersW: public std::wstring {
		CSolveKnownFoldersW(const std::wstring& rstrPath) {
			std::wstring strResult = CExpandEnvironmentStringsW(rstrPath);
#if _MSC_VER >= 1400
			size_t sizeOpen  = strResult.find_first_of(L"<");
			while (sizeOpen != npos) {
				size_t sizeClose = strResult.find_first_of(L">");
				//TRACE_HERE("sizeOpen  = %d\n", sizeOpen);
				//TRACE_HERE("sizeClose = %d\n", sizeClose);
				std::wstring strCSIDLName = strResult.substr(sizeOpen+1, sizeClose- sizeOpen-1);
				//TRACE_HERE("strCSIDLName = %s\n", strCSIDLName.c_str());
				DWORD dwCSIDL;
				// Get path for each computer, non-user specific and non-roaming data.
				if (false) {}
				else if (strCSIDLName == L"CSIDL_COMMON_APPDATA"            ) { dwCSIDL = CSIDL_COMMON_APPDATA            ;}
				else if (strCSIDLName == L"CSIDL_LOCAL_APPDATA"             ) { dwCSIDL = CSIDL_LOCAL_APPDATA             ;}
				else if (strCSIDLName == L"CSIDL_APPDATA"                   ) { dwCSIDL = CSIDL_APPDATA                   ;}
//				else if (strCSIDLName == L"CSIDL_PROGRAM_FILESX64"          ) { dwCSIDL = CSIDL_PROGRAM_FILESX64          ;}
				else if (strCSIDLName == L"CSIDL_PROGRAM_FILESX86"          ) { dwCSIDL = CSIDL_PROGRAM_FILESX86          ;}
				else if (strCSIDLName == L"CSIDL_PROGRAM_FILES"             ) { dwCSIDL = CSIDL_PROGRAM_FILES             ;}
				else if (strCSIDLName == L"CSIDL_PROGRAM_FILES_COMMON"      ) { dwCSIDL = CSIDL_PROGRAM_FILES_COMMON      ;}
//				else if (strCSIDLName == L"CSIDL_PROGRAM_FILES_COMMONX64"   ) { dwCSIDL = CSIDL_PROGRAM_FILES_COMMONX64   ;}
				else if (strCSIDLName == L"CSIDL_PROGRAM_FILES_COMMONX86"   ) { dwCSIDL = CSIDL_PROGRAM_FILES_COMMONX86   ;}
				else if (strCSIDLName == L"CSIDL_PROFILE"                   ) { dwCSIDL = CSIDL_PROFILE                   ;}
				else {assign(strResult);return;}
				//TRACE_HERE("dwCSIDL = 0X%X\n", dwCSIDL);
				std::wstring strKnownFolder = CSHGetFolderPathW(dwCSIDL);	// SHGetKnownFolderPath is supported by Vista and above.
				if ( strKnownFolder.size() ) {
					//TRACE_HERE("szKnownFolder = %s\n", szKnownFolder.c_str());
					strResult.replace(sizeOpen, sizeClose- sizeOpen+1, strKnownFolder.c_str());
				}
				//TRACE_HERE("strResult = %s\n", strResult.c_str());
				sizeOpen  = strResult.find_first_of(L"<");
			}
#endif
			assign(strResult);
		}
	};
	struct CSolveKnownFolders: public tstring {
		CSolveKnownFolders (const tstring& rstrPath) {
			assign(_W2T(CSolveKnownFoldersW(_T2W(rstrPath))));
		}
	};
	struct CSolveKnownFoldersA: public std::string {
		CSolveKnownFoldersA (const std::string& rstrPath) {
			assign(_A(CSolveKnownFoldersW(_W(rstrPath))));
		}
	};

	//! ファイルサイズを返す。
	//! エラーが起こったら 0 を返す。
	static LONGLONG CGetFileSize(const std::string& rstrFilePath) {
		HANDLE hFile = CreateFileA(rstrFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			return 0;
		}
#if _MSC_VER >= 1400
		LARGE_INTEGER largeIntegerSize;
		if (GetFileSizeEx(hFile, &largeIntegerSize) == 0) {
			CloseHandle(hFile);
			return 0;
		}
		CloseHandle(hFile);
		return largeIntegerSize.QuadPart;
#else
		DWORD dwHigh = 0;
		DWORD dwLow  = GetFileSize(hFile, &dwHigh);
		if (dwLow == -1 && ::GetLastError() != NO_ERROR) {
			CloseHandle(hFile);
			return 0;
		}
		CloseHandle(hFile);
		LONGLONG llSize = dwHigh;
		llSize = (llSize << 32) | dwLow;
		return llSize;
#endif
	};
	
	static LONGLONG CGetFileSize(const std::wstring& rwstrFilePath) {
		HANDLE hFile = CreateFileW(rwstrFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (hFile == INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			return 0;
		}
#if _MSC_VER >= 1400
		LARGE_INTEGER largeIntegerSize;
		if (GetFileSizeEx(hFile, &largeIntegerSize) == 0) {
			CloseHandle(hFile);
			return 0;
		}
		CloseHandle(hFile);
		return largeIntegerSize.QuadPart;
#else
		DWORD dwHigh = 0;
		DWORD dwLow  = GetFileSize(hFile, &dwHigh);
		if (dwLow == -1 && ::GetLastError() != NO_ERROR) {
			CloseHandle(hFile);
			return 0;
		}
		CloseHandle(hFile);
		LONGLONG llSize = dwHigh;
		llSize = (llSize << 32) | dwLow;
		return llSize;
#endif
	};
	static LONGLONG CGetFileSizeW(const std::wstring& rwstrFilePath) {
		return CGetFileSize(rwstrFilePath);
	}
	static LONGLONG CGetFileSizeA(const std::string& rstrFilePath) {
		return CGetFileSize(rstrFilePath);
	}

	// 指定されたモジュールを含む実行ファイルの、フルパス名とファイル名を取得します
	// <http://msdn.microsoft.com/ja-jp/library/cc429127.aspx>
	struct CGetModuleFileNameW : public std::wstring {
		CGetModuleFileNameW (HMODULE hModule) {
			wchar_t strFilePath[_MAX_PATH];
			DWORD Ret = GetModuleFileNameW(hModule, strFilePath, _MAX_PATH);
			assign(strFilePath);
		}
	};	
	struct CGetModuleFileNameA : public std::string {
		CGetModuleFileNameA (HMODULE hModule) {
			char strFilePath[_MAX_PATH];
			DWORD Ret = GetModuleFileNameA(hModule, strFilePath, _MAX_PATH);
			assign(strFilePath);
		}
	};	
#ifdef _UNICODE
	typedef CGetModuleFileNameW CGetModuleFileName;
#else
	typedef CGetModuleFileNameA CGetModuleFileName;
#endif

	// 自モジュールのディレクトリを得る。
	struct CGetModuleDirectoryA : public std::string {
		CGetModuleDirectoryA() {
			char cPath[MAX_PATH+1];
			if (GetModuleFileNameA( NULL, cPath, MAX_PATH )) {
				std::string strModuleFilePath = cPath;
				assign(PathUtils::CPathRemoveFileSpecA(strModuleFilePath));
			}
		}
	};
	struct CGetModuleDirectoryW : public std::wstring {
		CGetModuleDirectoryW() {
			wchar_t cPath[MAX_PATH+1];
			if (GetModuleFileNameW( NULL, cPath, MAX_PATH )) {
				std::wstring strModuleFilePath = cPath;
				assign(PathUtils::CPathRemoveFileSpecW(strModuleFilePath));
			}
		}
	};
#ifdef _UNICODE
	typedef CGetModuleDirectoryW CGetModuleDirectory;
#else
	typedef CGetModuleDirectoryA CGetModuleDirectory;
#endif


	struct CGetSystemDirectoryW : public std::wstring {
		CGetSystemDirectoryW() {
			wchar_t cPath[MAX_PATH+1];
			if (GetSystemDirectoryW( cPath, MAX_PATH )) {
				std::wstring strPath = cPath;
				assign(strPath);
			}
		}
	};
	struct CGetSystemDirectoryA : public std::string {
		CGetSystemDirectoryA() {
			char cPath[MAX_PATH+1];
			if (GetSystemDirectoryA( cPath, MAX_PATH )) {
				std::string strPath = cPath;
				assign(strPath);
			}
		}
	};
#ifdef _UNICODE
	typedef CGetSystemDirectoryW CGetSystemDirectory;
#else
	typedef CGetSystemDirectoryA CGetSystemDirectory;
#endif


	//! ファイルが存在しない場合は、空のファイルを生成し、その成否を返す。
	//! ファイルが存在する場合は何もせず true を返す。
	inline bool CMakeFileExist (const std::string& rstrPathFile) {
		if (CPathFileExistsA(rstrPathFile)) {
			return true;
		}
		CMakeSureDirectoryPathExists(rstrPathFile);
		FILE* pfile = NULL;
		if (fopen_s(&pfile, rstrPathFile.c_str(), "a+") == 0 && pfile) {
			fclose(pfile);
			return true;
		} else {
			return false;
		}
	}
	//! ファイルが存在しない場合は、空のファイルを生成し、その成否を返す。
	//! ファイルが存在する場合は何もせず true を返す。
	inline bool CMakeFileExist (const std::wstring& rstrPathFile) {
		if (CPathFileExistsW(rstrPathFile)) {
			return true;
		}
		CMakeSureDirectoryPathExists(rstrPathFile);
		FILE* pfile = NULL;
		if (_wfopen_s(&pfile, rstrPathFile.c_str(), L"a+") == 0 && pfile) {
			fclose(pfile);
			return true;
		} else {
			return false;
		}
	}

	//! ローカルコンピュータの NetBIOS 名を取得します。
	//! この名前はシステムの起動時に確立されるもので、システムはレジストリからこの名前を読み取ります。
	struct CGetComputerNameW : public std::wstring {
		CGetComputerNameW() {
			wchar_t cPath[MAX_PATH+1];
			DWORD dwSize = sizeof(cPath)/sizeof(cPath[0]);
			if (GetComputerNameW( cPath, &dwSize )) {
				std::wstring strPath = cPath;
				assign(strPath);
			}
		}
	};
	//! ローカルコンピュータの NetBIOS 名を取得します。
	//! この名前はシステムの起動時に確立されるもので、システムはレジストリからこの名前を読み取ります。
	struct CGetComputerNameA : public std::string {
		CGetComputerNameA() {
			char cPath[MAX_PATH+1];
			DWORD dwSize = sizeof(cPath)/sizeof(cPath[0]);
			if (GetComputerNameA( cPath, &dwSize )) {
				std::string strPath = cPath;
				assign(strPath);
			}
		}
	};
#ifdef _UNICODE
	typedef CGetComputerNameW CGetComputerName;
#else
	typedef CGetComputerNameA CGetComputerName;
#endif

	//! パスのルート部分を抽出する。
	//! c:\temp\ => c:   \\?\c:\temp\ => \\?\c:\,   \\?\UNC\nsa003\devdoc\MCT-1F17\ => \\?\UNC\nsa003\devdoc, \\nsa003\devdoc\ => \\nsa003\devdoc
	struct CPathStripToRootW: public std::wstring  {
		CPathStripToRootW(const std::wstring& rwstrDirectoryPath) {
			wchar_t pBuffer[MAX_PATH];
			WSTRCPY_S(pBuffer, MAX_PATH, rwstrDirectoryPath.c_str());
			LPWSTR lpstrPath = pBuffer;
			::PathStripToRootW(lpstrPath);
			assign(lpstrPath);
		}
	};
	//! パスのルート部分を抽出する。
	//! c:\temp\ => c:   \\?\c:\temp\ => \\?\c:\,   \\?\UNC\nsa003\devdoc\MCT-1F17\ => \\?\UNC\nsa003\devdoc, \\nsa003\devdoc\ => \\nsa003\devdoc
	struct CPathStripToRootA: public std::string  {
		CPathStripToRootA(const std::string& rstrDirectoryPath) {
			char pBuffer[MAX_PATH];
			STRCPY_S(pBuffer, MAX_PATH, rstrDirectoryPath.c_str());
			LPSTR lpstrPath = pBuffer;
			::PathStripToRootA(lpstrPath);
			assign(lpstrPath);
		}
	};
#ifdef _UNICODE
	typedef CPathStripToRootW CPathStripToRoot;
#else
	typedef CPathStripToRootA CPathStripToRoot;
#endif

	//! プロセスのカレントディレクトリを返す。
	//! 末尾に \\ は含まない.
	struct CGetCurrentDirectoryA: public std::string {
		CGetCurrentDirectoryA() {
			char pBuffer[MAX_PATH]={0};
			::GetCurrentDirectoryA (MAX_PATH, pBuffer);
			assign(pBuffer);
		}
	};
	//! プロセスのカレントディレクトリを返す。
	//! 末尾に \\ は含まない.
	struct CGetCurrentDirectoryW: public std::wstring {
		CGetCurrentDirectoryW() {
			wchar_t pBuffer[MAX_PATH]={0};
			::GetCurrentDirectoryW (MAX_PATH, pBuffer);
			assign(pBuffer);
		}
	};
#ifdef _UNICODE
	typedef CGetCurrentDirectoryW CGetCurrentDirectory;
#else
	typedef CGetCurrentDirectoryA CGetCurrentDirectory;
#endif


	// SHFileOperation の good practice.
	// SHFileOperation は成功で 0 を返すが、これにはユーザーによるキャンセルが含まれる.
	// この関数 IsSHFileOperationAccomplished は SHFileOperation を実行して、完遂した場合のみ true を返し、それ以外の場合には false を返す。
	static bool IsSHFileOperationAccomplished(LPSHFILEOPSTRUCTW lpFileOp, int* piErrorCode = 0) {
		bool bRet = true;
		int iResult = SHFileOperationW(lpFileOp);
		if (iResult != 0) {
			bRet = false;
		} else {
			if (lpFileOp->fAnyOperationsAborted) {
				bRet = false;
			} else {
				bRet = true;
			}
		}
		if (piErrorCode) {
			*piErrorCode = iResult;
		}
		return bRet;
	}
	static bool IsSHFileOperationAccomplished(LPSHFILEOPSTRUCTA lpFileOp, int* piErrorCode = 0) {
		bool bRet = true;
		int iResult = SHFileOperationA(lpFileOp);
		if (iResult != 0) {
			bRet = false;
		} else {
			if (lpFileOp->fAnyOperationsAborted) {
				bRet = false;
			} else {
				bRet = true;
			}
		}
		if (piErrorCode) {
			*piErrorCode = iResult;
		}
		return bRet;
	}

	//! @brief 与えられた vector<string> をパス要素として結合する。
	//! @brief セパレータは '\\' で固定. 入力に '/' が含まれていても置換しない.
	//! @note string strS = PathUtils::Join(vector<string>{"c:\\AAA", "BB", "\\CC", "DD\\", "\\EE\\", "\\\\", "moga.csv"});
	//! @note => "c:\AAA\BB\CC\DD\EE\moga.csv"
	//! @note string strS2 = PathUtils::Join(vector<string>{"c:\\AAA", "BB", "\\CC", "DD\\", "\\EE\\"});
	//! @note => "c:\AAA\BB\CC\DD\EE\"
	//! @note string strS5 = PathUtils::Join(vector<string>{"abc", "def/ghi"});
	//! @note => "abc\def/ghi"
	static std::string Join(const std::vector<std::string>& rvecstrIn) {
		if (rvecstrIn.size() == 0) {
			return "";
		}
		if (rvecstrIn.size() == 1) {
			return rvecstrIn.front();
		}
		std::string strOut = PathUtils::CPathAddBackslashA(rvecstrIn.front());
		for (std::vector<std::string>::const_iterator cit = rvecstrIn.begin() + 1;
			cit != rvecstrIn.end();
			++cit)
		{
			std::string strItem = _removeLeadingSeparator(*cit);
			if (strItem.size() == 0) {
				continue;
			}
			strOut = PathUtils::CPathAddBackslashA(strOut) + strItem;
		}
		return strOut;
	}

	//! @brief 与えられた vector<wstring> をパス要素として結合する。
	//! @brief セパレータは '\\' で固定. 入力に '/' が含まれていても置換しない.
	//! @note 例は Join(std::vector<std::string>&) を参照.
	static std::wstring Join(const std::vector<std::wstring>& rvecstrIn) {
		if (rvecstrIn.size() == 0) {
			return L"";
		}
		if (rvecstrIn.size() == 1) {
			return rvecstrIn.front();
		}
		std::wstring strOut = PathUtils::CPathAddBackslashW(rvecstrIn.front());
		for (std::vector<std::wstring>::const_iterator cit = rvecstrIn.begin() + 1;
			cit != rvecstrIn.end();
			++cit)
		{
			std::wstring strItem = _removeLeadingSeparator(*cit);
			if (strItem.size() == 0) {
				continue;
			}
			strOut = PathUtils::CPathAddBackslashW(strOut) + strItem;
		}
		return strOut;
	}

	//! @brief 可変長引数で与えられた string たちをパス要素として結合する。
	//! @brief セパレータは '\\' で固定. 入力に '/' が含まれていても置換しない.
	//! @note string strS = PathUtils::Join("c:\\AAA", "BB", "\\CC", "DD\\", "\\EE\\", "\\\\", "moga.csv");
	//! @note => "c:\AAA\BB\CC\DD\EE\moga.csv"
	//! @note string strS2 = PathUtils::Join("c:\\AAA", "BB", "\\CC", "DD\\", "\\EE\\");
	//! @note => "c:\AAA\BB\CC\DD\EE\"
	//! @note string strS5 = PathUtils::Join("abc", "def/ghi");
	//! @note => "abc\def/ghi"
	static std::string Join(const std::string& rstrLast) { return _removeLeadingSeparator(rstrLast); };
	template <typename... T_Rest>
#if _MSC_VER >= 1930
	static std::string Join(const std::string& rstrFirst, T_Rest&&... rvastrRest) {
#else
	static std::string Join(const std::string & rstrFirst, T_Rest&... rvastrRest) {
#endif
		// Memo: rvastrRest の要素数は sizeof...(rstrRest) で取れる.
		const std::string strFirst = _removeLeadingSeparator(rstrFirst);
		return PathUtils::CPathAddBackslashA(strFirst) + Join(rvastrRest...);
	}

	//! @brief 可変長引数で与えられた wstring たちをパス要素として結合する。
	//! @brief セパレータは '\\' で固定. 入力に '/' が含まれていても置換しない.
	//! @note 例は Join(const std::string&, T_Rest&) を参照.
	static std::wstring Join(const std::wstring& rstrLast) { return _removeLeadingSeparator(rstrLast); };
	template <typename... T_Rest>
#if _MSC_VER >= 1930
	static std::wstring Join(const std::wstring& rstrFirst, T_Rest&&... rstrRest) {
#else
	static std::wstring Join(const std::wstring& rstrFirst, T_Rest&... rstrRest) {
#endif
		const std::wstring strFirst = _removeLeadingSeparator(rstrFirst);
		return PathUtils::CPathAddBackslashW(strFirst) + Join(rstrRest...);
	}

	//! 文字列の先頭の "\", "/" を取り除く補助関数
	static std::string _removeLeadingSeparator(const std::string& rstrIn) {
		std::string strOut = rstrIn;
		// 全部 separator なら "" を返す
		bool bAllSlash = true;
		for (int iI = 0; iI < strOut.size(); ++iI) {
			if (strOut[iI] != '\\' && strOut[iI] != '/') {
				bAllSlash = false;
				break;
			}
		}
		if (bAllSlash) {
			return "";
		}
		// UNC path. \\[^\\]+
		if (strOut.find("\\\\\\") == std::string::npos && strOut.find("\\\\") == 0) {
			return strOut;
		}
		// 先頭の "\", "/" を取り除く
		while (strOut.size() > 0 && (strOut[0] == '\\' || strOut[0] == '/')) {
			strOut = strOut.substr(1);
		}
		return strOut;
	}

	static std::wstring _removeLeadingSeparator(const std::wstring& rstrIn) {
		std::wstring strOut = rstrIn;
		// 全部 separator なら "" を返す
		bool bAllSlash = true;
		for (int iI = 0; iI < strOut.size(); ++iI) {
			if (strOut[iI] != L'\\' && strOut[iI] != L'/') {
				bAllSlash = false;
				break;
			}
		}
		if (bAllSlash) {
			return L"";
		}
		// UNC path. \\[^\\]+
		if (strOut.find(L"\\\\\\") == std::string::npos && strOut.find(L"\\\\") == 0) {
			return strOut;
		}
		// 先頭の "\", "/" を取り除く
		while (strOut.size() > 0 && (strOut[0] == L'\\' || strOut[0] == L'/')) {
			strOut = strOut.substr(1);
		}
		return strOut;
	}
};

#endif // if !defined(_WIN32)
#endif // #ifndef __SHLWAPIWRAPPER__
