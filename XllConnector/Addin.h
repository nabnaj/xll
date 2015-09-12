////////////////////////////////////////////////////////////////////////////
// Addin.h -- defines macros to export functions to Excel

#pragma once

#include "xlldef.h"
#include "FunctionInfo.h"
#include "Conversion.h"
#include <cstdint>

XLL_BEGIN_NAMEPSACE

FunctionInfoBuilder AddFunction(FunctionInfo &f);

#define EXPORT_UNDECORATED_NAME comment(linker, "/export:" __FUNCTION__ "=" __FUNCDNAME__)

#if 0
template <typename T> inline const wchar_t * GetTypeText()
{
	static_assert(false, "The supplied type is not a valid XLL argument type.");
}
#define DEFINE_TYPE_TEXT(type, text) \
template<> inline const wchar_t * GetTypeText<type>() { return L##text; }

DEFINE_TYPE_TEXT(bool, "A");
DEFINE_TYPE_TEXT(bool*, "L");
DEFINE_TYPE_TEXT(double, "B");
DEFINE_TYPE_TEXT(double*, "E");
DEFINE_TYPE_TEXT(char*, "C");
DEFINE_TYPE_TEXT(const char*, "C");
DEFINE_TYPE_TEXT(uint16_t, "H");
DEFINE_TYPE_TEXT(int16_t, "I");
DEFINE_TYPE_TEXT(int16_t*, "M");
DEFINE_TYPE_TEXT(int32_t, "J");
DEFINE_TYPE_TEXT(int32_t*, "N");
DEFINE_TYPE_TEXT(wchar_t*, "C%");
DEFINE_TYPE_TEXT(const wchar_t*, "C%");
DEFINE_TYPE_TEXT(LPXLOPER12, "Q");
#endif

inline LPXLOPER12 getReturnValue()
{
#if XLL_SUPPORT_THREAD_LOCAL
	__declspec(thread) extern XLOPER12 xllReturnValue;
	return &xllReturnValue;
#else
	LPXLOPER12 p = (LPXLOPER12)malloc(sizeof(XLOPER12));
	if (p == nullptr)
		throw std::bad_alloc();
	return p;
#endif
}

template <typename Func> struct StripCallingConvention;
template <typename TRet, typename... TArgs>
struct StripCallingConvention < TRet __cdecl(TArgs...) >
{
	typedef TRet type(TArgs...);
};
#ifndef _WIN64
template <typename TRet, typename... TArgs>
struct StripCallingConvention < TRet __stdcall(TArgs...) >
{
	typedef TRet type(TArgs...);
};
template <typename TRet, typename... TArgs>
struct StripCallingConvention < TRet __fastcall(TArgs...) >
{
	typedef TRet type(TArgs...);
};
#else
template <typename TRet, typename... TArgs>
struct StripCallingConvention < TRet __vectorcall(TArgs...) >
{
	typedef TRet type(TArgs...);
};
#endif

template <typename Func, Func *func, typename TRet, typename... TArgs>
inline LPXLOPER12 XLWrapperImpl(typename ArgumentMarshaler<TArgs>::WireType... args)
{
	try
	{
		LPXLOPER12 pvRetVal = getReturnValue();
		*pvRetVal = make<XLOPER12>(func(ArgumentMarshaler<TArgs>::Marshal(args)...));
		return pvRetVal;
	}
	catch (const std::exception &)
	{
		// todo: report exception
	}
	catch (...)
	{
		// todo: report exception
	}
	return const_cast<ExcelVariant*>(&ExcelVariant::ErrValue);
}

template <typename Func, Func *func, typename = typename StripCallingConvention<Func>::type>
struct XLWrapper;

XLL_END_NAMESPACE

#define EXPORT_XLL_FUNCTION(f) \
	namespace XLL_NAMESPACE { \
		template <typename TRet, typename... TArgs> \
		struct XLWrapper < decltype(f), f, TRet(TArgs...) > \
		{ \
			static LPXLOPER12 __stdcall Call(typename ArgumentMarshaler<TArgs>::WireType... args) \
			{ \
				__pragma(comment(linker, "/export:" "XL" #f "=" __FUNCDNAME__)) \
				return XLWrapperImpl<decltype(f), f, TRet, TArgs...>(args...); \
			} \
		}; \
		static auto XLWrapper_Call_##f = XLWrapper<decltype(f), f>::Call; \
	} \
	static ::XLL_NAMESPACE::FunctionInfoBuilder XLFun_##f = ::XLL_NAMESPACE::AddFunction( \
		::XLL_NAMESPACE::FunctionInfoFactory<decltype(f)>::Create(L"XL" L#f)).Name(L#f)