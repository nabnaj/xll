////////////////////////////////////////////////////////////////////////////
// Wrapper.h -- type-safe wrapper to expose user-defined functions to Excel

#pragma once

#include "xlldef.h"
#include "FunctionInfo.h"
#include "Conversion.h"
#include "Marshal.h"

namespace XLL_NAMESPACE
{
	//
	// StripCallingConvention<Func>
	//
	// Removes the calling convention from a function type.
	//                

	template <typename Func, typename = void> struct StripCallingConvention;

	template <int> struct Placeholder;

	template <typename TRet, typename... TArgs>
	struct StripCallingConvention < TRet(TArgs...), void >
	{
		typedef TRet type(TArgs...);
	};

#define XLL_DEFINE_STRIP_CC(n, cc) \
	template <typename TRet, typename... TArgs> \
	struct StripCallingConvention <TRet cc(TArgs...), std::conditional_t< \
		std::is_same< TRet cc(TArgs...), TRet(TArgs...)>::value, \
		Placeholder<n>, void > > \
		{ \
		typedef TRet type(TArgs...); \
		}

	XLL_DEFINE_STRIP_CC(0, __cdecl);
	XLL_DEFINE_STRIP_CC(1, __stdcall);
	XLL_DEFINE_STRIP_CC(2, __fastcall);
	XLL_DEFINE_STRIP_CC(3, __vectorcall);

	template <typename Func>
	using strip_cc_t = typename StripCallingConvention<Func>::type;
}

namespace XLL_NAMESPACE
{
	//
	// getReturnValue
	//
	// Returns a pointer an XLOPER12 that holds the return value of a
	// wrapper function. Because the code that fills the return value
	// is guaranteed never to be called recursively, we allocate the
	// return value in thread-local storage (TLS) where supported. 
	//
	// TLS is properly supported starting from WIndows Vista. On
	// earlier platforms, we allocate the return value on the heap.
	//

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
}

namespace XLL_NAMESPACE
{
	//
	// XLSimpleWrapper<Func, func>
	//
	// Simple UDF wrapper that 
	//   1) takes care of argument and return value marshalling, and
	//   2) provide a __stdcall entry point for Excel to call.
	//
	// The entry point is exported by decorated name. If you don't want
	// your internal name to appear as part of the decoration, simply
	// don't export your name (then it won't go into the signature).
	// 

	// TODO: Find some way to have one less template parameter (to make
	// export table prettier. Might need to use tuples.
	template <typename Func, Func *func, typename = strip_cc_t<Func> >
	struct XLWrapper;

	template <typename Func, Func *func, typename TRet, typename... TArgs>
	struct XLWrapper < Func, func, TRet(TArgs...) >
	{
		static __declspec(dllexport) LPXLOPER12 __stdcall EntryPoint(
			typename ArgumentMarshaler<TArgs>::WireType... args)
		{
			try
			{
				LPXLOPER12 pvRetVal = getReturnValue();
				HRESULT hr = SetValue(pvRetVal,
					func(ArgumentMarshaler<TArgs>::Marshal(args)...));
				if (FAILED(hr))
				{
					throw std::invalid_argument(
						"Cannot convert return value to XLOPER12.");
				}
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

		static inline FunctionInfo& GetFunctionInfo()
		{
			static FunctionInfo& s_info = FunctionInfo::Create(EntryPoint);
			return s_info;
		}
	};
}

//
// Macro to create and export a wrapper for UDF. Requirements:
//
// *) The macro must be placed in a source file.
// *) The macro must refer to a UDF accessible from that source file.
// *) The UDF must have external linkage.
// *) The macro may be put in any namespace.
//

#define EXPORT_XLL_FUNCTION(f) \
	static auto XLWrapperInfo_##f = ::XLL_NAMESPACE::FunctionInfoBuilder( \
		::XLL_NAMESPACE::XLWrapper<decltype(f),f>::GetFunctionInfo()) \
		.Name(L#f)