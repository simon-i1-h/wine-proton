/*
 * Window stations and desktops
 *
 * Copyright 2002 Alexandre Julliard
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

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winerror.h"
#include "wingdi.h"
#include "winuser.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "user_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(winstation);


/* callback for enumeration functions */
struct enum_proc_lparam
{
    NAMEENUMPROCA func;
    LPARAM        lparam;
};

static BOOL CALLBACK enum_names_WtoA( LPWSTR name, LPARAM lparam )
{
    struct enum_proc_lparam *data = (struct enum_proc_lparam *)lparam;
    char buffer[MAX_PATH];

    if (!WideCharToMultiByte( CP_ACP, 0, name, -1, buffer, sizeof(buffer), NULL, NULL ))
        return FALSE;
    return data->func( buffer, data->lparam );
}

/* return a handle to the directory where window station objects are created */
static HANDLE get_winstations_dir_handle(void)
{
    static HANDLE handle = NULL;
    WCHAR buffer[64];
    UNICODE_STRING str;
    OBJECT_ATTRIBUTES attr;

    if (!handle)
    {
        HANDLE dir;

        swprintf( buffer, ARRAY_SIZE(buffer), L"\\Sessions\\%u\\Windows\\WindowStations", NtCurrentTeb()->Peb->SessionId );
        RtlInitUnicodeString( &str, buffer );
        InitializeObjectAttributes( &attr, &str, 0, 0, NULL );
        NtOpenDirectoryObject( &dir, DIRECTORY_CREATE_OBJECT | DIRECTORY_TRAVERSE, &attr );
        if (InterlockedCompareExchangePointer( &handle, dir, 0 ) != 0) /* someone beat us here */
            CloseHandle( dir );
    }
    return handle;
}

static WCHAR default_name[29];

static BOOL WINAPI winstation_default_name_once( INIT_ONCE *once, void *param, void **context )
{
    TOKEN_STATISTICS stats;
    BOOL ret;

    ret = GetTokenInformation( GetCurrentProcessToken(), TokenStatistics, &stats, sizeof(stats), NULL );
    if (ret)
        swprintf( default_name, ARRAY_SIZE(default_name), L"Service-0x%x-%x$",
                  stats.AuthenticationId.HighPart, stats.AuthenticationId.LowPart );

    return ret;
}

static const WCHAR *get_winstation_default_name( void )
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    BOOL ret;

    ret = InitOnceExecuteOnce( &once, winstation_default_name_once, NULL, NULL );
    return ret ? default_name : NULL;
}


static void map_shared_memory_section( const WCHAR *name, SIZE_T size, HANDLE root, HANDLE *handle, void **ptr )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING section_str;
    NTSTATUS status;

    RtlInitUnicodeString( &section_str, name );
    InitializeObjectAttributes( &attr, &section_str, 0, root, NULL );
    status = NtOpenSection( handle, SECTION_ALL_ACCESS, &attr );
    if (status)
    {
        ERR( "failed to open section %s: %08x\n", debugstr_w(name), status );
        *ptr = NULL;
        *handle = NULL;
        return;
    }

    *ptr = NULL;
    status = NtMapViewOfSection( *handle, GetCurrentProcess(), ptr, 0, 0, NULL,
                                 &size, ViewUnmap, 0, PAGE_READONLY );
    if (status)
    {
        ERR( "failed to map view of section %s: %08x\n", debugstr_w(name), status );
        CloseHandle( *handle );
        *ptr = NULL;
        *handle = NULL;
    }
}


volatile struct desktop_shared_memory *get_desktop_shared_memory( void )
{
    static const WCHAR *dir_desktop_maps = L"__wine_desktop_mappings\\";
    struct user_thread_info *thread_info = get_user_thread_info();
    HANDLE root = get_winstations_dir_handle(), handles[2];
    WCHAR buf[MAX_PATH], *ptr;
    DWORD i, needed;

    if (thread_info->desktop_shared_memory) return thread_info->desktop_shared_memory;

    handles[0] = GetProcessWindowStation();
    handles[1] = GetThreadDesktop( GetCurrentThreadId() );

    memcpy( buf, dir_desktop_maps, wcslen(dir_desktop_maps) * sizeof(WCHAR) );
    ptr = buf + wcslen(dir_desktop_maps);

    for (i = 0; i < 2; i++)
    {
        GetUserObjectInformationW( handles[i], UOI_NAME, (void *)ptr, sizeof(buf) - (ptr - buf) * sizeof(WCHAR), &needed );
        ptr += needed / sizeof(WCHAR);
        if (i == 0) *(ptr - 1) = '\\';
    }

    map_shared_memory_section( buf, sizeof(struct desktop_shared_memory), root,
                               &thread_info->desktop_shared_map, (void **)&thread_info->desktop_shared_memory );
    return thread_info->desktop_shared_memory;
}


volatile struct queue_shared_memory *get_queue_shared_memory( void )
{
    struct user_thread_info *thread_info = get_user_thread_info();
    WCHAR buf[MAX_PATH];

    if (thread_info->queue_shared_memory) return thread_info->queue_shared_memory;

    swprintf( buf, ARRAY_SIZE(buf), L"\\KernelObjects\\__wine_thread_mappings\\%08x-queue", GetCurrentThreadId() );
    map_shared_memory_section( buf, sizeof(struct queue_shared_memory), NULL,
                               &thread_info->queue_shared_map, (void **)&thread_info->queue_shared_memory );
    return thread_info->queue_shared_memory;
}


static volatile struct input_shared_memory *get_thread_input_shared_memory( DWORD tid, HANDLE *handle,
                                                                            struct input_shared_memory **ptr )
{
    WCHAR buf[MAX_PATH];

    if (*ptr && (*ptr)->tid == tid) return *ptr;
    if (*ptr) CloseHandle( *handle );

    swprintf( buf, ARRAY_SIZE(buf), L"\\KernelObjects\\__wine_thread_mappings\\%08x-input", tid );
    map_shared_memory_section( buf, sizeof(struct input_shared_memory), NULL,
                               handle, (void **)ptr );
    return *ptr;
}


volatile struct input_shared_memory *get_input_shared_memory( void )
{
    volatile struct queue_shared_memory *queue = get_queue_shared_memory();
    struct user_thread_info *thread_info = get_user_thread_info();
    DWORD tid;

    if (!queue) return NULL;
    SHARED_READ_BEGIN( &queue->seq )
    {
        tid = queue->input_tid;
    }
    SHARED_READ_END( &queue->seq );

    return get_thread_input_shared_memory( tid, &thread_info->input_shared_map,
                                           &thread_info->input_shared_memory );
}


volatile struct input_shared_memory *get_foreground_shared_memory( void )
{
    volatile struct desktop_shared_memory *desktop = get_desktop_shared_memory();
    struct user_thread_info *thread_info = get_user_thread_info();
    DWORD tid;

    if (!desktop) return NULL;
    SHARED_READ_BEGIN( &desktop->seq )
    {
        tid = desktop->foreground_tid;
    }
    SHARED_READ_END( &desktop->seq );

    if (!tid) return NULL;
    return get_thread_input_shared_memory( tid, &thread_info->foreground_shared_map,
                                           &thread_info->foreground_shared_memory );
}


/***********************************************************************
 *              CreateWindowStationA  (USER32.@)
 */
HWINSTA WINAPI CreateWindowStationA( LPCSTR name, DWORD flags, ACCESS_MASK access,
                                     LPSECURITY_ATTRIBUTES sa )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return CreateWindowStationW( NULL, flags, access, sa );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return CreateWindowStationW( buffer, flags, access, sa );
}


/***********************************************************************
 *              CreateWindowStationW  (USER32.@)
 */
HWINSTA WINAPI CreateWindowStationW( LPCWSTR name, DWORD flags, ACCESS_MASK access,
                                     LPSECURITY_ATTRIBUTES sa )
{
    HANDLE ret;
    DWORD len = name ? lstrlenW(name) : 0;

    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    if (!len)
    {
        name = get_winstation_default_name();
        len = lstrlenW( name );
    }
    SERVER_START_REQ( create_winstation )
    {
        req->flags      = 0;
        req->access     = access;
        req->attributes = OBJ_CASE_INSENSITIVE |
                          ((flags & CWF_CREATE_ONLY) ? 0 : OBJ_OPENIF) |
                          ((sa && sa->bInheritHandle) ? OBJ_INHERIT : 0);
        req->rootdir    = wine_server_obj_handle( get_winstations_dir_handle() );
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        wine_server_call_err( req );
        ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              OpenWindowStationA  (USER32.@)
 */
HWINSTA WINAPI OpenWindowStationA( LPCSTR name, BOOL inherit, ACCESS_MASK access )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return OpenWindowStationW( NULL, inherit, access );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return OpenWindowStationW( buffer, inherit, access );
}


/******************************************************************************
 *              OpenWindowStationW  (USER32.@)
 */
HWINSTA WINAPI OpenWindowStationW( LPCWSTR name, BOOL inherit, ACCESS_MASK access )
{
    HANDLE ret = 0;
    DWORD len = name ? lstrlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    if (!len)
    {
        name = get_winstation_default_name();
        len = lstrlenW( name );
    }
    SERVER_START_REQ( open_winstation )
    {
        req->access     = access;
        req->attributes = OBJ_CASE_INSENSITIVE | (inherit ? OBJ_INHERIT : 0);
        req->rootdir    = wine_server_obj_handle( get_winstations_dir_handle() );
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *              CloseWindowStation  (USER32.@)
 */
BOOL WINAPI CloseWindowStation( HWINSTA handle )
{
    BOOL ret;
    SERVER_START_REQ( close_winstation )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              GetProcessWindowStation  (USER32.@)
 */
HWINSTA WINAPI GetProcessWindowStation(void)
{
    HWINSTA ret = 0;

    SERVER_START_REQ( get_process_winstation )
    {
        if (!wine_server_call_err( req ))
            ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *              SetProcessWindowStation  (USER32.@)
 */
BOOL WINAPI SetProcessWindowStation( HWINSTA handle )
{
    BOOL ret;

    SERVER_START_REQ( set_process_winstation )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              EnumWindowStationsA  (USER32.@)
 */
BOOL WINAPI EnumWindowStationsA( WINSTAENUMPROCA func, LPARAM lparam )
{
    struct enum_proc_lparam data;
    data.func   = func;
    data.lparam = lparam;
    return EnumWindowStationsW( enum_names_WtoA, (LPARAM)&data );
}


/******************************************************************************
 *              EnumWindowStationsW  (USER32.@)
 */
BOOL WINAPI EnumWindowStationsW( WINSTAENUMPROCW func, LPARAM lparam )
{
    unsigned int index = 0;
    WCHAR name[MAX_PATH];
    BOOL ret = TRUE;
    NTSTATUS status;

    while (ret)
    {
        SERVER_START_REQ( enum_winstation )
        {
            req->index = index;
            wine_server_set_reply( req, name, sizeof(name) - sizeof(WCHAR) );
            status = wine_server_call( req );
            name[wine_server_reply_size(reply)/sizeof(WCHAR)] = 0;
            index = reply->next;
        }
        SERVER_END_REQ;
        if (status == STATUS_NO_MORE_ENTRIES)
            break;
        if (status)
        {
            SetLastError( RtlNtStatusToDosError( status ) );
            return FALSE;
        }
        ret = func( name, lparam );
    }
    return ret;
}


/***********************************************************************
 *              CreateDesktopA   (USER32.@)
 */
HDESK WINAPI CreateDesktopA( LPCSTR name, LPCSTR device, LPDEVMODEA devmode,
                             DWORD flags, ACCESS_MASK access, LPSECURITY_ATTRIBUTES sa )
{
    WCHAR buffer[MAX_PATH];

    if (device || devmode)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (!name) return CreateDesktopW( NULL, NULL, NULL, flags, access, sa );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return CreateDesktopW( buffer, NULL, NULL, flags, access, sa );
}


/***********************************************************************
 *              CreateDesktopW   (USER32.@)
 */
HDESK WINAPI CreateDesktopW( LPCWSTR name, LPCWSTR device, LPDEVMODEW devmode,
                             DWORD flags, ACCESS_MASK access, LPSECURITY_ATTRIBUTES sa )
{
    HANDLE ret;
    DWORD len = name ? lstrlenW(name) : 0;

    if (device || devmode)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( create_desktop )
    {
        req->flags      = flags;
        req->access     = access;
        req->attributes = OBJ_CASE_INSENSITIVE | OBJ_OPENIF |
                          ((sa && sa->bInheritHandle) ? OBJ_INHERIT : 0);
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        wine_server_call_err( req );
        ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              OpenDesktopA   (USER32.@)
 */
HDESK WINAPI OpenDesktopA( LPCSTR name, DWORD flags, BOOL inherit, ACCESS_MASK access )
{
    WCHAR buffer[MAX_PATH];

    if (!name) return OpenDesktopW( NULL, flags, inherit, access );

    if (!MultiByteToWideChar( CP_ACP, 0, name, -1, buffer, MAX_PATH ))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    return OpenDesktopW( buffer, flags, inherit, access );
}


HDESK open_winstation_desktop( HWINSTA hwinsta, LPCWSTR name, DWORD flags, BOOL inherit, ACCESS_MASK access )
{
    HANDLE ret = 0;
    DWORD len = name ? lstrlenW(name) : 0;
    if (len >= MAX_PATH)
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( open_desktop )
    {
        req->winsta     = wine_server_obj_handle( hwinsta );
        req->flags      = flags;
        req->access     = access;
        req->attributes = OBJ_CASE_INSENSITIVE | (inherit ? OBJ_INHERIT : 0);
        wine_server_add_data( req, name, len * sizeof(WCHAR) );
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              OpenDesktopW   (USER32.@)
 */
HDESK WINAPI OpenDesktopW( LPCWSTR name, DWORD flags, BOOL inherit, ACCESS_MASK access )
{
    return open_winstation_desktop( NULL, name, flags, inherit, access );
}


/***********************************************************************
 *              CloseDesktop  (USER32.@)
 */
BOOL WINAPI CloseDesktop( HDESK handle )
{
    BOOL ret;
    SERVER_START_REQ( close_desktop )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              GetThreadDesktop   (USER32.@)
 */
HDESK WINAPI GetThreadDesktop( DWORD thread )
{
    HDESK ret = 0;

    SERVER_START_REQ( get_thread_desktop )
    {
        req->tid = thread;
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              SetThreadDesktop   (USER32.@)
 */
BOOL WINAPI SetThreadDesktop( HDESK handle )
{
    BOOL ret;

    SERVER_START_REQ( set_thread_desktop )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    if (ret)  /* reset the desktop windows */
    {
        struct user_thread_info *thread_info = get_user_thread_info();
        thread_info->top_window = 0;
        thread_info->msg_window = 0;
        if (thread_info->desktop_shared_map)
        {
            CloseHandle( thread_info->desktop_shared_map );
            thread_info->desktop_shared_map = NULL;
            thread_info->desktop_shared_memory = NULL;
        }
    }
    return ret;
}


/******************************************************************************
 *              EnumDesktopsA   (USER32.@)
 */
BOOL WINAPI EnumDesktopsA( HWINSTA winsta, DESKTOPENUMPROCA func, LPARAM lparam )
{
    struct enum_proc_lparam data;
    data.func   = func;
    data.lparam = lparam;
    return EnumDesktopsW( winsta, enum_names_WtoA, (LPARAM)&data );
}


/******************************************************************************
 *              EnumDesktopsW   (USER32.@)
 */
BOOL WINAPI EnumDesktopsW( HWINSTA winsta, DESKTOPENUMPROCW func, LPARAM lparam )
{
    unsigned int index = 0;
    WCHAR name[MAX_PATH];
    BOOL ret = TRUE;
    NTSTATUS status;

    if (!winsta)
        winsta = GetProcessWindowStation();

    while (ret)
    {
        SERVER_START_REQ( enum_desktop )
        {
            req->winstation = wine_server_obj_handle( winsta );
            req->index      = index;
            wine_server_set_reply( req, name, sizeof(name) - sizeof(WCHAR) );
            status = wine_server_call( req );
            name[wine_server_reply_size(reply)/sizeof(WCHAR)] = 0;
            index = reply->next;
        }
        SERVER_END_REQ;
        if (status == STATUS_NO_MORE_ENTRIES)
            break;
        if (status)
        {
            SetLastError( RtlNtStatusToDosError( status ) );
            return FALSE;
        }
        ret = func(name, lparam);
    }
    return ret;
}


/******************************************************************************
 *              OpenInputDesktop   (USER32.@)
 */
HDESK WINAPI OpenInputDesktop( DWORD flags, BOOL inherit, ACCESS_MASK access )
{
    HANDLE ret = 0;

    TRACE( "(%x,%i,%x)\n", flags, inherit, access );

    if (flags)
        FIXME( "partial stub flags %08x\n", flags );

    SERVER_START_REQ( open_input_desktop )
    {
        req->flags      = flags;
        req->access     = access;
        req->attributes = inherit ? OBJ_INHERIT : 0;
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    return ret;
}


/***********************************************************************
 *              GetUserObjectInformationA   (USER32.@)
 */
BOOL WINAPI GetUserObjectInformationA( HANDLE handle, INT index, LPVOID info, DWORD len, LPDWORD needed )
{
    /* check for information types returning strings */
    if (index == UOI_TYPE || index == UOI_NAME)
    {
        WCHAR buffer[MAX_PATH];
        DWORD lenA, lenW;

        if (!GetUserObjectInformationW( handle, index, buffer, sizeof(buffer), &lenW )) return FALSE;
        lenA = WideCharToMultiByte( CP_ACP, 0, buffer, -1, NULL, 0, NULL, NULL );
        if (needed) *needed = lenA;
        if (lenA > len)
        {
            /* If the buffer length supplied by the caller is insufficient, Windows returns a
               'needed' length based upon the Unicode byte length, so we should do similarly. */
            if (needed) *needed = lenW;

            SetLastError( ERROR_INSUFFICIENT_BUFFER );
            return FALSE;
        }
        if (info) WideCharToMultiByte( CP_ACP, 0, buffer, -1, info, len, NULL, NULL );
        return TRUE;
    }
    return GetUserObjectInformationW( handle, index, info, len, needed );
}


/***********************************************************************
 *              GetUserObjectInformationW   (USER32.@)
 */
BOOL WINAPI GetUserObjectInformationW( HANDLE handle, INT index, LPVOID info, DWORD len, LPDWORD needed )
{
    BOOL ret;

    switch(index)
    {
    case UOI_FLAGS:
        {
            USEROBJECTFLAGS *obj_flags = info;
            if (needed) *needed = sizeof(*obj_flags);
            if (len < sizeof(*obj_flags))
            {
                SetLastError( ERROR_BUFFER_OVERFLOW );
                return FALSE;
            }
            SERVER_START_REQ( set_user_object_info )
            {
                req->handle    = wine_server_obj_handle( handle );
                req->flags     = 0;
                ret = !wine_server_call_err( req );
                if (ret)
                {
                    /* FIXME: inherit flag */
                    obj_flags->dwFlags = reply->old_obj_flags;
                }
            }
            SERVER_END_REQ;
        }
        return ret;

    case UOI_TYPE:
        SERVER_START_REQ( set_user_object_info )
        {
            req->handle = wine_server_obj_handle( handle );
            req->flags  = 0;
            ret = !wine_server_call_err( req );
            if (ret)
            {
                size_t size = reply->is_desktop ? sizeof(L"Desktop") : sizeof(L"WindowStation");
                if (needed) *needed = size;
                if (len < size)
                {
                    SetLastError( ERROR_INSUFFICIENT_BUFFER );
                    ret = FALSE;
                }
                else memcpy( info, reply->is_desktop ? L"Desktop" : L"WindowStation", size );
            }
        }
        SERVER_END_REQ;
        return ret;

    case UOI_NAME:
        {
            WCHAR buffer[MAX_PATH];
            SERVER_START_REQ( set_user_object_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->flags  = 0;
                wine_server_set_reply( req, buffer, sizeof(buffer) - sizeof(WCHAR) );
                ret = !wine_server_call_err( req );
                if (ret)
                {
                    size_t size = wine_server_reply_size( reply );
                    buffer[size / sizeof(WCHAR)] = 0;
                    size += sizeof(WCHAR);
                    if (needed) *needed = size;
                    if (len < size)
                    {
                        SetLastError( ERROR_INSUFFICIENT_BUFFER );
                        ret = FALSE;
                    }
                    else memcpy( info, buffer, size );
                }
            }
            SERVER_END_REQ;
        }
        return ret;

    case UOI_USER_SID:
        FIXME( "not supported index %d\n", index );
        /* fall through */
    default:
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
}


/******************************************************************************
 *              SetUserObjectInformationA   (USER32.@)
 */
BOOL WINAPI SetUserObjectInformationA( HANDLE handle, INT index, LPVOID info, DWORD len )
{
    return SetUserObjectInformationW( handle, index, info, len );
}


/******************************************************************************
 *              SetUserObjectInformationW   (USER32.@)
 */
BOOL WINAPI SetUserObjectInformationW( HANDLE handle, INT index, LPVOID info, DWORD len )
{
    BOOL ret;
    const USEROBJECTFLAGS *obj_flags = info;

    if (index != UOI_FLAGS || !info || len < sizeof(*obj_flags))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    /* FIXME: inherit flag */
    SERVER_START_REQ( set_user_object_info )
    {
        req->handle    = wine_server_obj_handle( handle );
        req->flags     = SET_USER_OBJECT_SET_FLAGS;
        req->obj_flags = obj_flags->dwFlags;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *              GetUserObjectSecurity   (USER32.@)
 */
BOOL WINAPI GetUserObjectSecurity( HANDLE handle, PSECURITY_INFORMATION info,
                                   PSECURITY_DESCRIPTOR sid, DWORD len, LPDWORD needed )
{
    FIXME( "(%p %p %p len=%d %p),stub!\n", handle, info, sid, len, needed );
    if (needed)
        *needed = sizeof(SECURITY_DESCRIPTOR);
    if (len < sizeof(SECURITY_DESCRIPTOR))
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }
    return InitializeSecurityDescriptor(sid, SECURITY_DESCRIPTOR_REVISION);
}

/***********************************************************************
 *              SetUserObjectSecurity   (USER32.@)
 */
BOOL WINAPI SetUserObjectSecurity( HANDLE handle, PSECURITY_INFORMATION info,
                                   PSECURITY_DESCRIPTOR sid )
{
    FIXME( "(%p,%p,%p),stub!\n", handle, info, sid );
    return TRUE;
}
