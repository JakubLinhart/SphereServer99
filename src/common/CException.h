#ifndef _CEXCEPTION_H
#define _CEXCEPTION_H

#include "../sphere/threads.h"
#include "../graysvr/CLog.h"

extern "C"
{
	extern void globalstartsymbol();
	extern void globalendsymbol();
	extern const int globalstartdata;
	extern const int globalenddata;
}

#ifdef _WIN32
	#if !defined(__MINGW32__) && !defined(_DEBUG)
		void SetExceptionTranslator();
	#endif
#else
	void SetSignals(bool bSet);
#endif

///////////////////////////////////////////////////////////

class CGrayError
{
	// Throw this structure to produce an error
public:
	CGrayError(LOGL_TYPE eSev, DWORD hErr, LPCTSTR pszDescription);
	CGrayError(const CGrayError &e);	// copy constructor needed
	virtual ~CGrayError() { };

public:
	LOGL_TYPE m_eSeverity;	// const
	DWORD m_hError;			// HRESULT S_OK, "winerror.h" code (0x20000000 = start of custom codes)
	LPCTSTR m_pszDescription;

public:
#ifdef _WIN32
	static int GetSystemErrorMessage(DWORD dwError, LPTSTR lpszError, DWORD dwMaxError);
#endif
	virtual bool GetErrorMessage(LPTSTR lpszError) const;

public:
	CGrayError &operator=(const CGrayError &other);
};

class CGrayAssert : public CGrayError
{
public:
	static const char *m_sClassName;

	CGrayAssert(LOGL_TYPE eSeverity, LPCTSTR pszExp, LPCTSTR pszFile, long lLine);
	virtual ~CGrayAssert() { }; 

protected:
	LPCTSTR const m_pszExp;
	LPCTSTR const m_pszFile;
	const long m_lLine;

public:
	virtual bool GetErrorMessage(LPTSTR lpszError) const;
	//LPCTSTR const GetAssertFile();
	//const unsigned GetAssertLine();

private:
	CGrayAssert &operator=(const CGrayAssert &other);
};

#ifdef _WIN32

	// Catch and get details on the system exceptions.
class CGrayException : public CGrayError
{
public:
	static const char *m_sClassName;

	CGrayException(unsigned int uCode, DWORD dwAddress);
	virtual ~CGrayException() { };

public:
	const DWORD m_dwAddress;

public:
	virtual bool GetErrorMessage(LPTSTR lpszError) const;

private:
	CGrayException &operator=(const CGrayException &other);
};

#endif

///////////////////////////////////////////////////////////

// Exceptions debugging routine
#ifdef EXCEPTIONS_DEBUG

	#define EXC_TRY(a)	LPCTSTR inLocalBlock = ""; \
						LPCTSTR inLocalArgs = a; \
						unsigned int inLocalBlockCnt = 0; \
						bool bCATCHExcept = false; \
						UNREFERENCED_PARAMETER(bCATCHExcept); \
						try {

	#define EXC_SET(a)	inLocalBlock = a; inLocalBlockCnt++

	#ifdef THREAD_TRACK_CALLSTACK
		#define EXC_CATCH_EXCEPTION(a)	bCATCHExcept = true; \
										StackDebugInformation::printStackTrace(); \
										if ( (inLocalBlock != NULL) && (inLocalBlockCnt > 0) ) \
											g_Log.CatchEvent(a, "%s::%s() #%u \"%s\"", m_sClassName, inLocalArgs, inLocalBlockCnt, inLocalBlock); \
										else \
											g_Log.CatchEvent(a, "%s::%s()", m_sClassName, inLocalArgs)
	#else
		#define EXC_CATCH_EXCEPTION(a)	bCATCHExcept = true; \
										if ( (inLocalBlock != NULL) && (inLocalBlockCnt > 0) ) \
											g_Log.CatchEvent(a, "%s::%s() #%u \"%s\"", m_sClassName, inLocalArgs, inLocalBlockCnt, inLocalBlock); \
										else \
											g_Log.CatchEvent(a, "%s::%s()", m_sClassName, inLocalArgs)
	#endif

	#define EXC_CATCH	} \
						catch ( const CGrayError &e )	{ EXC_CATCH_EXCEPTION(&e); CurrentProfileData.Count(PROFILE_STAT_FAULTS, 1); } \
						catch ( ... )	{ EXC_CATCH_EXCEPTION(NULL); CurrentProfileData.Count(PROFILE_STAT_FAULTS, 1); }

	#define EXC_DEBUG_START	if ( bCATCHExcept ) { try {

	#define EXC_DEBUG_END	/*StackDebugInformation::printStackTrace();*/ \
							} catch ( ... ) { g_Log.EventError("Exception adding debug message on the exception.\n"); CurrentProfileData.Count(PROFILE_STAT_FAULTS, 1); } }

	#define EXC_TRYSUB(a)	LPCTSTR inLocalSubBlock = ""; \
							LPCTSTR inLocalSubArgs = a; \
							unsigned int inLocalSubBlockCnt = 0; \
							bool bCATCHExceptSub = false; \
							UNREFERENCED_PARAMETER(bCATCHExceptSub); \
							try {

	#define EXC_SETSUB(a)	inLocalSubBlock = a; inLocalSubBlockCnt++

	#ifdef THREAD_TRACK_CALLSTACK
		#define EXC_CATCH_SUB(a, b)	bCATCHExceptSub = true; \
									StackDebugInformation::printStackTrace(); \
									if ( (inLocalSubBlock != NULL) && (inLocalSubBlockCnt > 0) ) \
										g_Log.CatchEvent(a, "SUB: %s::%s::%s() #%u \"%s\"", m_sClassName, b, inLocalSubArgs, inLocalSubBlockCnt, inLocalSubBlock); \
									else \
										g_Log.CatchEvent(a, "SUB: %s::%s::%s()", m_sClassName, b, inLocalSubArgs)
	#else
		#define EXC_CATCH_SUB(a, b)	bCATCHExceptSub = true; \
									if ( (inLocalSubBlock != NULL) && (inLocalSubBlockCnt > 0) ) \
										g_Log.CatchEvent(a, "SUB: %s::%s::%s() #%u \"%s\"", m_sClassName, b, inLocalSubArgs, inLocalSubBlockCnt, inLocalSubBlock); \
									else \
										g_Log.CatchEvent(a, "SUB: %s::%s::%s()", m_sClassName, b, inLocalSubArgs)
	#endif

	#define EXC_CATCHSUB(a)	} \
							catch ( const CGrayError &e ) \
							{ \
								EXC_CATCH_SUB(&e, a); \
								CurrentProfileData.Count(PROFILE_STAT_FAULTS, 1); \
							} \
							catch ( ... ) \
							{ \
								EXC_CATCH_SUB(NULL, a); \
								CurrentProfileData.Count(PROFILE_STAT_FAULTS, 1); \
							}

	#define EXC_DEBUGSUB_START	if ( bCATCHExceptSub ) { try {
	
	#define EXC_DEBUGSUB_END	/*StackDebugInformation::printStackTrace();*/ \
								} catch ( ... ) { g_Log.EventError("Exception adding debug message on the exception.\n"); CurrentProfileData.Count(PROFILE_STAT_FAULTS, 1); }}

	#define EXC_ADD_SCRIPT		g_Log.EventDebug("command '%s' args '%s'\n", s.GetKey(), s.GetArgRaw());
	#define EXC_ADD_SCRIPTSRC	g_Log.EventDebug("command '%s' args '%s' [%p]\n", s.GetKey(), s.GetArgRaw(), static_cast<void *>(pSrc));
	#define EXC_ADD_KEYRET(src)	g_Log.EventDebug("command '%s' ret '%s' [%p]\n", pszKey, (LPCTSTR)sVal, static_cast<void *>(src));

#else

	#define EXC_TRY(a) {
	#define EXC_SET(a)
	#define EXC_SETSUB(a)
	#define EXC_CATCH }
	#define EXC_TRYSUB(a) {
	#define EXC_CATCHSUB(a) }
	
	#define EXC_DEBUG_START {
	#define EXC_DEBUG_END }
	#define EXC_DEBUGSUB_START {
	#define EXC_DEBUGSUB_END }
	#define EXC_ADD_SCRIPT
	#define EXC_ADD_SCRIPTSRC
	#define EXC_ADD_KEYRET(a)

#endif

#endif	// _CEXCEPTION_H
