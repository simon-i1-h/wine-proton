/*
 * Copyright 2006 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <assert.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

struct HTMLStyleSheet {
    DispatchEx dispex;
    IHTMLStyleSheet IHTMLStyleSheet_iface;

    LONG ref;

    nsIDOMCSSStyleSheet *nsstylesheet;
};

struct HTMLStyleSheetsCollection {
    DispatchEx dispex;
    IHTMLStyleSheetsCollection IHTMLStyleSheetsCollection_iface;

    LONG ref;

    nsIDOMStyleSheetList *nslist;
};

struct HTMLStyleSheetRulesCollection {
    DispatchEx dispex;
    IHTMLStyleSheetRulesCollection IHTMLStyleSheetRulesCollection_iface;

    LONG ref;

    nsIDOMCSSRuleList *nslist;
};

struct HTMLStyleSheetRule {
    DispatchEx dispex;
    IHTMLStyleSheetRule IHTMLStyleSheetRule_iface;

    LONG ref;

    nsIDOMCSSRule *nsstylesheetrule;
};

static inline HTMLStyleSheetRule *impl_from_IHTMLStyleSheetRule(IHTMLStyleSheetRule *iface)
{
    return CONTAINING_RECORD(iface, HTMLStyleSheetRule, IHTMLStyleSheetRule_iface);
}

static HRESULT WINAPI HTMLStyleSheetRule_QueryInterface(IHTMLStyleSheetRule *iface,
        REFIID riid, void **ppv)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if (IsEqualGUID(&IID_IUnknown, riid))
        *ppv = &This->IHTMLStyleSheetRule_iface;
    else if (IsEqualGUID(&IID_IHTMLStyleSheetRule, riid))
        *ppv = &This->IHTMLStyleSheetRule_iface;
    else if (dispex_query_interface(&This->dispex, riid, ppv))
        return *ppv ? S_OK : E_NOINTERFACE;
    else
    {
        *ppv = NULL;
        FIXME("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLStyleSheetRule_AddRef(IHTMLStyleSheetRule *iface)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLStyleSheetRule_Release(IHTMLStyleSheetRule *iface)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        if(This->nsstylesheetrule)
            nsIDOMCSSRule_Release(This->nsstylesheetrule);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLStyleSheetRule_GetTypeInfoCount(
        IHTMLStyleSheetRule *iface, UINT *pctinfo)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLStyleSheetRule_GetTypeInfo(IHTMLStyleSheetRule *iface,
        UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLStyleSheetRule_GetIDsOfNames(IHTMLStyleSheetRule *iface,
        REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLStyleSheetRule_Invoke(IHTMLStyleSheetRule *iface,
        DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLStyleSheetRule_put_selectorText(IHTMLStyleSheetRule *iface, BSTR v)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheetRule_get_selectorText(IHTMLStyleSheetRule *iface, BSTR *p)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheetRule_get_style(IHTMLStyleSheetRule *iface, IHTMLRuleStyle **p)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheetRule_get_readOnly(IHTMLStyleSheetRule *iface, VARIANT_BOOL *p)
{
    HTMLStyleSheetRule *This = impl_from_IHTMLStyleSheetRule(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLStyleSheetRuleVtbl HTMLStyleSheetRuleVtbl = {
    HTMLStyleSheetRule_QueryInterface,
    HTMLStyleSheetRule_AddRef,
    HTMLStyleSheetRule_Release,
    HTMLStyleSheetRule_GetTypeInfoCount,
    HTMLStyleSheetRule_GetTypeInfo,
    HTMLStyleSheetRule_GetIDsOfNames,
    HTMLStyleSheetRule_Invoke,
    HTMLStyleSheetRule_put_selectorText,
    HTMLStyleSheetRule_get_selectorText,
    HTMLStyleSheetRule_get_style,
    HTMLStyleSheetRule_get_readOnly
};

static const tid_t HTMLStyleSheetRule_iface_tids[] = {
    IHTMLStyleSheetRule_tid,
    0
};
static dispex_static_data_t HTMLStyleSheetRule_dispex = {
    NULL,
    DispHTMLStyleSheetRule_tid,
    HTMLStyleSheetRule_iface_tids
};

static IHTMLStyleSheetRule *HTMLStyleSheetRule_Create(nsIDOMCSSRule *nsstylesheetrule)
{
    HTMLStyleSheetRule *ret;
    nsresult nsres;

    if(!(ret = heap_alloc(sizeof(*ret))))
        return NULL;

    ret->IHTMLStyleSheetRule_iface.lpVtbl = &HTMLStyleSheetRuleVtbl;
    ret->ref = 1;
    ret->nsstylesheetrule = NULL;

    init_dispex(&ret->dispex, (IUnknown *)&ret->IHTMLStyleSheetRule_iface, &HTMLStyleSheetRule_dispex);

    if (nsstylesheetrule)
    {
        nsres = nsIDOMCSSRule_QueryInterface(nsstylesheetrule, &IID_nsIDOMCSSRule,
                (void **)&ret->nsstylesheetrule);
        if (NS_FAILED(nsres))
            ERR("Could not get nsIDOMCSSRule interface: %08x\n", nsres);
    }

    return &ret->IHTMLStyleSheetRule_iface;
}

static inline HTMLStyleSheetRulesCollection *impl_from_IHTMLStyleSheetRulesCollection(IHTMLStyleSheetRulesCollection *iface)
{
    return CONTAINING_RECORD(iface, HTMLStyleSheetRulesCollection, IHTMLStyleSheetRulesCollection_iface);
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_QueryInterface(IHTMLStyleSheetRulesCollection *iface,
        REFIID riid, void **ppv)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLStyleSheetRulesCollection_iface;
    }else if(IsEqualGUID(&IID_IHTMLStyleSheetRulesCollection, riid)) {
        *ppv = &This->IHTMLStyleSheetRulesCollection_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        FIXME("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLStyleSheetRulesCollection_AddRef(IHTMLStyleSheetRulesCollection *iface)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLStyleSheetRulesCollection_Release(IHTMLStyleSheetRulesCollection *iface)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        if(This->nslist)
            nsIDOMCSSRuleList_Release(This->nslist);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_GetTypeInfoCount(
        IHTMLStyleSheetRulesCollection *iface, UINT *pctinfo)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_GetTypeInfo(IHTMLStyleSheetRulesCollection *iface,
        UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_GetIDsOfNames(IHTMLStyleSheetRulesCollection *iface,
        REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_Invoke(IHTMLStyleSheetRulesCollection *iface,
        DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_get_length(IHTMLStyleSheetRulesCollection *iface,
        LONG *p)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    UINT32 len = 0;

    TRACE("(%p)->(%p)\n", This, p);

    if(This->nslist) {
        nsresult nsres;

        nsres = nsIDOMCSSRuleList_GetLength(This->nslist, &len);
        if(NS_FAILED(nsres))
            ERR("GetLength failed: %08x\n", nsres);
    }

    *p = len;
    return S_OK;
}

static HRESULT WINAPI HTMLStyleSheetRulesCollection_item(IHTMLStyleSheetRulesCollection *iface,
        LONG index, IHTMLStyleSheetRule **ppHTMLStyleSheetRule)
{
    HTMLStyleSheetRulesCollection *This = impl_from_IHTMLStyleSheetRulesCollection(iface);
    nsIDOMCSSRule *nsstylesheetrule;
    nsresult nsres;

    TRACE("(%p)->(%d %p)\n", This, index, ppHTMLStyleSheetRule);

    nsres = nsIDOMCSSRuleList_Item(This->nslist, index, &nsstylesheetrule);
    if(NS_FAILED(nsres))
        return map_nsresult(nsres);
    if(!nsstylesheetrule)
        return E_INVALIDARG;

    *ppHTMLStyleSheetRule = HTMLStyleSheetRule_Create(nsstylesheetrule);
    return *ppHTMLStyleSheetRule ? S_OK : E_OUTOFMEMORY;
}

static const IHTMLStyleSheetRulesCollectionVtbl HTMLStyleSheetRulesCollectionVtbl = {
    HTMLStyleSheetRulesCollection_QueryInterface,
    HTMLStyleSheetRulesCollection_AddRef,
    HTMLStyleSheetRulesCollection_Release,
    HTMLStyleSheetRulesCollection_GetTypeInfoCount,
    HTMLStyleSheetRulesCollection_GetTypeInfo,
    HTMLStyleSheetRulesCollection_GetIDsOfNames,
    HTMLStyleSheetRulesCollection_Invoke,
    HTMLStyleSheetRulesCollection_get_length,
    HTMLStyleSheetRulesCollection_item
};

static const tid_t HTMLStyleSheetRulesCollection_iface_tids[] = {
    IHTMLStyleSheetRulesCollection_tid,
    0
};
static dispex_static_data_t HTMLStyleSheetRulesCollection_dispex = {
    NULL,
    DispHTMLStyleSheetRulesCollection_tid,
    HTMLStyleSheetRulesCollection_iface_tids
};

static IHTMLStyleSheetRulesCollection *HTMLStyleSheetRulesCollection_Create(nsIDOMCSSRuleList *nslist)
{
    HTMLStyleSheetRulesCollection *ret;

    ret = heap_alloc(sizeof(*ret));
    ret->IHTMLStyleSheetRulesCollection_iface.lpVtbl = &HTMLStyleSheetRulesCollectionVtbl;
    ret->ref = 1;
    ret->nslist = nslist;

    init_dispex(&ret->dispex, (IUnknown*)&ret->IHTMLStyleSheetRulesCollection_iface, &HTMLStyleSheetRulesCollection_dispex);

    if(nslist)
        nsIDOMCSSRuleList_AddRef(nslist);

    return &ret->IHTMLStyleSheetRulesCollection_iface;
}

static inline HTMLStyleSheetsCollection *impl_from_IHTMLStyleSheetsCollection(IHTMLStyleSheetsCollection *iface)
{
    return CONTAINING_RECORD(iface, HTMLStyleSheetsCollection, IHTMLStyleSheetsCollection_iface);
}

static HRESULT WINAPI HTMLStyleSheetsCollection_QueryInterface(IHTMLStyleSheetsCollection *iface,
         REFIID riid, void **ppv)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLStyleSheetsCollection_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        *ppv = &This->IHTMLStyleSheetsCollection_iface;
    }else if(IsEqualGUID(&IID_IHTMLStyleSheetsCollection, riid)) {
        *ppv = &This->IHTMLStyleSheetsCollection_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        WARN("unsupported %s\n", debugstr_mshtml_guid(riid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLStyleSheetsCollection_AddRef(IHTMLStyleSheetsCollection *iface)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLStyleSheetsCollection_Release(IHTMLStyleSheetsCollection *iface)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        if(This->nslist)
            nsIDOMStyleSheetList_Release(This->nslist);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLStyleSheetsCollection_GetTypeInfoCount(IHTMLStyleSheetsCollection *iface,
        UINT *pctinfo)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLStyleSheetsCollection_GetTypeInfo(IHTMLStyleSheetsCollection *iface,
        UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLStyleSheetsCollection_GetIDsOfNames(IHTMLStyleSheetsCollection *iface,
        REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames,
            lcid, rgDispId);
}

static HRESULT WINAPI HTMLStyleSheetsCollection_Invoke(IHTMLStyleSheetsCollection *iface,
        DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
        VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid,
            wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLStyleSheetsCollection_get_length(IHTMLStyleSheetsCollection *iface,
        LONG *p)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    UINT32 len = 0;

    TRACE("(%p)->(%p)\n", This, p);

    if(This->nslist)
        nsIDOMStyleSheetList_GetLength(This->nslist, &len);

    *p = len;
    return S_OK;
}

static HRESULT WINAPI HTMLStyleSheetsCollection_get__newEnum(IHTMLStyleSheetsCollection *iface,
        IUnknown **p)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheetsCollection_item(IHTMLStyleSheetsCollection *iface,
        VARIANT *pvarIndex, VARIANT *pvarResult)
{
    HTMLStyleSheetsCollection *This = impl_from_IHTMLStyleSheetsCollection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_variant(pvarIndex), pvarResult);

    switch(V_VT(pvarIndex)) {
    case VT_I4: {
        nsIDOMStyleSheet *nsstylesheet;
        nsresult nsres;

        TRACE("index=%d\n", V_I4(pvarIndex));

        nsres = nsIDOMStyleSheetList_Item(This->nslist, V_I4(pvarIndex), &nsstylesheet);
        if(NS_FAILED(nsres) || !nsstylesheet) {
            WARN("Item failed: %08x\n", nsres);
            V_VT(pvarResult) = VT_EMPTY;
            return E_INVALIDARG;
        }

        V_VT(pvarResult) = VT_DISPATCH;
        V_DISPATCH(pvarResult) = (IDispatch*)HTMLStyleSheet_Create(nsstylesheet);

        return S_OK;
    }

    case VT_BSTR:
        FIXME("id=%s not implemented\n", debugstr_w(V_BSTR(pvarResult)));
        return E_NOTIMPL;

    default:
        WARN("Invalid index %s\n", debugstr_variant(pvarIndex));
    }

    return E_INVALIDARG;
}

static const IHTMLStyleSheetsCollectionVtbl HTMLStyleSheetsCollectionVtbl = {
    HTMLStyleSheetsCollection_QueryInterface,
    HTMLStyleSheetsCollection_AddRef,
    HTMLStyleSheetsCollection_Release,
    HTMLStyleSheetsCollection_GetTypeInfoCount,
    HTMLStyleSheetsCollection_GetTypeInfo,
    HTMLStyleSheetsCollection_GetIDsOfNames,
    HTMLStyleSheetsCollection_Invoke,
    HTMLStyleSheetsCollection_get_length,
    HTMLStyleSheetsCollection_get__newEnum,
    HTMLStyleSheetsCollection_item
};

static const tid_t HTMLStyleSheetsCollection_iface_tids[] = {
    IHTMLStyleSheetsCollection_tid,
    0
};
static dispex_static_data_t HTMLStyleSheetsCollection_dispex = {
    NULL,
    DispHTMLStyleSheetsCollection_tid,
    HTMLStyleSheetsCollection_iface_tids
};

IHTMLStyleSheetsCollection *HTMLStyleSheetsCollection_Create(nsIDOMStyleSheetList *nslist)
{
    HTMLStyleSheetsCollection *ret = heap_alloc(sizeof(HTMLStyleSheetsCollection));

    ret->IHTMLStyleSheetsCollection_iface.lpVtbl = &HTMLStyleSheetsCollectionVtbl;
    ret->ref = 1;

    if(nslist)
        nsIDOMStyleSheetList_AddRef(nslist);
    ret->nslist = nslist;

    init_dispex(&ret->dispex, (IUnknown*)&ret->IHTMLStyleSheetsCollection_iface,
            &HTMLStyleSheetsCollection_dispex);

    return &ret->IHTMLStyleSheetsCollection_iface;
}

static inline HTMLStyleSheet *impl_from_IHTMLStyleSheet(IHTMLStyleSheet *iface)
{
    return CONTAINING_RECORD(iface, HTMLStyleSheet, IHTMLStyleSheet_iface);
}

static HRESULT WINAPI HTMLStyleSheet_QueryInterface(IHTMLStyleSheet *iface, REFIID riid, void **ppv)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_mshtml_guid(riid), ppv);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        *ppv = &This->IHTMLStyleSheet_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        *ppv = &This->IHTMLStyleSheet_iface;
    }else if(IsEqualGUID(&IID_IHTMLStyleSheet, riid)) {
        *ppv = &This->IHTMLStyleSheet_iface;
    }else if(dispex_query_interface(&This->dispex, riid, ppv)) {
        return *ppv ? S_OK : E_NOINTERFACE;
    }else {
        *ppv = NULL;
        WARN("unsupported %s\n", debugstr_mshtml_guid(riid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI HTMLStyleSheet_AddRef(IHTMLStyleSheet *iface)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI HTMLStyleSheet_Release(IHTMLStyleSheet *iface)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        release_dispex(&This->dispex);
        if(This->nsstylesheet)
            nsIDOMCSSStyleSheet_Release(This->nsstylesheet);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI HTMLStyleSheet_GetTypeInfoCount(IHTMLStyleSheet *iface, UINT *pctinfo)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    TRACE("(%p)->(%p)\n", This, pctinfo);
    return IDispatchEx_GetTypeInfoCount(&This->dispex.IDispatchEx_iface, pctinfo);
}

static HRESULT WINAPI HTMLStyleSheet_GetTypeInfo(IHTMLStyleSheet *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    return IDispatchEx_GetTypeInfo(&This->dispex.IDispatchEx_iface, iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLStyleSheet_GetIDsOfNames(IHTMLStyleSheet *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    return IDispatchEx_GetIDsOfNames(&This->dispex.IDispatchEx_iface, riid, rgszNames, cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLStyleSheet_Invoke(IHTMLStyleSheet *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    return IDispatchEx_Invoke(&This->dispex.IDispatchEx_iface, dispIdMember, riid, lcid, wFlags, pDispParams,
            pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLStyleSheet_put_title(IHTMLStyleSheet *iface, BSTR v)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_title(IHTMLStyleSheet *iface, BSTR *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_parentStyleSheet(IHTMLStyleSheet *iface,
                                                          IHTMLStyleSheet **p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_owningElement(IHTMLStyleSheet *iface, IHTMLElement **p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_put_disabled(IHTMLStyleSheet *iface, VARIANT_BOOL v)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%x)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_disabled(IHTMLStyleSheet *iface, VARIANT_BOOL *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_readOnly(IHTMLStyleSheet *iface, VARIANT_BOOL *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_imports(IHTMLStyleSheet *iface,
                                                 IHTMLStyleSheetsCollection **p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_put_href(IHTMLStyleSheet *iface, BSTR v)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_href(IHTMLStyleSheet *iface, BSTR *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    nsAString href_str;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    nsAString_Init(&href_str, NULL);
    nsres = nsIDOMCSSStyleSheet_GetHref(This->nsstylesheet, &href_str);
    return return_nsstr(nsres, &href_str, p);
}

static HRESULT WINAPI HTMLStyleSheet_get_type(IHTMLStyleSheet *iface, BSTR *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_id(IHTMLStyleSheet *iface, BSTR *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_addImport(IHTMLStyleSheet *iface, BSTR bstrURL,
                                               LONG lIndex, LONG *plIndex)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%s %d %p)\n", This, debugstr_w(bstrURL), lIndex, plIndex);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_addRule(IHTMLStyleSheet *iface, BSTR bstrSelector,
                                             BSTR bstrStyle, LONG lIndex, LONG *plIndex)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%s %s %d %p)\n", This, debugstr_w(bstrSelector), debugstr_w(bstrStyle),
          lIndex, plIndex);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_removeImport(IHTMLStyleSheet *iface, LONG lIndex)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%d)\n", This, lIndex);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_removeRule(IHTMLStyleSheet *iface, LONG lIndex)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%d)\n", This, lIndex);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_put_media(IHTMLStyleSheet *iface, BSTR v)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_get_media(IHTMLStyleSheet *iface, BSTR *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLStyleSheet_put_cssText(IHTMLStyleSheet *iface, BSTR v)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    nsresult nsres;

    TRACE("(%p)->(%s)\n", This, debugstr_w(v));

    do {
        nsres = nsIDOMCSSStyleSheet_DeleteRule(This->nsstylesheet, 0);
    }while(NS_SUCCEEDED(nsres));

    if(v && *v) {
        UINT32 i, depth, idx;
        nsAString nsstr;
        WCHAR *ws;

        depth = 0;
        ws = heap_alloc(sizeof(*ws) * (lstrlenW(v) + 1));
        do
        {
            for (i = 0; v[i]; ++i)
            {
                ws[i] = v[i];
                if (ws[i] == '{')
                    ++depth;
                else if (ws[i] == '}' && !--depth)
                    break;
            }
            if (ws[i])
                ws[++i] = 0;

            v += i;

            for (i = 0; ws[i]; ++i)
                if (!iswspace(ws[i]))
                    break;

            if (!ws[i])
            {
                TRACE("Skipping empty part.\n");
                continue;
            }

            nsAString_InitDepend(&nsstr, ws);
            nsres = nsIDOMCSSStyleSheet_InsertRule(This->nsstylesheet, &nsstr, 0, &idx);
            nsAString_Finish(&nsstr);

            if(NS_FAILED(nsres))
                FIXME("InsertRule failed for string %s.\n", debugstr_w(ws));
            else
                TRACE("Added rule %s.\n", debugstr_w(ws));
        }
        while (*v);
        heap_free(ws);
    }

    return S_OK;
}

static HRESULT WINAPI HTMLStyleSheet_get_cssText(IHTMLStyleSheet *iface, BSTR *p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    nsIDOMCSSRuleList *nslist = NULL;
    nsIDOMCSSRule *nsrule;
    nsAString nsstr;
    UINT32 len;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    nsres = nsIDOMCSSStyleSheet_GetCssRules(This->nsstylesheet, &nslist);
    if(NS_FAILED(nsres)) {
        ERR("GetCssRules failed: %08x\n", nsres);
        return E_FAIL;
    }

    nsres = nsIDOMCSSRuleList_GetLength(nslist, &len);
    assert(nsres == NS_OK);

    if(len) {
        nsres = nsIDOMCSSRuleList_Item(nslist, 0, &nsrule);
        if(NS_FAILED(nsres))
            ERR("Item failed: %08x\n", nsres);
    }

    nsIDOMCSSRuleList_Release(nslist);
    if(NS_FAILED(nsres))
        return E_FAIL;

    if(!len) {
        *p = NULL;
        return S_OK;
    }

    nsAString_Init(&nsstr, NULL);
    nsres = nsIDOMCSSRule_GetCssText(nsrule, &nsstr);
    nsIDOMCSSRule_Release(nsrule);
    return return_nsstr(nsres, &nsstr, p);
}

static HRESULT WINAPI HTMLStyleSheet_get_rules(IHTMLStyleSheet *iface,
                                               IHTMLStyleSheetRulesCollection **p)
{
    HTMLStyleSheet *This = impl_from_IHTMLStyleSheet(iface);
    nsIDOMCSSRuleList *nslist = NULL;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    nsres = nsIDOMCSSStyleSheet_GetCssRules(This->nsstylesheet, &nslist);
    if(NS_FAILED(nsres)) {
        ERR("GetCssRules failed: %08x\n", nsres);
        return E_FAIL;
    }

    *p = HTMLStyleSheetRulesCollection_Create(nslist);
    return S_OK;
}

static const IHTMLStyleSheetVtbl HTMLStyleSheetVtbl = {
    HTMLStyleSheet_QueryInterface,
    HTMLStyleSheet_AddRef,
    HTMLStyleSheet_Release,
    HTMLStyleSheet_GetTypeInfoCount,
    HTMLStyleSheet_GetTypeInfo,
    HTMLStyleSheet_GetIDsOfNames,
    HTMLStyleSheet_Invoke,
    HTMLStyleSheet_put_title,
    HTMLStyleSheet_get_title,
    HTMLStyleSheet_get_parentStyleSheet,
    HTMLStyleSheet_get_owningElement,
    HTMLStyleSheet_put_disabled,
    HTMLStyleSheet_get_disabled,
    HTMLStyleSheet_get_readOnly,
    HTMLStyleSheet_get_imports,
    HTMLStyleSheet_put_href,
    HTMLStyleSheet_get_href,
    HTMLStyleSheet_get_type,
    HTMLStyleSheet_get_id,
    HTMLStyleSheet_addImport,
    HTMLStyleSheet_addRule,
    HTMLStyleSheet_removeImport,
    HTMLStyleSheet_removeRule,
    HTMLStyleSheet_put_media,
    HTMLStyleSheet_get_media,
    HTMLStyleSheet_put_cssText,
    HTMLStyleSheet_get_cssText,
    HTMLStyleSheet_get_rules
};

static const tid_t HTMLStyleSheet_iface_tids[] = {
    IHTMLStyleSheet_tid,
    0
};
static dispex_static_data_t HTMLStyleSheet_dispex = {
    NULL,
    DispHTMLStyleSheet_tid,
    HTMLStyleSheet_iface_tids
};

IHTMLStyleSheet *HTMLStyleSheet_Create(nsIDOMStyleSheet *nsstylesheet)
{
    HTMLStyleSheet *ret = heap_alloc(sizeof(HTMLStyleSheet));
    nsresult nsres;

    ret->IHTMLStyleSheet_iface.lpVtbl = &HTMLStyleSheetVtbl;
    ret->ref = 1;
    ret->nsstylesheet = NULL;

    init_dispex(&ret->dispex, (IUnknown*)&ret->IHTMLStyleSheet_iface, &HTMLStyleSheet_dispex);

    if(nsstylesheet) {
        nsres = nsIDOMStyleSheet_QueryInterface(nsstylesheet, &IID_nsIDOMCSSStyleSheet,
                (void**)&ret->nsstylesheet);
        if(NS_FAILED(nsres))
            ERR("Could not get nsICSSStyleSheet interface: %08x\n", nsres);
    }

    return &ret->IHTMLStyleSheet_iface;
}
