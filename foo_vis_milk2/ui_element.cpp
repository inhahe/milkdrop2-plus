/*
 * ui_element.cpp - Implements the MilkDrop 2 visualization component.
 *
 * Copyright (c) 2023-2024 Jimmy Cassis
 * SPDX-License-Identifier: MPL-2.0
 */

#include "pch.h"
#include "ui_element.h"
#include "favorites_manager.h"
#include "preset_browser.h"

#pragma hdrstop

#ifdef __AVX__
#error `/arch:AVX`, `/arch:AVX2` or `/arch:AVX512` compiler flag set.
#endif

//#ifdef __clang__
//#pragma clang diagnostic ignored "-Wcovered-switch-default"
//#pragma clang diagnostic ignored "-Wswitch-enum"
//#endif

CPlugin g_plugin;
HWND g_hWindow;

// Indicates to hybrid graphics systems to prefer the discrete part by default.
//extern "C" {
//    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
//    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
//}

namespace
{
milk2_ui_element::milk2_ui_element(ui_element_config::ptr config, ui_element_instance_callback_ptr p_callback) :
    m_callback(p_callback),
    m_bMsgHandled(TRUE),
    play_callback_impl_base(flag_on_playback_starting | flag_on_playback_new_track | flag_on_playback_stop),
    playlist_callback_impl_base(flag_on_items_added | flag_on_items_reordered | flag_on_items_removed | flag_on_items_selection_change |
                                flag_on_item_focus_change | flag_on_items_modified | flag_on_playlist_activate | flag_on_playlists_reorder |
                                flag_on_playlists_removed | flag_on_playback_order_changed)
{
    m_milk2 = false;
    m_in_sizemove = false;
    m_in_suspend = false;
    m_minimized = false;
#if defined(TIMER_TP)
    m_tpTimer = nullptr;
#elif defined(TIMER_32)
    m_last_time = 0.0;
#endif
    m_refresh_interval = 33;
    m_art_data = std::make_unique<artFetchData>();

    m_pwd = L".\\";
    set_configuration(config);
}

ui_element_config::ptr milk2_ui_element::g_get_default_configuration()
{
    MILK2_CONSOLE_LOG("g_get_default_configuration")

    try
    {
        ui_element_config_builder builder;
        milk2_config config;
        config.init();
        config.build(builder, false);
        return builder.finish(g_get_guid());
    }
    catch (exception_io& exc)
    {
        FB2K_console_print(core_api::get_my_file_name(), ": Exception while building default configuration data - ", exc);
        return ui_element_config::g_create_empty(g_get_guid());
    }
}

void milk2_ui_element::set_configuration(ui_element_config::ptr p_data)
{
    MILK2_CONSOLE_LOG("set_configuration")

    //LPVOID dataptr = const_cast<LPVOID>(p_data->get_data());
    //if (dataptr && p_data->get_data_size() >= 4 && static_cast<DWORD*>(dataptr)[0] == ('M' | 'I' << 8 | 'L' << 16 | 'K' << 24))
    //    s_config = p_data;
    //else
    //    s_config = g_get_default_configuration();

    if (s_milk2)
        return;

    ui_element_config_parser parser(p_data);
    s_config.parse(parser);
}

ui_element_config::ptr milk2_ui_element::get_configuration()
{
    MILK2_CONSOLE_LOG("get_configuration")

    ui_element_config_builder builder;
    s_config.build(builder, !s_in_toggle);
    return builder.finish(g_get_guid());
}

void milk2_ui_element::notify(const GUID& p_what, size_t p_param1, const void* p_param2, size_t p_param2size)
{
    UNREFERENCED_PARAMETER(p_param1);
    UNREFERENCED_PARAMETER(p_param2);
    UNREFERENCED_PARAMETER(p_param2size);

    MILK2_CONSOLE_LOG("notify")
    if (p_what == ui_element_notify_colors_changed || p_what == ui_element_notify_font_changed)
        Invalidate();
}

HWND GetRealParent(HWND hWnd)
{
    HWND hWndOwner;

    // To obtain a window's owner window, instead of using `GetParent()`,
    // use `GetWindow()` with the `GW_OWNER` flag.
    if (NULL != (hWndOwner = GetWindow(hWnd, GW_OWNER)))
        return hWndOwner;

    // Obtain the parent window and not the owner.
    return GetAncestor(hWnd, GA_PARENT);
}

int milk2_ui_element::OnCreate(LPCREATESTRUCT cs)
{
#ifndef _DEBUG
    UNREFERENCED_PARAMETER(cs);
#endif

    MILK2_CONSOLE_LOG("OnCreate0 ", cs->x, ", ", cs->y, ", ", GetWnd())
    if (!XMVerifyCPUSupport()) {
        FB2K_console_print(core_api::get_my_file_name(), ": CPU does not support mathematics intrinsics. Exiting.");
        return E_FAIL;
    }

    if (!s_milk2)
    {
        ResolvePwd();
        s_config.init();
#ifdef TIMER_TP
        InitializeCriticalSection(&s_cs);
#endif
    }

    if (!m_milk2)
    {
        SetPwd(s_pwd);

        try
        {
            static_api_ptr_t<visualisation_manager> vis_manager;

            vis_manager->create_stream(m_vis_stream, 0);

            m_vis_stream->request_backlog(0.8);
            UpdateChannelMode();
        }
        catch (std::exception& exc)
        {
            FB2K_console_print(core_api::get_my_file_name(), ": Exception while creating visualization stream - ", exc);
        }

#if defined(TIMER_TP)
        m_tpTimer = CreateThreadpoolTimer(TimerCallback, this, NULL);
#elif defined(TIMER_DX)
        message_loop_v2::get()->add_idle_handler(this);
#endif

        RegisterForArtwork();
    }

    HRESULT hr = S_OK;
    int w, h;
    GetDefaultSize(w, h);

    CRect r{};
    WIN32_OP_D(GetClientRect(&r))
    if (r.right - r.left > 0 && r.bottom - r.top > 0)
    {
        w = r.right - r.left;
        h = r.bottom - r.top;
    }
    if (!Initialize(get_wnd(), w, h))
    {
        FB2K_console_print(core_api::get_my_file_name(), ": Could not initialize MilkDrop");
    }

    // Preset browser is initialized lazily on first use (context menu, keyboard shortcut).
    // Do NOT call InitPresetBrowser() here — MilkDrop engine isn't fully ready yet.

    MILK2_CONSOLE_LOG("OnCreate1 ", r.right, ", ", r.left, ", ", r.top, ", ", r.bottom, ", ", GetWnd())

    return hr;
}

void milk2_ui_element::OnClose()
{
    MILK2_CONSOLE_LOG("OnClose ", GetWnd())
    //DestroyWindow();
}

void milk2_ui_element::OnDestroy()
{
    MILK2_CONSOLE_LOG("OnDestroy ", GetWnd())
#if defined(TIMER_TP)
    StopTimer();
    EnterCriticalSection(&s_cs);
#elif defined(TIMER_DX)
    message_loop_v2::get()->remove_idle_handler(this);
#endif
    if (m_vis_stream.is_valid())
        m_vis_stream.release();
    //DestroyMenu();
    auto manager = now_playing_album_art_notify_manager::tryGet();
    if (manager.is_valid())
        manager->remove(this);

    if (m_milk2)
        m_milk2 = false;
    s_count = 0ull;

    if (!s_in_toggle)
    {
        MILK2_CONSOLE_LOG("ExitVis")
        s_fullscreen = false;
        s_in_toggle = false;
        s_was_topmost = false;
        s_milk2 = false;
#if defined(TIMER_32)
        KillTimer(ID_REFRESH_TIMER);
#elif defined(TIMER_TP)
        DeleteCriticalSection(&s_cs);
#endif
        wcscpy_s(s_config.settings.m_szPresetDir, g_plugin.GetPresetDir()); // save last "Load Preset" menu directory
        g_plugin.PluginQuit();

        HWND parent = GetRealParent(get_wnd());
        ::SetClassLongPtr(parent, GCLP_HICON, NULL);
        ::SetClassLongPtr(parent, GCLP_HICONSM, NULL);

        //PostQuitMessage(0);
    }
    else
    {
        s_in_toggle = false;
    }
#ifdef TIMER_TP
    LeaveCriticalSection(&s_cs);
#endif
}

void milk2_ui_element::OnTimer(UINT_PTR nIDEvent)
{
    UNREFERENCED_PARAMETER(nIDEvent);

    MILK2_CONSOLE_LOG_LIMIT("OnTimer ", GetWnd())
#ifdef TIMER_32
    KillTimer(ID_REFRESH_TIMER);
    InvalidateRect(NULL, TRUE);
#endif
}

void milk2_ui_element::OnPaint(CDCHandle dc)
{
    (void)dc;
    MILK2_CONSOLE_LOG_LIMIT("OnPaint ", GetWnd())
    if (m_in_sizemove && m_milk2) // foobar2000 does not enter/exit size/move
    {
        Tick();
    }
    else
    {
        PAINTSTRUCT ps;
        std::ignore = BeginPaint(&ps);
        EndPaint(&ps);
    }
#ifdef TIMER_TP
    StartTimer();
#endif
    ValidateRect(NULL);
#ifdef TIMER_32
    ULONGLONG now = GetTickCount64();
#endif
    if (m_vis_stream.is_valid())
    {
        BuildWaves();
#ifdef TIMER_32
        ULONGLONG next_refresh = m_last_refresh + m_refresh_interval;
        // (next_refresh < now) would break when GetTickCount() overflows
        if (static_cast<LONGLONG>(next_refresh - now) < 0)
        {
            next_refresh = now;
        }
        SetTimer(ID_REFRESH_TIMER, static_cast<UINT>(next_refresh - now));
#endif
    }
#ifdef TIMER_32
    m_last_refresh = now;
#endif
}

BOOL milk2_ui_element::OnEraseBkgnd(CDCHandle dc)
{
    (void)dc;
    MILK2_CONSOLE_LOG_LIMIT("OnEraseBkgnd ", GetWnd())
    ++s_count;

#if 0
    CRect r;
    WIN32_OP_D(GetClientRect(&r));
    CBrush brush;
    //const COLORREF rgbBlack = 0x00000000;
    WIN32_OP_D(brush.CreateSolidBrush(m_callback->query_std_color(ui_color_background)) != NULL);
    WIN32_OP_D(dc.FillRect(&r, brush));
#else
    if (!m_milk2 && !s_fullscreen && s_milk2)
    {
        int w, h;
        GetDefaultSize(w, h);

        CRect r{};
        WIN32_OP_D(GetClientRect(&r))
        if (r.right - r.left > 0 && r.bottom - r.top > 0)
        {
            w = r.right - r.left;
            h = r.bottom - r.top;
        }
        if (!Initialize(get_wnd(), w, h))
        {
            FB2K_console_print(core_api::get_my_file_name(), ": Could not initialize MilkDrop");
        }
    }
#ifdef TIMER_32
    Tick();
#endif
#endif
#if 0
    FB2K_console_print(m_timer.GetFramesPerSecond(), "FPS");
#endif

    return TRUE;
}

void milk2_ui_element::OnMove(CPoint ptPos)
{
    UNREFERENCED_PARAMETER(ptPos);

    MILK2_CONSOLE_LOG("OnMove ", GetWnd())
    if (m_milk2)
    {
#ifdef TIMER_TP
        if (TryEnterCriticalSection(&s_cs) == 0)
            return;
#endif
        g_plugin.OnWindowMoved();
#ifdef TIMER_TP
        LeaveCriticalSection(&s_cs);
#endif
    }
}

void milk2_ui_element::OnSize(UINT nType, CSize size)
{
    MILK2_CONSOLE_LOG("OnSize0 ", nType, ", ", size.cx, ", ", size.cy, ", ", GetWnd())
    if (nType == SIZE_MINIMIZED)
    {
        if (!m_minimized)
        {
            m_minimized = true;
            if (!m_in_suspend && m_milk2)
                OnSuspending();
            m_in_suspend = true;
        }
    }
    else if (m_minimized)
    {
        m_minimized = false;
        if (m_in_suspend && m_milk2)
            OnResuming();
        m_in_suspend = false;
    }
    else if (!m_in_sizemove && m_milk2)
    {
        int width = size.cx;
        int height = size.cy;

        if (!width || !height)
            return;
        if (width < 128)
            width = 128;
        if (height < 128)
            height = 128;
        MILK2_CONSOLE_LOG("OnSize1 ", nType, ", ", size.cx, ", ", size.cy, ", ", GetWnd())
#ifdef TIMER_TP
        if (TryEnterCriticalSection(&s_cs) == 0)
            return;
#endif
        g_plugin.OnWindowSizeChanged(size.cx, size.cy);
#ifdef TIMER_TP
        LeaveCriticalSection(&s_cs);
#endif
    }
}

void milk2_ui_element::OnEnterSizeMove()
{
    MILK2_CONSOLE_LOG("OnEnterSizeMove ", GetWnd())
    m_in_sizemove = true;
}

void milk2_ui_element::OnExitSizeMove()
{
    MILK2_CONSOLE_LOG("OnExitSizeMove ", GetWnd())
    m_in_sizemove = false;
    if (m_milk2)
    {
#ifdef TIMER_TP
        if (TryEnterCriticalSection(&s_cs) == 0)
            return;
#endif
        RECT rc;
        WIN32_OP_D(GetClientRect(&rc));
        g_plugin.OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
#ifdef TIMER_TP
        LeaveCriticalSection(&s_cs);
#endif
    }
}

// To avoid a 1-pixel-thick border of noise.
LRESULT milk2_ui_element::OnNcCalcSize(BOOL bCalcValidRects, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    MILK2_CONSOLE_LOG("OnNcCalcSize ", GetWnd())
    if (bCalcValidRects == TRUE)
    {
        //auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        //adjust_fullscreen_client_rect(get_wnd(), params.rgrc[0]);
        return 0;
    }
    return 0;
}

BOOL milk2_ui_element::OnCopyData(CWindow wnd, PCOPYDATASTRUCT pcds)
{
    UNREFERENCED_PARAMETER(wnd);
    //typedef struct
    //{
    //    wchar_t error[1024];
    //} ErrorCopy;

    MILK2_CONSOLE_LOG("OnCopyData ", GetWnd())
    switch (pcds->dwData)
    {
        case 0x09: // PRINT STDOUT
            {
                LPCTSTR lpszString = (LPCTSTR)((ErrorCopy*)(pcds->lpData))->error;
                FB2K_console_print(core_api::get_my_file_name(), ": ", lpszString);
                break;
            }
    }

    return TRUE;
}

void milk2_ui_element::OnDisplayChange(UINT uBitsPerPixel, CSize sizeScreen)
{
    UNREFERENCED_PARAMETER(uBitsPerPixel);
    UNREFERENCED_PARAMETER(sizeScreen);

    MILK2_CONSOLE_LOG("OnDisplayChange ", GetWnd())
    if (m_milk2)
    {
        g_plugin.OnDisplayChange();
    }
}

void milk2_ui_element::OnDpiChanged(UINT nDpiX, UINT nDpiY, PRECT pRect)
{
    UNREFERENCED_PARAMETER(nDpiX);
    UNREFERENCED_PARAMETER(nDpiY);
    UNREFERENCED_PARAMETER(pRect);
}

void milk2_ui_element::OnGetMinMaxInfo(LPMINMAXINFO lpMMI)
{
    lpMMI->ptMinTrackSize.x = 150; // 320
    lpMMI->ptMinTrackSize.y = 150; // 200
}

void milk2_ui_element::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
    UNREFERENCED_PARAMETER(dwThreadID);

    MILK2_CONSOLE_LOG("OnActivateApp ", GetWnd())
    if (m_milk2)
    {
        if (bActive)
        {
            OnActivated();
        }
        else
        {
            OnDeactivated();
        }
    }
}

BOOL milk2_ui_element::OnPowerBroadcast(DWORD dwPowerEvent, DWORD_PTR dwData)
{
    UNREFERENCED_PARAMETER(dwData);

    MILK2_CONSOLE_LOG("OnPowerBroadcast ", GetWnd())
    switch (dwPowerEvent)
    {
        case PBT_APMQUERYSUSPEND:
            if (!m_in_suspend && m_milk2)
                OnSuspending();
            m_in_suspend = true;
            break;
        case PBT_APMRESUMESUSPEND:
            if (!m_minimized)
            {
                if (m_in_suspend && m_milk2)
                    OnResuming();
                m_in_suspend = false;
            }
            break;
        default:
            break;
    }

    return TRUE;
}

void milk2_ui_element::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);

    MILK2_CONSOLE_LOG("OnLButtonDblClk ", GetWnd())
    ToggleFullScreen();
}

#pragma region Keyboard Controls
#include "keyboard.cpp"
#pragma endregion

void milk2_ui_element::OnContextMenu(CWindow wnd, CPoint point)
{
    UNREFERENCED_PARAMETER(wnd);

    InitPresetBrowser(); // lazy init on first use

    MILK2_CONSOLE_LOG("OnContextMenu ", point.x, ", ", point.y, ", ", GetWnd())
    if (m_callback->is_edit_mode_enabled())
    {
        SetMsgHandled(FALSE);
        return;
    }

    CMenu menu;
    WIN32_OP_D(menu.CreatePopupMenu());

    // Current preset name (grayed).
    std::wstring currentPreset = GetCurrentPreset();
    bool isFav = m_favorites.IsFavorite(GetCurrentPresetRelativePath());
    std::wstring presetLabel = currentPreset;
    if (isFav)
        presetLabel += L" \x2605"; // Unicode star for favorites
    menu.AppendMenu(MF_GRAYED, IDM_CURRENT_PRESET, presetLabel.c_str());
    menu.AppendMenu(MF_SEPARATOR);

    // Check if engine is ready (scan complete and a preset is loaded).
    bool bEngineReady = g_plugin.m_nPresets > 0;
    UINT disableIfNotReady = bEngineReady ? MF_STRING : (MF_STRING | MF_GRAYED);

    // Favorite add/remove for current preset.
    // Show hotkeys only in fullscreen (keys don't reach the plugin when embedded).
    if (s_fullscreen)
    {
        menu.AppendMenu(disableIfNotReady | (isFav ? MF_GRAYED : 0), IDM_ADD_FAVORITE, TEXT("Add to Favorites\tF11"));
        menu.AppendMenu(disableIfNotReady | (!isFav ? MF_GRAYED : 0), IDM_REMOVE_FAVORITE, TEXT("Remove from Favorites\tShift+F11"));
    }
    else
    {
        menu.AppendMenu(disableIfNotReady | (isFav ? MF_GRAYED : 0), IDM_ADD_FAVORITE, TEXT("Add to Favorites"));
        menu.AppendMenu(disableIfNotReady | (!isFav ? MF_GRAYED : 0), IDM_REMOVE_FAVORITE, TEXT("Remove from Favorites"));
    }
    menu.AppendMenu(MF_SEPARATOR);

    if (!bEngineReady)
    {
        wchar_t scanLabel[128];
        swprintf_s(scanLabel, L"Scanning presets... (%d found)", g_plugin.m_nPresets);
        menu.AppendMenu(MF_GRAYED, static_cast<UINT_PTR>(0), scanLabel);
        menu.AppendMenu(MF_SEPARATOR);
    }

    // Preset navigation.
    if (s_fullscreen)
    {
        menu.AppendMenu(disableIfNotReady, IDM_NEXT_PRESET, TEXT("Next Preset\tSpace"));
        menu.AppendMenu(disableIfNotReady, IDM_PREVIOUS_PRESET, TEXT("Previous Preset\tBackspace"));
        menu.AppendMenu(disableIfNotReady, IDM_SHUFFLE_PRESET, TEXT("Random Preset\tR"));
        menu.AppendMenu(disableIfNotReady | (IsPresetLock() ? MF_CHECKED : 0), IDM_LOCK_PRESET, TEXT("Lock Preset\tScroll Lock"));
    }
    else
    {
        menu.AppendMenu(disableIfNotReady, IDM_NEXT_PRESET, TEXT("Next Preset"));
        menu.AppendMenu(disableIfNotReady, IDM_PREVIOUS_PRESET, TEXT("Previous Preset"));
        menu.AppendMenu(disableIfNotReady, IDM_SHUFFLE_PRESET, TEXT("Random Preset"));
        menu.AppendMenu(disableIfNotReady | (IsPresetLock() ? MF_CHECKED : 0), IDM_LOCK_PRESET, TEXT("Lock Preset"));
    }
    menu.AppendMenu(MF_SEPARATOR);

    // Shuffle mode submenu.
    CMenu shuffleMenu;
    shuffleMenu.CreatePopupMenu();
    BuildShuffleModeSubmenu(shuffleMenu);
    menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(shuffleMenu.m_hMenu), TEXT("Shuffle Mode"));

    // Browse by category submenu.
    int nextBrowseId = IDM_BROWSE_CATEGORY_BASE;
    CMenu browseMenu;
    browseMenu.CreatePopupMenu();
    BuildBrowseSubmenu(browseMenu, nextBrowseId);
    menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(browseMenu.m_hMenu), TEXT("Random Preset from Category"));

    // Category selector.
    {
        wchar_t catLabel[64];
        swprintf_s(catLabel, L"Select Categories... (%zu selected)", m_selectedCategories.size());
        menu.AppendMenu(MF_STRING, IDM_SELECT_CATEGORIES_DLG, catLabel);
    }

    // Favorites.
    {
        size_t favCount = m_favorites.Count();
        wchar_t favLabel[64];
        swprintf_s(favLabel, L"Manage Favorites... (%zu)", favCount);
        menu.AppendMenu(MF_STRING, IDM_MANAGE_FAVORITES, favLabel);
    }

    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, IDM_SET_PRESET_LIBRARY, TEXT("Set Preset Library Folder..."));
    menu.AppendMenu(MF_STRING, IDM_RESCAN_PRESETS, TEXT("Rescan Presets"));
    if (!m_presetLibraryPath.empty())
    {
        // Show current library path (truncated if long).
        std::wstring pathLabel = m_presetLibraryPath;
        if (pathLabel.length() > 50)
            pathLabel = L"..." + pathLabel.substr(pathLabel.length() - 47);
        menu.AppendMenu(MF_GRAYED, static_cast<UINT_PTR>(0), pathLabel.c_str());
    }
    menu.AppendMenu(MF_SEPARATOR);

    // Original menu items.
    menu.AppendMenu(MF_STRING | (s_config.settings.m_bEnableDownmix ? MF_CHECKED : 0), IDM_ENABLE_DOWNMIX, TEXT("Downmix Channels"));
    menu.AppendMenu(MF_SEPARATOR);
    if (s_fullscreen)
    {
        menu.AppendMenu(MF_STRING | (g_plugin.m_show_playlist ? MF_CHECKED : 0), IDM_SHOW_PLAYLIST, TEXT("Show Playlist\tP"));
        menu.AppendMenu(MF_STRING | (s_config.settings.m_bShowAlbum && std::filesystem::exists(s_config.settings.m_szImgIniFile)
                                         ? MF_CHECKED
                                         : (std::filesystem::exists(s_config.settings.m_szImgIniFile) ? 0 : MF_DISABLED)),
                        IDM_SHOW_ALBUM, TEXT("Show Album Art"));
        menu.AppendMenu(MF_STRING, IDM_SHOW_TITLE, TEXT("Launch Title\tT"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING | (g_plugin.m_show_help ? MF_CHECKED : 0), IDM_SHOW_HELP, TEXT("Show Help\tF1"));
        menu.AppendMenu(MF_STRING, IDM_SHOW_PREFS, TEXT("Launch Preferences Page\tAlt+K"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING | (s_fullscreen ? MF_CHECKED : 0), IDM_TOGGLE_FULLSCREEN, TEXT("Fullscreen\tEsc to exit"));
    }
    else
    {
        menu.AppendMenu(MF_STRING | (g_plugin.m_show_playlist ? MF_CHECKED : 0), IDM_SHOW_PLAYLIST, TEXT("Show Playlist"));
        menu.AppendMenu(MF_STRING | (s_config.settings.m_bShowAlbum && std::filesystem::exists(s_config.settings.m_szImgIniFile)
                                         ? MF_CHECKED
                                         : (std::filesystem::exists(s_config.settings.m_szImgIniFile) ? 0 : MF_DISABLED)),
                        IDM_SHOW_ALBUM, TEXT("Show Album Art"));
        menu.AppendMenu(MF_STRING, IDM_SHOW_TITLE, TEXT("Launch Title"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING | (g_plugin.m_show_help ? MF_CHECKED : 0), IDM_SHOW_HELP, TEXT("Show Help"));
        menu.AppendMenu(MF_STRING, IDM_SHOW_PREFS, TEXT("Launch Preferences Page"));
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING | (s_fullscreen ? MF_CHECKED : 0), IDM_TOGGLE_FULLSCREEN, TEXT("Fullscreen"));
    }

    int cmd = menu.TrackPopupMenu(TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, *this);

    // Handle the command.
    switch (cmd)
    {
        case 0:
            // User cancelled.
            break;
        case IDM_TOGGLE_FULLSCREEN:
            if (g_plugin.GetFrame() > 0)
                ToggleFullScreen();
            break;
        case IDM_NEXT_PRESET:
            NextPreset();
            break;
        case IDM_PREVIOUS_PRESET:
            PrevPreset();
            break;
        case IDM_LOCK_PRESET:
            LockPreset(!IsPresetLock());
            break;
        case IDM_SHUFFLE_PRESET:
            RandomPreset();
            break;
        case IDM_ADD_FAVORITE:
        {
            std::wstring relPath = GetCurrentPresetRelativePath();
            if (!relPath.empty() && !m_favorites.IsFavorite(relPath))
            {
                m_favorites.Add(relPath);
                m_favorites.Save();
                wchar_t buf[512];
                swprintf_s(buf, L"Added to favorites: %s", relPath.c_str());
                g_plugin.AddError(buf, 2.0f, ERR_NOTIFY, false);
            }
            break;
        }
        case IDM_REMOVE_FAVORITE:
        {
            std::wstring relPath = GetCurrentPresetRelativePath();
            if (!relPath.empty() && m_favorites.IsFavorite(relPath))
            {
                m_favorites.Remove(relPath);
                m_favorites.Save();
                wchar_t buf[512];
                swprintf_s(buf, L"Removed from favorites: %s", relPath.c_str());
                g_plugin.AddError(buf, 2.0f, ERR_NOTIFY, false);
            }
            break;
        }
        case IDM_SHUFFLE_MODE_ALL:
            SetShuffleMode(ShuffleMode::All);
            break;
        case IDM_SHUFFLE_MODE_CATEGORY:
            SetShuffleMode(ShuffleMode::Category);
            break;
        case IDM_SHUFFLE_MODE_FAVORITES:
            SetShuffleMode(ShuffleMode::Favorites);
            break;
        case IDM_SHUFFLE_MODE_FAV_CATEGORY:
            SetShuffleMode(ShuffleMode::FavoritesCategory);
            break;
        case IDM_RESCAN_PRESETS:
            m_browserInitialized = false;
            InitPresetBrowser();
            break;
        case IDM_SELECT_ALL_CATEGORIES:
            SelectAllCategories();
            break;
        case IDM_DESELECT_ALL_CATEGORIES:
            DeselectAllCategories();
            break;
        case IDM_SELECT_ALL_FAV_CATEGORIES:
        {
            auto favCats = m_favorites.GetFavoriteCategories();
            m_selectedFavCategories.clear();
            for (const auto& c : favCats)
                m_selectedFavCategories.insert(c);
            SaveBrowserConfig();
            SyncFiltersToEngine();
            break;
        }
        case IDM_DESELECT_ALL_FAV_CATEGORIES:
            m_selectedFavCategories.clear();
            SaveBrowserConfig();
            SyncFiltersToEngine();
            break;
        case IDM_SELECT_CATEGORIES_DLG:
            ShowCategorySelector();
            break;
        case IDM_MANAGE_FAVORITES:
            ShowFavoritesManager();
            break;
        case IDM_SET_PRESET_LIBRARY:
            BrowseForPresetLibrary();
            break;
        case IDM_ENABLE_DOWNMIX:
            s_config.settings.m_bEnableDownmix = !s_config.settings.m_bEnableDownmix;
            UpdateChannelMode();
            break;
        case IDM_SHOW_PREFS:
            ShowPreferencesPage();
            break;
        case IDM_SHOW_HELP:
            ToggleHelp();
            break;
        case IDM_SHOW_PLAYLIST:
            TogglePlaylist();
            break;
        case IDM_SHOW_TITLE:
            LaunchSongTitle();
            break;
        case IDM_SHOW_ALBUM:
            s_config.settings.m_bShowAlbum = !s_config.settings.m_bShowAlbum;
            ShowAlbumArt();
            break;
        case IDM_QUIT:
            break;
        default:
            // Handle Random Preset from Category: load a random preset from the clicked category.
            if (cmd >= IDM_BROWSE_CATEGORY_BASE && cmd < IDM_SHUFFLE_CATEGORY_BASE)
            {
                int catIdx = cmd - IDM_BROWSE_CATEGORY_BASE;
                auto categories = m_presetBrowser.GetCategories();
                if (catIdx >= 0 && catIdx < static_cast<int>(categories.size()))
                {
                    auto preset = m_presetBrowser.GetRandomPresetInCategory(categories[catIdx]);
                    if (!preset.fullPath.empty())
                    {
                        g_plugin.LoadPreset(preset.fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
                    }
                    else
                    {
                        wchar_t buf[256];
                        swprintf_s(buf, L"No presets found in category: %s", categories[catIdx].c_str());
                        g_plugin.AddError(buf, 3.0f, ERR_NOTIFY, false);
                    }
                }
            }
            // Handle Shuffle category toggle IDs (checkboxes).
            else if (cmd >= IDM_SHUFFLE_CATEGORY_BASE && cmd < IDM_SHUFFLE_FAV_CATEGORY_BASE)
            {
                int catIdx = cmd - IDM_SHUFFLE_CATEGORY_BASE;
                auto categories = m_presetBrowser.GetCategories();
                if (catIdx >= 0 && catIdx < static_cast<int>(categories.size()))
                {
                    ToggleCategorySelection(categories[catIdx]);
                }
            }
            // Handle favorite-category toggle IDs (multi-select).
            else if (cmd >= IDM_SHUFFLE_FAV_CATEGORY_BASE && cmd < IDM_PRESET_BASE)
            {
                int catIdx = cmd - IDM_SHUFFLE_FAV_CATEGORY_BASE;
                auto favCategories = m_favorites.GetFavoriteCategories();
                if (catIdx >= 0 && catIdx < static_cast<int>(favCategories.size()))
                {
                    auto it = m_selectedFavCategories.find(favCategories[catIdx]);
                    if (it != m_selectedFavCategories.end())
                        m_selectedFavCategories.erase(it);
                    else
                        m_selectedFavCategories.insert(favCategories[catIdx]);
                    SaveBrowserConfig();
                    SyncFiltersToEngine();
                }
            }
            // Handle dynamic preset loading from favorites list.
            else if (cmd >= IDM_PRESET_BASE)
            {
                if (!g_plugin.m_bPresetListReady || g_plugin.m_nPresets == 0)
                {
                    g_plugin.AddError(const_cast<wchar_t*>(L"Presets are still loading, please wait..."), 2.0f, ERR_NOTIFY, false);
                }
                else
                {
                    int presetIdx = cmd - IDM_PRESET_BASE;
                    auto favs = m_favorites.GetAll();
                    if (presetIdx >= 0 && presetIdx < static_cast<int>(favs.size()))
                    {
                        std::wstring fullPath = m_presetBrowser.GetRootDir() + favs[presetIdx].relativePath;
                        g_plugin.LoadPreset(fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
                    }
                }
            }
            break;
    }

    // Detach submenus so they don't get destroyed twice.
    shuffleMenu.Detach();
    browseMenu.Detach();

    Invalidate();
}

LRESULT milk2_ui_element::OnImeNotify(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    MILK2_CONSOLE_LOG("OnImeNotify ", GetWnd())
    if (uMsg != WM_IME_NOTIFY)
        return 1;
    if (wParam == IMN_CLOSESTATUSWINDOW)
    {
        if (s_fullscreen && !s_in_toggle)
            ToggleFullScreen();
    }
    return 0;
}

LRESULT milk2_ui_element::OnQuit(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    MILK2_CONSOLE_LOG("OnQuit ", GetWnd())
    return 0;
}

LRESULT milk2_ui_element::OnMilk2Message(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg != WM_MILK2)
        return -1;

    auto api = playlist_manager::get();
    if (LOBYTE(wParam) == 0x21 && HIBYTE(wParam) == 0x09)
    {
        wchar_t buf[2048], title[64];
        LoadString(core_api::get_my_instance(), LOWORD(lParam), buf, 2048);
        LoadString(core_api::get_my_instance(), HIWORD(lParam), title, 64);
        MILK2_CONSOLE_LOG("milk2 -> title: ", title, ", message: ", buf)
        return HIWORD(lParam) == IDS_MILKDROP_ERROR || HIWORD(lParam) == IDS_MILKDROP_WARNING ? 0 : 1;
    }
    else if (lParam == IPC_GETVERSION)
    {
        MILK2_CONSOLE_LOG("IPC_GETVERSION")
        //const t_core_version_data v = core_version_info_v2::get_version();
        return 1;
    }
    else if (lParam == IPC_GETVERSIONSTRING)
    {
        MILK2_CONSOLE_LOG("IPC_GETVERSIONSTRING")
        return reinterpret_cast<LRESULT>(core_version_info_v2::g_get_version_string()); // "foobar2000 v2.1.2"
        return 0;
    }
    else if (lParam == IPC_ISPLAYING)
    {
        MILK2_CONSOLE_LOG("IPC_ISPLAYING")
        if (m_playback_control->is_playing())
            return 1;
        else if (m_playback_control->is_paused())
            return 3;
        else
            return 0;
    }
    else if (lParam == IPC_SETPLAYLISTPOS)
    {
        //MILK2_CONSOLE_LOG("IPC_SETPLAYLISTPOS")
        SetSelectionSingle(static_cast<size_t>(g_plugin.m_playlist_pos));
        return static_cast<LRESULT>(wParam);
    }
    else if (lParam == IPC_GETLISTLENGTH)
    {
        //MILK2_CONSOLE_LOG("IPC_GETLISTLENGTH")
        const size_t count = api->activeplaylist_get_item_count();
        return static_cast<LRESULT>(count);
    }
    else if (lParam == IPC_GETLISTPOS)
    {
        //MILK2_CONSOLE_LOG("IPC_GETLISTPOS")
        if (m_playback_control->is_playing())
        {
            size_t playing_index = NULL, playing_playlist = NULL;
            bool valid = api->get_playing_item_location(&playing_playlist, &playing_index);
            if (valid && playing_playlist == api->get_active_playlist())
                return static_cast<LRESULT>(playing_index);
        }
        return -1;
    }
    else if (lParam == IPC_GETPLAYLISTTITLEW || lParam == IPC_GET_PLAYING_TITLE)
    {
        //MILK2_CONSOLE_LOG(IPC_GETPLAYLISTTITLEW ? "IPC_GETPLAYLISTTITLEW" : "IPC_GET_PLAYING_TITLE")
        titleformat_object::ptr title_format;
        if (lParam == IPC_GETPLAYLISTTITLEW && m_script.is_empty())
        {
            pfc::string8 pattern = pfc::utf8FromWide(s_config.settings.m_szTitleFormat);
            static_api_ptr_t<titleformat_compiler>()->compile_safe_ex(m_script, pattern);
        }
        if (lParam == IPC_GET_PLAYING_TITLE && m_title.is_empty())
        {
            pfc::string8 pattern = default_szTitleFormat;
            static_api_ptr_t<titleformat_compiler>()->compile_safe_ex(m_title, pattern);
        }
        pfc::string_formatter state;
        metadb_handle_list list;
        api->activeplaylist_get_all_items(list);
        if (list.size() == 0)
            state = ""; // no playlist
        else if (wParam == -1 || !(list.get_item(static_cast<size_t>(wParam)))->format_title(NULL, state, IPC_GETPLAYLISTTITLEW ? m_script : m_title, NULL))
            if (m_playback_control->is_playing())
                state = "Opening...";
            else
                state = "Stopped.";
        m_szBuffer = pfc::wideFromUTF8(state);
        return reinterpret_cast<LRESULT>(m_szBuffer.c_str());
    }
    else if (lParam == IPC_GETOUTPUTTIME)
    {
        //MILK2_CONSOLE_LOG("IPC_GETOUTPUTTIME")
        if (wParam == 0)
        {
            if (m_playback_control->is_playing())
                return static_cast<LRESULT>(m_playback_control->playback_get_position() * 1000);
        }
        else if (wParam == 1)
        {
            if (m_playback_control->is_playing())
                return static_cast<LRESULT>(m_playback_control->playback_get_length());
        }
        else if (wParam == 2)
        {
            if (m_playback_control->is_playing())
                return static_cast<LRESULT>(m_playback_control->playback_get_length() * 1000);
        }
        return -1;
    }
    else if (lParam == IPC_GETPLUGINDIRECTORYW)
    {
        //MILK2_CONSOLE_LOG("IPC_GETPLUGINDIRECTORYW")
        m_szBuffer = s_config.settings.m_szPluginsDirPath;
        return reinterpret_cast<LRESULT>(m_szBuffer.c_str());
    }
    else if (lParam == IPC_GETINIDIRECTORYW)
    {
        //MILK2_CONSOLE_LOG("IPC_GETINIDIRECTORYW")
        m_szBuffer = s_config.settings.m_szConfigIniFile;
        size_t p = m_szBuffer.find_last_of(L"\\");
        if (p != std::wstring::npos)
            m_szBuffer = m_szBuffer.substr(0, p + 1);
        return reinterpret_cast<LRESULT>(m_szBuffer.c_str());
    }
    else if (lParam == IPC_FETCH_ALBUMART)
    {
        MILK2_CONSOLE_LOG("IPC_FETCH_ALBUMART")
        if (m_art_file.empty())
        {
            m_art_data->imgData = m_raster.data();
            m_art_data->imgDataLen = static_cast<int>(m_raster.size());
            m_art_data->type[0] = L'j'; m_art_data->type[1] = L'p'; m_art_data->type[2] = L'g'; m_art_data->type[3] = L'\0';
            m_art_data->gracenoteFileId = nullptr;
        }
        else
        {
            m_art_data->imgData = nullptr;
            m_art_data->imgDataLen = 0;
            m_art_data->gracenoteFileId = m_art_file.data();
            std::wstring ext = GetExtension(m_art_file);
            std::copy_n(ext.begin(), ext.size(), &m_art_data->type[0]);
            std::fill_n(&m_art_data->type[0] + ext.size() + 1, 10 - ext.size(), L'\0');
        }
        s_config.settings.m_artData = m_art_data.get();

        return reinterpret_cast<LRESULT>(m_art_data.get());
    }
#if 0
    else if (lParam == IPC_SETVISWND)
    {
        //MILK2_CONSOLE_LOG("IPC_SETVISWND")
        //SendMessage(m_hwnd_winamp, WM_WA_IPC, (WPARAM)m_hwnd, IPC_SETVISWND);
    }
    else if (lParam == IPC_CB_VISRANDOM)
    {
        //MILK2_CONSOLE_LOG("IPC_CB_VISRANDOM")
        //SendMessage(GetWinampWindow(), WM_WA_IPC, (m_bPresetLockOnAtStartup ? 0 : 1) << 16, IPC_CB_VISRANDOM);
    }
    else if (lParam == IPC_IS_PLAYING_VIDEO)
    {
        //MILK2_CONSOLE_LOG("IPC_IS_PLAYING_VIDEO")
        //if (m_screenmode == FULLSCREEN && SendMessage(GetWinampWindow(), WM_WA_IPC, 0, IPC_IS_PLAYING_VIDEO) > 1)
    }
    else if (lParam == IPC_SET_VIS_FS_FLAG)
    {
        //MILK2_CONSOLE_LOG("IPC_SET_VIS_FS_FLAG")
        //SendMessage(GetWinampWindow(), WM_WA_IPC, 1, IPC_SET_VIS_FS_FLAG);
    }
    else if (lParam == IPC_GET_D3DX9)
    {
        //MILK2_CONSOLE_LOG("IPC_GET_D3DX9")
        //HMODULE d3dx9 = (HMODULE)SendMessage(winamp, WM_WA_IPC, 0, IPC_GET_D3DX9);
    }
    else if (lParam == IPC_GET_API_SERVICE)
    {
        //MILK2_CONSOLE_LOG("IPC_GET_API_SERVICE")
        //WASABI_API_SVC = (api_service*)SendMessage(hwndParent, WM_WA_IPC, 0, IPC_GET_API_SERVICE);
    }
    else if (lParam == IPC_GETDIALOGBOXPARENT)
    {
        //MILK2_CONSOLE_LOG("IPC_GETDIALOGBOXPARENT")
        //HWND parent = (HWND)SendMessage(winamp, WM_WA_IPC, 0, IPC_GETDIALOGBOXPARENT);
    }
    else if (lParam == IPC_GET_RANDFUNC)
    {
        //MILK2_CONSOLE_LOG("IPC_GET_RANDFUNC")
        //int warand();
        //warand = (int (*)(void))SendMessage(hwndParent, WM_WA_IPC, 0, IPC_GET_RANDFUNC);
    }
#endif

    return 1;
}

LRESULT milk2_ui_element::OnConfigurationChange(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    MILK2_CONSOLE_LOG("OnConfigurationChange ", GetWnd())
    if (uMsg != WM_CONFIG_CHANGE)
        return 1;
    switch (wParam)
    {
        case 0: // Preferences Dialog
            {
                s_config.reset();
                m_script.reset();
                m_refresh_interval = static_cast<DWORD>(lround(1000.0f / s_config.settings.m_max_fps_fs));
                g_plugin.PanelSettings(&s_config.settings);
                break;
            }
        case 1: // Advanced Preferences
            {
                break;
            }
    }

    if (s_milk2 && m_milk2)
    {
#ifdef TIMER_TP
        EnterCriticalSection(&s_cs);
#endif
        RECT rect{};
        GetClientRect(&rect);
        g_plugin.OnWindowSizeChanged(rect.right - rect.left, rect.bottom - rect.top);
#ifdef TIMER_TP
        LeaveCriticalSection(&s_cs);
#endif
    }
    SetMsgHandled(TRUE);

    return 0;
}

// Initialize the Direct3D resources required to run.
bool milk2_ui_element::Initialize(HWND window, int width, int height)
{
    if (!s_milk2)
    {
        if (FALSE == g_plugin.PluginPreInitialize(window, core_api::get_my_instance()))
            return false;
        if (!g_plugin.PanelSettings(&s_config.settings))
            return false;

        //swprintf_s(g_plugin.m_szPluginsDirPath, L"%ls", s_config.settings.m_szPluginsDirPath);
        //swprintf_s(g_plugin.m_szConfigIniFile, L"%ls", s_config.settings.m_szConfigIniFile);
        swprintf_s(g_plugin.m_szComponentDirPath, L"%ls", const_cast<wchar_t*>(m_pwd.c_str()));

        if (!s_fullscreen)
            g_hWindow = get_wnd();
    }

    g_plugin.SetWinampWindow(window);

    if (!s_milk2)
    {
        if (FALSE == g_plugin.PluginInitialize(width, height))
            return false;

        HICON hIcon = ::LoadIcon(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_MILK2_ICON));
        HWND parent = GetRealParent(get_wnd());
        ::SetClassLongPtr(parent, GCLP_HICON, (LONG_PTR)hIcon);
        ::SetClassLongPtr(parent, GCLP_HICONSM, (LONG_PTR)hIcon);

        s_milk2 = true;
    }
    else
    {
#ifdef TIMER_TP
        EnterCriticalSection(&s_cs);
#endif
        g_plugin.OnWindowSwap(window, width, height);
#ifdef TIMER_TP
        LeaveCriticalSection(&s_cs);
#endif
    }

    m_milk2 = true;

    return true;
}

#pragma region Frame Update
// Executes the render.
void milk2_ui_element::Tick()
{
#ifdef TIMER_TP
    if (TryEnterCriticalSection(&s_cs) == 0)
        return;
#endif

#ifdef TIMER_DX
    m_timer.Tick([&]() { Update(m_timer); });
#endif

    Render();

#ifdef TIMER_TP
    LeaveCriticalSection(&s_cs);

    InvalidateRect(NULL, TRUE);
#endif
}

#ifdef TIMER_DX
// Updates the world.
void milk2_ui_element::Update(DX::StepTimer const& timer)
{
}
#endif
#pragma endregion

#pragma region Frame Render
// Draws the scene.
HRESULT milk2_ui_element::Render()
{
    // Do not try to render anything before the first `Update()`.
#ifdef TIMER_DX
    if (m_timer.GetFrameCount() == 0)
    {
        return S_OK;
    }
#else
    if (g_plugin.GetFrame() == 0)
    {
    }
#endif

    Clear();

    return g_plugin.PluginRender(waves[0].data(), waves[1].data());
}

// Clears the back buffers and the window contents.
void milk2_ui_element::Clear()
{
#if 0
    HDC hdc = GetDC();
    RECT rect{};
    GetClientRect(&rect);
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    ReleaseDC(hdc);
#endif
}

void milk2_ui_element::BuildWaves()
{
    //if (!m_vis_stream.is_valid())
    //    return;

    double time;
    if (!m_vis_stream->get_absolute_time(time))
        return;

    double dt = time - m_last_time;
    m_last_time = time;

    constexpr double min_time = 1.0 / 1000.0;
    constexpr double max_time = 1.0 / 10.0;

    bool use_fake = false;

    if (dt < min_time)
    {
        dt = min_time;
        use_fake = true;
    }
    else if (dt > max_time)
        dt = max_time;

    audio_chunk_impl chunk;
    if (use_fake || !m_vis_stream->get_chunk_absolute(chunk, time - dt, dt))
    {
        //m_vis_stream->make_fake_chunk_absolute(chunk, time - dt, dt);
        for (uint32_t i = 0; i < static_cast<uint32_t>(NUM_AUDIO_BUFFER_SAMPLES); ++i)
        {
            waves[0][i] = waves[1][i] = 0U;
        }
        return;
    }
    auto count = chunk.get_sample_count();
    //auto sample_rate = chunk.get_srate();
    auto channels = chunk.get_channel_count();
    audio_sample* audio_data = chunk.get_data();

    size_t top = std::min(count / channels, static_cast<size_t>(NUM_AUDIO_BUFFER_SAMPLES));
    for (size_t i = 0; i < top; ++i)
    {
        waves[0][i] = static_cast<float>(audio_data[i * channels] * 128.0f);
        if (channels >= 2)
            waves[1][i] = static_cast<float>(audio_data[i * channels + 1] * 128.0f);
        else
            waves[1][i] = waves[0][i];
    }
}
#pragma endregion

#pragma region Message Handlers
void milk2_ui_element::OnActivated()
{
}

void milk2_ui_element::OnDeactivated()
{
}

void milk2_ui_element::OnSuspending()
{
}

void milk2_ui_element::OnResuming()
{
#ifdef TIMER_DX
    m_timer.ResetElapsedTime();
#endif
}
#pragma endregion

// Properties
void milk2_ui_element::GetDefaultSize(int& width, int& height) const noexcept
{
    width = 640;
    height = 480;
}

void milk2_ui_element::SetPwd(std::wstring pwd) noexcept
{
    m_pwd.assign(pwd);
}

void milk2_ui_element::ToggleFullScreen()
{
    MILK2_CONSOLE_LOG("ToggleFullScreen0 ", GetWnd())
    if (m_milk2)
    {
        if (!s_fullscreen)
        {
            g_hWindow = get_wnd();
            m_milk2 = false;
        }
#if 0
        if (s_fullscreen)
        {
            SetWindowLongPtr(GWL_STYLE, WS_OVERLAPPEDWINDOW);
            SetWindowLongPtr(GWL_EXSTYLE, 0);

            int width = 800;
            int height = 600;

            ShowWindow(SW_SHOWNORMAL);

            SetWindowPos(HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        else
        {
            SetWindowLongPtr(GWL_STYLE, WS_POPUP);
            SetWindowLongPtr(GWL_EXSTYLE, WS_EX_TOPMOST);

            SetWindowPos(HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

            ShowWindow(SW_SHOWMAXIMIZED);
        }
#endif
        s_fullscreen = !s_fullscreen;
        s_in_toggle = true;
        SetTopMost();
        static_api_ptr_t<ui_element_common_methods_v2>()->toggle_fullscreen(g_get_guid(), core_api::get_main_window());
        MILK2_CONSOLE_LOG("ToggleFullScreen1 ", GetWnd())
    }
}

void milk2_ui_element::ToggleHelp()
{
    g_plugin.ToggleHelp();
}

void milk2_ui_element::TogglePlaylist()
{
    g_plugin.TogglePlaylist();
}

void milk2_ui_element::ToggleSongTitle()
{
    s_config.settings.m_bShowSongTitle = !s_config.settings.m_bShowSongTitle;
    g_plugin.m_bShowSongTitle = s_config.settings.m_bShowSongTitle;
}

void milk2_ui_element::ToggleSongLength()
{
    if (s_config.settings.m_bShowSongTime && s_config.settings.m_bShowSongLen)
    {
        s_config.settings.m_bShowSongTime = false;
        s_config.settings.m_bShowSongLen = false;
    }
    else if (s_config.settings.m_bShowSongTime && !s_config.settings.m_bShowSongLen)
    {
        s_config.settings.m_bShowSongLen = true;
    }
    else
    {
        s_config.settings.m_bShowSongTime = true;
        s_config.settings.m_bShowSongLen = false;
    }
    g_plugin.m_bShowSongTime = s_config.settings.m_bShowSongTime;
    g_plugin.m_bShowSongLen = s_config.settings.m_bShowSongLen;
}

void milk2_ui_element::TogglePresetInfo()
{
    s_config.settings.m_bShowPresetInfo = !s_config.settings.m_bShowPresetInfo;
    g_plugin.m_bShowPresetInfo = s_config.settings.m_bShowPresetInfo;
}

void milk2_ui_element::ToggleFps()
{
    s_config.settings.m_bShowFPS = !s_config.settings.m_bShowFPS;
    g_plugin.m_bShowFPS = s_config.settings.m_bShowFPS;
}

void milk2_ui_element::ToggleRating()
{
    s_config.settings.m_bShowRating = !s_config.settings.m_bShowRating;
    g_plugin.m_bShowRating = s_config.settings.m_bShowRating;
}

void milk2_ui_element::ToggleShaderHelp()
{
    s_config.settings.m_bShowShaderHelp = !s_config.settings.m_bShowShaderHelp;
    g_plugin.m_bShowShaderHelp = s_config.settings.m_bShowShaderHelp;
}

const char* milk2_ui_element::ToggleShuffle(bool forward = true)
{
    auto api = playlist_manager::get();
    size_t nModeCount = api->playback_order_get_count();
    size_t nCurrentMode = api->playback_order_get_active();
    size_t nNewMode = (forward ? ++nCurrentMode % nModeCount : (nCurrentMode == 0 ? nModeCount - 1 : --nCurrentMode));
    api->playback_order_set_active(nNewMode);
    return api->playback_order_get_name(nNewMode);
}

void milk2_ui_element::NextPreset(float fBlendTime)
{
    g_plugin.NextPreset(fBlendTime);
}

void milk2_ui_element::PrevPreset(float fBlendTime)
{
    g_plugin.PrevPreset(fBlendTime);
}

bool milk2_ui_element::LoadPreset(int select)
{
#if 0
    g_plugin.m_nCurrentPreset = select + g_plugin.m_nDirs;

    wchar_t szFile[MAX_PATH] = {0};
    wcscpy_s(szFile, g_plugin.m_szPresetDir); // Note: m_szPresetDir always ends with '\'
    wcscat_s(szFile, g_plugin.m_presets[g_plugin.m_nCurrentPreset].szFilename.c_str());

    g_plugin.LoadPreset(szFile, 1.0f);
#else
    UNREFERENCED_PARAMETER(select);
#endif
    return true;
}

std::wstring milk2_ui_element::GetCurrentPreset()
{
#ifdef _DEBUG
    wchar_t buf[512]{};
    swprintf_s(buf, L"%s", (g_plugin.m_nLoadingPreset != 0) ? g_plugin.m_pNewState->m_szDesc : g_plugin.m_pState->m_szDesc);
    wcscat_s(buf, L".milk");
    if (g_plugin.m_presets.size() == 0 || g_plugin.m_nCurrentPreset == -1)
    {
        MILK2_CONSOLE_LOG("No presets found")
    }
    else if (wcscmp(buf, g_plugin.m_presets[g_plugin.m_nCurrentPreset].szFilename.c_str()))
    {
        MILK2_CONSOLE_LOG("GetCurrentPreset Mismatch --> ", buf, " != ", g_plugin.m_presets[g_plugin.m_nCurrentPreset].szFilename.c_str())
    }
#endif
    return g_plugin.m_nCurrentPreset != -1 ? g_plugin.m_presets[g_plugin.m_nCurrentPreset].szFilename : std::wstring();
}

void milk2_ui_element::LockPreset(bool lockUnlock)
{
    g_plugin.m_bPresetLockedByUser = lockUnlock;
}

bool milk2_ui_element::IsPresetLock()
{
    return g_plugin.m_bPresetLockedByUser || g_plugin.m_bPresetLockedByCode;
}

void milk2_ui_element::RandomPreset(float fBlendTime)
{
    g_plugin.LoadRandomPreset(fBlendTime);
}

void milk2_ui_element::SetPresetRating(float inc_dec)
{
    g_plugin.SetCurrentPresetRating(g_plugin.m_pState->m_fRating + inc_dec);
}

void milk2_ui_element::Seek(UINT nRepCnt, bool bShiftHeldDown, double seekDelta)
{
    int reps = (bShiftHeldDown) ? 6 * nRepCnt : 1 * nRepCnt;
    if (seekDelta > 0.0)
    {
        if (!m_playback_control->playback_can_seek())
        {
            m_playback_control->next();
            return;
        }
    }
    else
    {
        if (!m_playback_control->playback_can_seek())
        {
            m_playback_control->previous();
            return;
        }
        double p = m_playback_control->playback_get_position();
        if (p < 0.0)
            return;
        if (p < (seekDelta * -1.0))
        {
            if (p < (seekDelta * -1.0) / 3.0)
            {
                if (m_playback_control->is_playing() && p > (seekDelta * -1.0))
                    m_playback_control->playback_seek(0.0);
                else
                    m_playback_control->previous();
                return;
            }
            else
            {
                m_playback_control->playback_seek(0.0);
                return;
            }
        }
    }
    for (int i = 0; i < reps; ++i)
        m_playback_control->playback_seek_delta(seekDelta);
}

// clang-format off
void milk2_ui_element::UpdateChannelMode()
{
    if (m_vis_stream.is_valid())
    {
        m_vis_stream->set_channel_mode(s_config.settings.m_bEnableDownmix ? visualisation_stream_v3::channel_mode_mono : visualisation_stream_v3::channel_mode_default);
    }
}
// clang-format on

void milk2_ui_element::UpdateTrack()
{
    if (m_script.is_empty())
    {
        pfc::string8 pattern = pfc::utf8FromWide(s_config.settings.m_szTitleFormat);
        static_api_ptr_t<titleformat_compiler>()->compile_safe_ex(m_script, pattern);
    }

    if (m_playback_control->playback_format_title(NULL, m_state, m_script, NULL, playback_control::display_level_all))
    {
        // Succeeded already.
    }
    else if (m_playback_control->is_playing())
    {
        // Starting playback but not done opening the first track yet.
        m_state = "Opening...";
    }
    else
    {
        m_state = "Stopped.";
    }
}

void milk2_ui_element::UpdateTrack(metadb_handle_ptr p_track)
{
    UpdateTrack();

    if (!p_track.is_valid())
        return;

    // Load the album art.
    if (wcsnlen_s(s_config.settings.m_szArtworkFormat, 256) != 0)
    {
        titleformat_object::ptr script;
        pfc::string8 pattern = pfc::utf8FromWide(s_config.settings.m_szArtworkFormat);
        bool success = titleformat_compiler::get()->compile(script, pattern);

        pfc::string result;
        if (success && script.is_valid() && p_track->format_title(nullptr, result, script, nullptr))
        {
            m_art_file.clear();
            std::vector<uint8_t> empty;
            m_raster.swap(empty);
            m_art_file = pfc::wideFromUTF8(result).c_str();
            m_raster.clear();
        }
    }
    else
    {
        LoadAlbumArt(p_track, fb2k::noAbort);
        ShowAlbumArt();
    }
}

void milk2_ui_element::ShowAlbumArt()
{
    // Kill all existing sprites.
    for (int x = 0; x < NUM_TEX; x++)
        g_plugin.KillSprite(x);

    if (s_config.settings.m_bShowAlbum)
    {
        if (!m_art_file.empty()) // file
        {
            // Check if file exists.
            pfc::string8 artFile = pfc::utf8FromWide(m_art_file.c_str());
            if (filesystem::g_exists(artFile, fb2k::noAbort))
            {
                return;
            }

            g_plugin.LaunchSprite(100, -1, m_art_file);
        }
        else if (m_raster.size() > 0) // memory
        {
            g_plugin.LaunchSprite(100, -1, L"", m_raster);
        }
        else // nothing
        {
            return;
        }
    }
}

void milk2_ui_element::UpdatePlaylist()
{
    auto api = playlist_manager::get();
    size_t total = api->activeplaylist_get_item_count();
    for (size_t i = 0; i < total; ++i)
    {
        if (api->activeplaylist_is_item_selected(i))
        {
            g_plugin.m_playlist_pos = i;
        }
    }
    g_plugin.m_playlist_top_idx = -1;
}

void milk2_ui_element::LaunchSongTitle()
{
    g_plugin.LaunchSongTitleAnim();
}

#ifdef TIMER_TP
// Starts the timer.
void milk2_ui_element::StartTimer() noexcept
{
    MILK2_CONSOLE_LOG_LIMIT("StartTimer ", GetWnd())
    if (m_tpTimer == nullptr)
        return;

    FILETIME DueTime{};
    DWORD RefreshInterval = static_cast<DWORD>(lround(1000.0f / s_config.settings.m_max_fps_fs));
    SetThreadpoolTimer(m_tpTimer, &DueTime, RefreshInterval, 0);
}

// Stops the timer.
void milk2_ui_element::StopTimer() noexcept
{
    MILK2_CONSOLE_LOG_LIMIT("StopTimer ", GetWnd())
    if (m_tpTimer == nullptr)
        return;

    SetThreadpoolTimer(m_tpTimer, NULL, 0, 0);
    WaitForThreadpoolTimerCallbacks(m_tpTimer, TRUE);
    CloseThreadpoolTimer(m_tpTimer);
}

// Handles a timer tick.
VOID CALLBACK milk2_ui_element::TimerCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer) noexcept
{
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Timer);

    ((milk2_ui_element*)Context)->Tick();
}
#endif

#ifdef TIMER_DX
bool milk2_ui_element::on_idle()
{
    Tick();
    //MILK2_CONSOLE_LOG("on_idle ", GetWnd())
    return true;
}
#endif

// Sets and unsets foobar2000's "Always on Top" setting (if main window is
// `TOPMOST`) so that visualization window becomes `TOPMOST` on entering
// fullscreen.
// Note: Using `SetWindowPos` does not work because foobar2000 appears to
//       reset itself to `TOPMOST` forcefully when this setting is enabled.
//       So unsetting and resetting the program's setting is a workaround.
bool milk2_ui_element::SetTopMost() noexcept
{
    LONG_PTR ptr = ::GetWindowLongPtr(core_api::get_main_window(), GWL_EXSTYLE);
    bool topmost = static_cast<bool>(ptr & WS_EX_TOPMOST);
    if (topmost)
    {
        if (s_fullscreen)
        {
            MILK2_CONSOLE_LOG("SetTopMost unsetting main window ", GetWnd())
            standard_commands::main_always_on_top(); //::SetWindowPos(get_wnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW /*| SWP_NOZORDER | SWP_FRAMECHANGED*/);
            s_was_topmost = true;
            return true;
        }
    }
    else
    {
        if (s_was_topmost)
        {
            MILK2_CONSOLE_LOG("SetTopMost resetting main window ", GetWnd())
            s_was_topmost = false;
            standard_commands::main_always_on_top(); //::SetWindowPos(get_wnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW /*| SWP_NOZORDER | SWP_FRAMECHANGED*/);
            return true;
        }
    }
    return false;
}

void milk2_ui_element::ShowPreferencesPage()
{
    ui_control::get()->show_preferences(guid_milk2_preferences);
}

void milk2_ui_element::SetSelectionSingle(size_t idx)
{
    SetSelectionSingle(idx, false, true, true);
}

void milk2_ui_element::SetSelectionSingle(size_t idx, bool toggle, bool focus, bool single_only)
{
    auto api = playlist_manager::get();
    const size_t total = api->activeplaylist_get_item_count();
    const size_t idx_focus = api->activeplaylist_get_focus_item();
    //if (idx_focus == pfc::infinite_size)
    //    return;

    bit_array_bittable mask(total);
    mask.set(idx, toggle ? !api->activeplaylist_is_item_selected(idx) : true);

    if (single_only || toggle || !api->activeplaylist_is_item_selected(idx))
    {
        pfc::bit_array_true baT;
        pfc::bit_array_one baO(idx);
        api->activeplaylist_set_selection(single_only ? (pfc::bit_array&)baT : (pfc::bit_array&)baO, mask);
    }
    if (focus && idx_focus != idx)
        api->activeplaylist_set_focus_item(idx);
}

// Resolves PWD, taking care of the case where the path contains non-ASCII
// characters.
void milk2_ui_element::ResolvePwd()
{
    // Get profile directory path through foobar2000 API.
    pfc::string8 full_path = filesystem::g_get_native_path(core_api::get_my_full_path());
    size_t t = full_path.lastIndexOf(L'\\');
    if (t != SIZE_MAX)
        full_path = full_path.subString(0, t + 1);
    size_t path_length = full_path.get_length();
    std::wstring base_path(path_length + 1, L'\0');
    path_length = pfc::stringcvt::convert_utf8_to_wide(const_cast<wchar_t*>(base_path.c_str()), base_path.size(), full_path.get_ptr(), path_length);
    base_path = base_path.erase(base_path.find(L'\0'));

#if 0
    // Get PWD path through Win32 API.
    wchar_t path[MAX_PATH];
    HMODULE hm = NULL;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          (LPCWSTR)&TEXT(APPLICATION_FILE_NAME),
                          &hm) == 0)
    {
        DWORD ret = GetLastError();
        MILK2_CONSOLE_LOG("GetModuleHandleEx failed, error = %d\n", ret);
    }
    if (GetModuleFileName(hm, path, MAX_PATH) == 0)
    {
        DWORD ret = GetLastError();
        MILK2_CONSOLE_LOG("GetModuleFileName failed, error = %d\n", ret);
    }
    std::wstring paths(path);
    size_t p = paths.rfind(L'\\');
    if (p != std::wstring::npos)
        paths.erase(p + 1);

    // Use Win32 string it mismatches with the foobar2000 string.
    if (paths != base_path)
    {
        s_pwd = paths;
    }
    else
    {
        s_pwd = base_path;
    }
#else
    s_pwd = base_path;
#endif
}

// Retrieves image raster data and clears the file path.
void milk2_ui_element::ExtractRasterData(const uint8_t* data, size_t size) noexcept
{
    m_art_file.clear();
    std::vector<uint8_t> empty;
    m_raster.swap(empty);
    if ((data != nullptr) && (size != 0))
    {
        m_raster.assign(data, data + size);
    }
}

// Registers with the album art notifier.
void milk2_ui_element::RegisterForArtwork()
{
#if 0
    // Register with the album art notification manager.
    auto AlbumArtNotificationManager = now_playing_album_art_notify_manager_v2::tryGet();

    if (AlbumArtNotificationManager.is_valid())
        AlbumArtNotificationManager->add(this);

    // Get the artwork data from the album art.
    if (wcsnlen_s(s_config.settings.m_szArtworkFormat, 256) == 0)
    {
        auto aanm = now_playing_album_art_notify_manager_v2::get();

        if (aanm != nullptr)
        {
            album_art_data_ptr aad = aanm->current();

            if (aad.is_valid())
            {
                ExtractRasterData(static_cast<const uint8_t*>(aad->data()), aad->size());
            }
        }
    }
#endif
}

// Loads embedded album art.
void milk2_ui_element::LoadAlbumArt(const metadb_handle_ptr& track, abort_callback& abort)
{
    static_api_ptr_t<album_art_manager_v2> aam;

    auto extractor = aam->open(pfc::list_single_ref_t(track), pfc::list_single_ref_t(album_art_ids::cover_front), abort);

    try
    {
        auto aad = extractor->query(album_art_ids::cover_front, abort);

        if (aad.is_valid())
        {
            ExtractRasterData(static_cast<const uint8_t*>(aad->data()), aad->size());
        }
    }
    catch (const exception_album_art_not_found&)
    {
        return;
    }
    catch (const exception_aborted&)
    {
        throw;
    }
    catch (...)
    {
        return;
    }
}

// Service factory publishes the class.
static service_factory_single_t<ui_element_milk2> g_ui_element_milk2_factory;
#pragma endregion

#pragma region Initialize/Quit
class milk2_initquit : public initquit
{
  public:
    void on_init()
    {
        MILK2_CONSOLE_LOG("on_init")
        if (core_api::is_quiet_mode_enabled())
            return;
        //create_first_run();
    }

    void on_quit()
    {
        MILK2_CONSOLE_LOG("on_quit")
        //NSEEL_quit();
        if (core_api::is_quiet_mode_enabled())
            return;
        //delete_first_run();
    }

    void create_first_run()
    {
        pfc::string8 milkdrop2_path = filesystem::g_get_native_path(core_api::get_profile_path());
        milkdrop2_path.end_with_slash();
        milkdrop2_path.add_string("milkdrop2\\", 10);
        if (!filesystem::g_exists(milkdrop2_path, fb2k::noAbort))
        {
            filesystem::g_create_directory(milkdrop2_path, fb2k::noAbort);
        }
        pfc::string8 presets_path = milkdrop2_path;
        presets_path.add_string("presets\\", 8);
        if (!filesystem::g_exists(presets_path, fb2k::noAbort))
        {
            filesystem::g_create_directory(presets_path, fb2k::noAbort);
        }
        if (filesystem::g_is_empty_directory(presets_path, fb2k::noAbort))
        {
            pfc::string8 preset_file = presets_path;
            preset_file.add_string("!.milk", 6);
            wchar_t szPresetFile[MAX_PATH];
            pfc::stringcvt::convert_utf8_to_wide(szPresetFile, MAX_PATH, preset_file, preset_file.length());
            char data_buffer[] =
                "MILKDROP_PRESET_VERSION=201\r\nPSVERSION=3\r\nPSVERSION_WARP=3\r\nPSVERSION_COMP=3\r\n[preset00]\r\nfRating=1.000\r\n";
            WriteMilkDropFile(szPresetFile, data_buffer, 106);
        }
    }

    void delete_first_run()
    {
        pfc::string8 preset_file = filesystem::g_get_native_path(core_api::get_profile_path());
        preset_file.end_with_slash();
        preset_file.add_string("milkdrop2\\presets\\!.milk", 24);
        if (filesystem::g_exists(preset_file, fb2k::noAbort))
        {
            filesystem::g_remove(preset_file, fb2k::noAbort);
        }
    }

    void WriteMilkDropFile(LPCWSTR szFile, LPCSTR szDataBuffer, CONST SIZE_T nDataSize)
    {
        HANDLE hFile;
        DWORD dwBytesToWrite = (DWORD)strnlen_s(szDataBuffer, nDataSize);
        DWORD dwBytesWritten = 0;
        BOOL bErrorFlag = FALSE;
        hFile = CreateFile(szFile, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            DisplayError(TEXT("CreateFile"));
            wprintf_s(TEXT("Terminal failure: Unable to open file \"%s\" for write.\n"), szFile);
            wchar_t buf[512] = {0}, title[64] = {0};
            swprintf_s(buf, L"Unable to open `%ls` for writing.", szFile);
            MessageBox(NULL, buf, WASABI_API_LNGSTRINGW_BUF(IDS_MILKDROP_ERROR, title, 64), MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
            return;
        }

        wprintf_s(TEXT("Writing %d bytes to %s.\n"), dwBytesToWrite, szFile);

        bErrorFlag = WriteFile(hFile, szDataBuffer, dwBytesToWrite, &dwBytesWritten, NULL);

        if (FALSE == bErrorFlag)
        {
            DisplayError(TEXT("WriteFile"));
            wprintf_s(TEXT("Terminal failure: Unable to write to file.\n"));
        }
        else
        {
            // This is an error because a synchronous write that results in
            // success (`WriteFile()` returns `TRUE`) should write all data as
            // requested. This would not necessarily be the case for
            // asynchronous writes.
            if (dwBytesWritten != dwBytesToWrite)
            {
                wprintf_s(TEXT("Error: dwBytesWritten != dwBytesToWrite.\n"));
            }
            else
            {
                wprintf_s(TEXT("Wrote %d bytes to %s successfully.\n"), dwBytesWritten, szFile);
            }
        }

        CloseHandle(hFile);
    }
};

FB2K_SERVICE_FACTORY(milk2_initquit);
#pragma endregion

#pragma region Preset Browser and Favorites

void milk2_ui_element::InitPresetBrowser()
{
    if (m_browserInitialized)
        return;

    // Initialize favorites and config file in the profile directory.
    std::wstring profileDir = s_config.settings.m_szPluginsDirPath;
    if (!profileDir.empty() && profileDir.back() != L'\\')
        profileDir += L'\\';

    std::wstring favPath = profileDir + L"favorites.ini";
    m_favorites.Initialize(favPath);
    m_favorites.Load();

    // Load browser config (shuffle mode, selected categories, preset library path).
    LoadBrowserConfig();

    // Determine the preset root directory.
    // Use the configured preset library path if set, otherwise use MilkDrop's preset dir.
    std::wstring scanDir = m_presetLibraryPath;
    if (scanDir.empty())
        scanDir = g_plugin.GetPresetDir();
    if (scanDir.empty())
        return;

    m_presetBrowser.ScanDirectory(scanDir);
    m_browserInitialized = true;

    // On first run (never configured), select all discovered categories.
    if (!m_categoriesConfigured)
    {
        auto cats = m_presetBrowser.GetCategories();
        for (const auto& c : cats)
            m_selectedCategories.insert(c);
        m_categoriesConfigured = true;
        SaveBrowserConfig();
    }

    // Push filter state to the engine.
    // Only trigger a rescan if the engine's filter state differs from ours.
    // On first init, the engine already did a full unfiltered scan in AllocateMilkDropDX11,
    // so we only need to rescan if we're actually filtering.
    if (m_categoriesConfigured && m_selectedCategories.empty())
    {
        // User explicitly deselected everything — use a sentinel that matches nothing.
        g_plugin.m_filterCategories.clear();
        g_plugin.m_filterCategories.insert(L"__NONE__");
    }
    else
    {
        g_plugin.m_filterCategories = m_selectedCategories;
    }
    g_plugin.m_bFilterFavoritesOnly = (m_shuffleMode == ShuffleMode::Favorites || m_shuffleMode == ShuffleMode::FavoritesCategory);
    if (g_plugin.m_bFilterFavoritesOnly)
    {
        auto favs = m_favorites.GetAll();
        for (const auto& fav : favs)
            g_plugin.m_filterFavorites.insert(fav.relativePath);
    }
    // Only rescan if we have active filters that differ from the initial unfiltered scan.
    bool needsRescan = g_plugin.m_bFilterFavoritesOnly ||
                       (!m_selectedCategories.empty() && m_selectedCategories.size() < m_presetBrowser.GetCategoryCount());
    if (needsRescan && g_plugin.m_bPresetListReady)
    {
        g_plugin.UpdatePresetList(true, true); // background + force
    }

    FB2K_console_print(core_api::get_my_file_name(), ": Preset browser initialized - ",
                       m_presetBrowser.GetPresetCount(), " presets in ",
                       m_presetBrowser.GetCategoryCount(), " categories, ",
                       m_favorites.Count(), " favorites");
}

void milk2_ui_element::SyncFiltersToEngine()
{
    // Push category selections to engine.
    g_plugin.m_filterCategories = m_selectedCategories;

    // Push favorites list to engine.
    g_plugin.m_filterFavorites.clear();
    if (m_shuffleMode == ShuffleMode::Favorites || m_shuffleMode == ShuffleMode::FavoritesCategory)
    {
        auto favs = m_favorites.GetAll();
        for (const auto& fav : favs)
            g_plugin.m_filterFavorites.insert(fav.relativePath);
        g_plugin.m_bFilterFavoritesOnly = true;
    }
    else
    {
        g_plugin.m_bFilterFavoritesOnly = false;
    }

    // Trigger a background rescan of the preset list with the new filters.
    // Must be non-blocking to avoid race conditions with the render timer thread.
    g_plugin.UpdatePresetList(true, true); // background + force
}

std::wstring milk2_ui_element::GetCurrentPresetRelativePath() const
{
    std::wstring currentFile = g_plugin.m_szCurrentPresetFile;
    if (currentFile.empty())
        return L"";

    // m_szCurrentPresetFile contains the full path (e.g. C:\...\presets\Fractal\Blobby\file.milk).
    // Strip the preset directory prefix to get the relative path.
    if (m_presetBrowser.IsReady())
        return m_presetBrowser.MakeRelativePath(currentFile);

    // Fallback: strip preset dir manually.
    std::wstring presetDir = g_plugin.GetPresetDir();
    if (!presetDir.empty() && _wcsnicmp(currentFile.c_str(), presetDir.c_str(), presetDir.length()) == 0)
        return currentFile.substr(presetDir.length());

    return currentFile;
}

void milk2_ui_element::ToggleFavorite()
{
    InitPresetBrowser();
    std::wstring relPath = GetCurrentPresetRelativePath();
    if (relPath.empty())
        return;

    bool nowFav = m_favorites.Toggle(relPath);
    m_favorites.Save();

    // Show a notification.
    wchar_t buf[512];
    if (nowFav)
        swprintf_s(buf, L"Added to favorites: %s", relPath.c_str());
    else
        swprintf_s(buf, L"Removed from favorites: %s", relPath.c_str());
    g_plugin.AddError(buf, 2.0f, ERR_NOTIFY, false);
}

void milk2_ui_element::SetShuffleMode(ShuffleMode mode)
{
    m_shuffleMode = mode;
    SaveBrowserConfig();
    SyncFiltersToEngine();

    const wchar_t* modeStr = L"All Presets";
    switch (mode)
    {
        case ShuffleMode::All:
            modeStr = L"All Presets";
            break;
        case ShuffleMode::Category:
        {
            static wchar_t catBuf[128];
            swprintf_s(catBuf, L"Selected Categories (%zu)", m_selectedCategories.size());
            modeStr = catBuf;
            break;
        }
        case ShuffleMode::Favorites:
            modeStr = L"All Favorites";
            break;
        case ShuffleMode::FavoritesCategory:
        {
            static wchar_t favCatBuf[128];
            swprintf_s(favCatBuf, L"Favorites in Selected Categories (%zu)", m_selectedFavCategories.size());
            modeStr = favCatBuf;
            break;
        }
    }

    wchar_t buf[256];
    swprintf_s(buf, L"Shuffle mode: %s", modeStr);
    g_plugin.AddError(buf, 2.0f, ERR_NOTIFY, false);
}

void milk2_ui_element::ToggleCategorySelection(const std::wstring& category)
{
    auto it = m_selectedCategories.find(category);
    if (it != m_selectedCategories.end())
        m_selectedCategories.erase(it);
    else
        m_selectedCategories.insert(category);
    SaveBrowserConfig();
    SyncFiltersToEngine();
}

void milk2_ui_element::SelectAllCategories()
{
    auto cats = m_presetBrowser.GetCategories();
    m_selectedCategories.clear();
    for (const auto& c : cats)
        m_selectedCategories.insert(c);
    SaveBrowserConfig();
    SyncFiltersToEngine();
}

void milk2_ui_element::DeselectAllCategories()
{
    m_selectedCategories.clear();
    SaveBrowserConfig();
    SyncFiltersToEngine();
}

void milk2_ui_element::LoadRandomPresetFromBrowser()
{
    InitPresetBrowser();
    if (!m_presetBrowser.IsReady())
    {
        RandomPreset();
        return;
    }

    BrowsablePreset preset;
    switch (m_shuffleMode)
    {
        case ShuffleMode::All:
            preset = m_presetBrowser.GetRandomPreset();
            break;

        case ShuffleMode::Category:
            if (m_selectedCategories.empty())
            {
                preset = m_presetBrowser.GetRandomPreset();
            }
            else
            {
                preset = m_presetBrowser.GetRandomPresetInCategories(m_selectedCategories);
            }
            break;

        case ShuffleMode::Favorites:
        {
            auto favs = m_favorites.GetAll();
            if (favs.empty())
            {
                g_plugin.AddError(const_cast<wchar_t*>(L"No favorites set. Add presets to favorites first (Ctrl+F)."), 3.0f, ERR_NOTIFY, false);
                return;
            }
            int idx = rand() % static_cast<int>(favs.size());
            std::wstring fullPath = m_presetBrowser.GetRootDir() + favs[idx].relativePath;
            g_plugin.LoadPreset(fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
            return;
        }

        case ShuffleMode::FavoritesCategory:
        {
            auto favs = m_selectedFavCategories.empty() ? m_favorites.GetAll() : m_favorites.GetByCategories(m_selectedFavCategories);
            if (favs.empty())
            {
                g_plugin.AddError(const_cast<wchar_t*>(L"No favorites in selected categories."), 3.0f, ERR_NOTIFY, false);
                return;
            }
            int idx = rand() % static_cast<int>(favs.size());
            std::wstring fullPath = m_presetBrowser.GetRootDir() + favs[idx].relativePath;
            g_plugin.LoadPreset(fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
            return;
        }
    }

    if (!preset.fullPath.empty())
    {
        g_plugin.LoadPreset(preset.fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
    }
}

void milk2_ui_element::BuildShuffleModeSubmenu(CMenu& shuffleMenu)
{
    // Top-level mode selectors (radio-style).
    shuffleMenu.AppendMenu(MF_STRING | (m_shuffleMode == ShuffleMode::All ? MF_CHECKED : 0),
                           IDM_SHUFFLE_MODE_ALL, TEXT("All Presets"));
    shuffleMenu.AppendMenu(MF_STRING | (m_shuffleMode == ShuffleMode::Category ? MF_CHECKED : 0),
                           IDM_SHUFFLE_MODE_CATEGORY, TEXT("Selected Categories"));
    shuffleMenu.AppendMenu(MF_STRING | (m_shuffleMode == ShuffleMode::Favorites ? MF_CHECKED : 0),
                           IDM_SHUFFLE_MODE_FAVORITES, TEXT("All Favorites"));
    shuffleMenu.AppendMenu(MF_STRING | (m_shuffleMode == ShuffleMode::FavoritesCategory ? MF_CHECKED : 0),
                           IDM_SHUFFLE_MODE_FAV_CATEGORY, TEXT("Favorites in Selected Categories"));

    shuffleMenu.AppendMenu(MF_SEPARATOR);

    // Category multi-select submenu (checkboxes).
    if (m_presetBrowser.IsReady())
    {
        auto categories = m_presetBrowser.GetCategories();
        if (!categories.empty())
        {
            CMenu catSubMenu;
            catSubMenu.CreatePopupMenu();

            // Select All / Deselect All at the top.
            catSubMenu.AppendMenu(MF_STRING, IDM_SELECT_ALL_CATEGORIES, TEXT("Select All"));
            catSubMenu.AppendMenu(MF_STRING, IDM_DESELECT_ALL_CATEGORIES, TEXT("Deselect All"));
            catSubMenu.AppendMenu(MF_SEPARATOR);

            for (int i = 0; i < static_cast<int>(categories.size()) && i < 290; i++)
            {
                bool selected = m_selectedCategories.count(categories[i]) > 0;
                auto presets = m_presetBrowser.GetPresetsByCategory(categories[i]);
                wchar_t label[256];
                swprintf_s(label, L"%s (%zu)", categories[i].c_str(), presets.size());
                catSubMenu.AppendMenu(MF_STRING | (selected ? MF_CHECKED : 0),
                                      IDM_SHUFFLE_CATEGORY_BASE + i, label);
            }

            wchar_t catLabel[64];
            swprintf_s(catLabel, L"Categories (%zu/%zu selected)",
                       m_selectedCategories.size(), categories.size());
            shuffleMenu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(catSubMenu.Detach()), catLabel);
        }
    }

    // Favorites-by-category multi-select submenu.
    auto favCategories = m_favorites.GetFavoriteCategories();
    if (!favCategories.empty())
    {
        CMenu favCatSubMenu;
        favCatSubMenu.CreatePopupMenu();

        favCatSubMenu.AppendMenu(MF_STRING, IDM_SELECT_ALL_FAV_CATEGORIES, TEXT("Select All"));
        favCatSubMenu.AppendMenu(MF_STRING, IDM_DESELECT_ALL_FAV_CATEGORIES, TEXT("Deselect All"));
        favCatSubMenu.AppendMenu(MF_SEPARATOR);

        for (int i = 0; i < static_cast<int>(favCategories.size()) && i < 490; i++)
        {
            bool selected = m_selectedFavCategories.count(favCategories[i]) > 0;
            auto favs = m_favorites.GetByCategory(favCategories[i]);
            wchar_t label[256];
            swprintf_s(label, L"%s (%zu)", favCategories[i].c_str(), favs.size());
            favCatSubMenu.AppendMenu(MF_STRING | (selected ? MF_CHECKED : 0),
                                     IDM_SHUFFLE_FAV_CATEGORY_BASE + i, label);
        }

        wchar_t favCatLabel[64];
        swprintf_s(favCatLabel, L"Favorite Categories (%zu/%zu selected)",
                   m_selectedFavCategories.size(), favCategories.size());
        shuffleMenu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(favCatSubMenu.Detach()), favCatLabel);
    }
}

void milk2_ui_element::BuildBrowseSubmenu(CMenu& browseMenu, int& /*nextId*/)
{
    if (!m_presetBrowser.IsReady())
    {
        browseMenu.AppendMenu(MF_GRAYED, static_cast<UINT_PTR>(0), TEXT("(scanning...)"));
        return;
    }

    auto categories = m_presetBrowser.GetCategories();
    if (categories.empty())
    {
        browseMenu.AppendMenu(MF_GRAYED, static_cast<UINT_PTR>(0), TEXT("(no categories found)"));
        return;
    }

    for (int i = 0; i < static_cast<int>(categories.size()) && i < 200; i++)
    {
        auto presets = m_presetBrowser.GetPresetsByCategory(categories[i]);
        wchar_t label[256];
        swprintf_s(label, L"%s (%zu)", categories[i].c_str(), presets.size());
        browseMenu.AppendMenu(MF_STRING, IDM_BROWSE_CATEGORY_BASE + i, label);
    }

    // Show count of uncategorized presets (in root).
    size_t rootCount = 0;
    auto all = m_presetBrowser.GetAllPresets();
    for (const auto& p : all)
    {
        if (p.category.empty())
            rootCount++;
    }
    if (rootCount > 0)
    {
        wchar_t label[256];
        swprintf_s(label, L"(Uncategorized) (%zu)", rootCount);
        browseMenu.AppendMenu(MF_GRAYED, static_cast<UINT_PTR>(0), label);
    }
}

void milk2_ui_element::BuildFavoritesSubmenu(CMenu& favMenu, int& /*nextId*/)
{
    auto favs = m_favorites.GetAll();
    if (favs.empty())
    {
        favMenu.AppendMenu(MF_GRAYED, static_cast<UINT_PTR>(0), TEXT("(no favorites yet - use Add to Favorites)"));
        return;
    }

    // Group by category.
    std::map<std::wstring, std::vector<std::pair<int, const FavoriteEntry*>>> grouped;
    for (int i = 0; i < static_cast<int>(favs.size()); i++)
    {
        std::wstring cat = favs[i].category.empty() ? L"(Uncategorized)" : favs[i].category;
        grouped[cat].push_back({i, &favs[i]});
    }

    // If only one group or few total favorites, show flat list with category prefix.
    if (grouped.size() <= 1 || favs.size() <= 20)
    {
        for (int i = 0; i < static_cast<int>(favs.size()); i++)
        {
            // Extract filename and category for display.
            std::wstring filename = favs[i].relativePath;
            size_t lastSep = filename.find_last_of(L'\\');
            if (lastSep != std::wstring::npos)
                filename = filename.substr(lastSep + 1);
            // Remove .milk extension.
            if (filename.length() > 5 && _wcsicmp(filename.c_str() + filename.length() - 5, L".milk") == 0)
                filename = filename.substr(0, filename.length() - 5);
            // Prepend category.
            std::wstring display;
            if (!favs[i].category.empty())
                display = L"[" + favs[i].category + L"] " + filename;
            else
                display = filename;
            favMenu.AppendMenu(MF_STRING, IDM_PRESET_BASE + i, display.c_str());
        }
    }
    else
    {
        // Show grouped by category in submenus.
        for (auto& [cat, entries] : grouped)
        {
            CMenu catSubMenu;
            catSubMenu.CreatePopupMenu();
            for (auto& [idx, entry] : entries)
            {
                std::wstring display = entry->relativePath;
                size_t lastSep = display.find_last_of(L'\\');
                if (lastSep != std::wstring::npos)
                    display = display.substr(lastSep + 1);
                if (display.length() > 5 && _wcsicmp(display.c_str() + display.length() - 5, L".milk") == 0)
                    display = display.substr(0, display.length() - 5);
                catSubMenu.AppendMenu(MF_STRING, IDM_PRESET_BASE + idx, display.c_str());
            }
            wchar_t label[256];
            swprintf_s(label, L"%s (%zu)", cat.c_str(), entries.size());
            favMenu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(catSubMenu.Detach()), label);
        }
    }
}

void milk2_ui_element::SaveBrowserConfig() const
{
    std::wstring profileDir = s_config.settings.m_szPluginsDirPath;
    if (!profileDir.empty() && profileDir.back() != L'\\')
        profileDir += L'\\';
    std::wstring configPath = profileDir + L"browser_config.ini";

    // Ensure directory exists.
    std::filesystem::path p(configPath);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    std::wofstream ofs(configPath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open())
        return;

    ofs << L"[BrowserConfig]" << std::endl;
    ofs << L"ShuffleMode=" << static_cast<int>(m_shuffleMode) << std::endl;
    ofs << L"PresetLibraryPath=" << m_presetLibraryPath << std::endl;
    ofs << L"CategoriesConfigured=" << (m_categoriesConfigured ? 1 : 0) << std::endl;

    // Save selected categories as a pipe-delimited list.
    ofs << L"SelectedCategories=";
    bool first = true;
    for (const auto& cat : m_selectedCategories)
    {
        if (!first) ofs << L"|";
        ofs << cat;
        first = false;
    }
    ofs << std::endl;

    // Save selected favorite categories.
    ofs << L"SelectedFavCategories=";
    first = true;
    for (const auto& cat : m_selectedFavCategories)
    {
        if (!first) ofs << L"|";
        ofs << cat;
        first = false;
    }
    ofs << std::endl;
}

void milk2_ui_element::LoadBrowserConfig()
{
    std::wstring profileDir = s_config.settings.m_szPluginsDirPath;
    if (!profileDir.empty() && profileDir.back() != L'\\')
        profileDir += L'\\';
    std::wstring configPath = profileDir + L"browser_config.ini";

    if (!std::filesystem::exists(configPath))
        return;

    std::wifstream ifs(configPath);
    if (!ifs.is_open())
        return;

    std::wstring line;
    while (std::getline(ifs, line))
    {
        if (line.empty() || line[0] == L'[' || line[0] == L';')
            continue;

        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos)
            continue;

        std::wstring key = line.substr(0, eq);
        std::wstring value = line.substr(eq + 1);

        // Trim trailing whitespace/CR.
        while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n' || value.back() == L' '))
            value.pop_back();

        if (key == L"ShuffleMode")
        {
            int mode = _wtoi(value.c_str());
            if (mode >= 0 && mode <= 3)
                m_shuffleMode = static_cast<ShuffleMode>(mode);
        }
        else if (key == L"PresetLibraryPath")
        {
            m_presetLibraryPath = value;
        }
        else if (key == L"CategoriesConfigured")
        {
            m_categoriesConfigured = (_wtoi(value.c_str()) != 0);
        }
        else if (key == L"SelectedCategories")
        {
            m_selectedCategories.clear();
            if (!value.empty())
            {
                size_t start = 0;
                size_t pipe;
                while ((pipe = value.find(L'|', start)) != std::wstring::npos)
                {
                    std::wstring cat = value.substr(start, pipe - start);
                    if (!cat.empty())
                        m_selectedCategories.insert(cat);
                    start = pipe + 1;
                }
                std::wstring last = value.substr(start);
                if (!last.empty())
                    m_selectedCategories.insert(last);
            }
        }
        else if (key == L"SelectedFavCategories")
        {
            m_selectedFavCategories.clear();
            if (!value.empty())
            {
                size_t start = 0;
                size_t pipe;
                while ((pipe = value.find(L'|', start)) != std::wstring::npos)
                {
                    std::wstring cat = value.substr(start, pipe - start);
                    if (!cat.empty())
                        m_selectedFavCategories.insert(cat);
                    start = pipe + 1;
                }
                std::wstring last = value.substr(start);
                if (!last.empty())
                    m_selectedFavCategories.insert(last);
            }
        }
    }
}

// ============================================================================
// Category Selector TreeView dialog
// ============================================================================

// Tri-state checkbox values (stored as state image indices, 1-based).
#define CBS_UNCHECKED 1
#define CBS_CHECKED   2
#define CBS_PARTIAL   3

struct CatSelState
{
    milk2_ui_element* self;
    HWND hTree;
    std::wstring rootDir;
    std::set<std::wstring> selected; // relative dir paths that are checked
};

static void CatSel_SetCheckState(HWND hTree, HTREEITEM hItem, int state)
{
    TVITEM tvi = {};
    tvi.mask = TVIF_HANDLE | TVIF_STATE;
    tvi.hItem = hItem;
    tvi.stateMask = TVIS_STATEIMAGEMASK;
    tvi.state = INDEXTOSTATEIMAGEMASK(state);
    TreeView_SetItem(hTree, &tvi);
}

static int CatSel_GetCheckState(HWND hTree, HTREEITEM hItem)
{
    TVITEM tvi = {};
    tvi.mask = TVIF_HANDLE | TVIF_STATE;
    tvi.hItem = hItem;
    tvi.stateMask = TVIS_STATEIMAGEMASK;
    TreeView_GetItem(hTree, &tvi);
    return (tvi.state >> 12) & 0xF;
}

// Check/uncheck all children recursively.
static void CatSel_SetChildrenState(HWND hTree, HTREEITEM hParent, int state)
{
    HTREEITEM hChild = TreeView_GetChild(hTree, hParent);
    while (hChild)
    {
        CatSel_SetCheckState(hTree, hChild, state);
        CatSel_SetChildrenState(hTree, hChild, state);
        hChild = TreeView_GetNextSibling(hTree, hChild);
    }
}

// Compute parent state from children.
static void CatSel_UpdateParentState(HWND hTree, HTREEITEM hItem)
{
    HTREEITEM hParent = TreeView_GetParent(hTree, hItem);
    if (!hParent)
        return;

    int numChecked = 0, numUnchecked = 0, numPartial = 0, total = 0;
    HTREEITEM hChild = TreeView_GetChild(hTree, hParent);
    while (hChild)
    {
        int st = CatSel_GetCheckState(hTree, hChild);
        if (st == CBS_CHECKED) numChecked++;
        else if (st == CBS_UNCHECKED) numUnchecked++;
        else numPartial++;
        total++;
        hChild = TreeView_GetNextSibling(hTree, hChild);
    }

    int parentState;
    if (numChecked == total)
        parentState = CBS_CHECKED;
    else if (numUnchecked == total)
        parentState = CBS_UNCHECKED;
    else
        parentState = CBS_PARTIAL;

    CatSel_SetCheckState(hTree, hParent, parentState);
    // Recurse up.
    CatSel_UpdateParentState(hTree, hParent);
}

// Toggle a node's check state on click.
static void CatSel_ToggleItem(HWND hTree, HTREEITEM hItem)
{
    int state = CatSel_GetCheckState(hTree, hItem);
    // If partial or checked, toggle to unchecked. If unchecked, toggle to checked.
    int newState = (state == CBS_UNCHECKED) ? CBS_CHECKED : CBS_UNCHECKED;
    CatSel_SetCheckState(hTree, hItem, newState);
    CatSel_SetChildrenState(hTree, hItem, newState);
    CatSel_UpdateParentState(hTree, hItem);
}

// Recursively collect checked leaf directories.
// Strip " (NNN)" suffix from tree item text to get the actual directory name.
static std::wstring CatSel_StripCount(const wchar_t* text)
{
    std::wstring s = text;
    size_t paren = s.rfind(L" (");
    if (paren != std::wstring::npos && s.back() == L')')
        s = s.substr(0, paren);
    return s;
}

static void CatSel_CollectChecked(HWND hTree, HTREEITEM hItem, const std::wstring& parentPath, std::set<std::wstring>& result)
{
    HTREEITEM hChild = TreeView_GetChild(hTree, hItem);

    // Get this item's text and strip the count suffix.
    wchar_t text[256] = {};
    TVITEM tvi = {};
    tvi.mask = TVIF_HANDLE | TVIF_TEXT;
    tvi.hItem = hItem;
    tvi.pszText = text;
    tvi.cchTextMax = 256;
    TreeView_GetItem(hTree, &tvi);

    std::wstring dirName = CatSel_StripCount(text);
    std::wstring thisPath = parentPath.empty() ? dirName : parentPath + L"\\" + dirName;
    int state = CatSel_GetCheckState(hTree, hItem);

    if (state == CBS_CHECKED)
    {
        // This dir and all subdirs are included. Add the top-level path.
        result.insert(thisPath);
        return; // No need to recurse — parent being checked means all children.
    }
    else if (state == CBS_PARTIAL)
    {
        // Some children checked — recurse.
        while (hChild)
        {
            CatSel_CollectChecked(hTree, hChild, thisPath, result);
            hChild = TreeView_GetNextSibling(hTree, hChild);
        }
    }
    // CBS_UNCHECKED: nothing to add.
}

// Populate the tree from the filesystem.
static void CatSel_PopulateTree(HWND hTree, HTREEITEM hParent, const std::wstring& dirPath,
                                const std::wstring& relativePath, const std::set<std::wstring>& selected,
                                bool forceChecked = false)
{
    // Enumerate subdirectories.
    try
    {
        std::vector<std::wstring> subdirs;
        for (const auto& entry : std::filesystem::directory_iterator(dirPath))
        {
            if (entry.is_directory())
                subdirs.push_back(entry.path().filename().wstring());
        }
        std::sort(subdirs.begin(), subdirs.end(), [](const std::wstring& a, const std::wstring& b) {
            return _wcsicmp(a.c_str(), b.c_str()) < 0;
        });

        for (const auto& subdir : subdirs)
        {
            std::wstring relPath = relativePath.empty() ? subdir : relativePath + L"\\" + subdir;
            std::wstring fullPath = dirPath + L"\\" + subdir;

            // Count .milk files in this dir (recursive).
            int milkCount = 0;
            try
            {
                for (const auto& f : std::filesystem::recursive_directory_iterator(
                         fullPath, std::filesystem::directory_options::skip_permission_denied))
                {
                    if (f.is_regular_file() && _wcsicmp(f.path().extension().c_str(), L".milk") == 0)
                        milkCount++;
                }
            }
            catch (...) {}

            wchar_t label[300];
            swprintf_s(label, L"%s (%d)", subdir.c_str(), milkCount);

            TVINSERTSTRUCT tvins = {};
            tvins.hParent = hParent;
            tvins.hInsertAfter = TVI_LAST;
            tvins.item.mask = TVIF_TEXT | TVIF_STATE;
            tvins.item.pszText = label;
            tvins.item.stateMask = TVIS_STATEIMAGEMASK;

            // Insert item first — TVS_CHECKBOXES ignores state during insert.
            tvins.item.mask = TVIF_TEXT;
            HTREEITEM hItem = TreeView_InsertItem(hTree, &tvins);

            // Determine this item's check state.
            bool isSelected = forceChecked;
            bool isParentOfSelected = false;
            if (!forceChecked)
            {
                for (const auto& sel : selected)
                {
                    if (_wcsicmp(sel.c_str(), relPath.c_str()) == 0)
                        isSelected = true;
                    if (_wcsnicmp(sel.c_str(), relPath.c_str(), relPath.length()) == 0 &&
                        sel.length() > relPath.length() && sel[relPath.length()] == L'\\')
                        isParentOfSelected = true;
                }
            }

            // Set check state. Pass forceChecked down to children.
            if (isSelected)
            {
                CatSel_SetCheckState(hTree, hItem, CBS_CHECKED);
                CatSel_PopulateTree(hTree, hItem, fullPath, relPath, selected, true);
            }
            else if (isParentOfSelected)
            {
                CatSel_PopulateTree(hTree, hItem, fullPath, relPath, selected, false);
                CatSel_SetCheckState(hTree, hItem, CBS_PARTIAL);
            }
            else
            {
                CatSel_SetCheckState(hTree, hItem, CBS_UNCHECKED);
                CatSel_PopulateTree(hTree, hItem, fullPath, relPath, selected, false);
            }
        }
    }
    catch (...) {}
}



static INT_PTR CALLBACK CatSelDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CatSelState* state = reinterpret_cast<CatSelState*>(GetWindowLongPtr(hdlg, GWLP_USERDATA));

    switch (msg)
    {
        case WM_INITDIALOG:
        {
            state = reinterpret_cast<CatSelState*>(lParam);
            SetWindowLongPtr(hdlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            RECT rc;
            GetClientRect(hdlg, &rc);

            // Create TreeView WITHOUT TVS_CHECKBOXES — we manage state images manually for tri-state.
            state->hTree = CreateWindowEx(0, WC_TREEVIEW, L"",
                                          WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES |
                                              TVS_LINESATROOT,
                                          10, 10, rc.right - 20, rc.bottom - 60,
                                          hdlg, reinterpret_cast<HMENU>(2001), core_api::get_my_instance(), NULL);

            // Create a state image list with 3 states: unchecked, checked, partial.
            // Index 0 = no image, 1 = unchecked, 2 = checked, 3 = partial.
            {
                int cx = GetSystemMetrics(SM_CXSMICON);
                int cy = GetSystemMetrics(SM_CYSMICON);
                HIMAGELIST hIml = ImageList_Create(cx, cy, ILC_COLOR32, 4, 0);

                for (int i = 0; i < 4; i++)
                {
                    HDC hdcScreen = GetDC(NULL);
                    HDC hdcMem = CreateCompatibleDC(hdcScreen);
                    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cx, cy);
                    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBmp);

                    // Fill background.
                    RECT rcBmp = {0, 0, cx, cy};
                    FillRect(hdcMem, &rcBmp, (HBRUSH)GetStockObject(WHITE_BRUSH));

                    if (i >= 1) // states 1, 2, 3
                    {
                        // Draw checkbox border.
                        RECT rcBox = {1, 1, cx - 1, cy - 1};
                        FrameRect(hdcMem, &rcBox, (HBRUSH)GetStockObject(BLACK_BRUSH));

                        if (i == 2) // checked
                        {
                            // Draw checkmark.
                            MoveToEx(hdcMem, 3, cy / 2, NULL);
                            LineTo(hdcMem, cx / 3, cy - 4);
                            LineTo(hdcMem, cx - 3, 3);
                            MoveToEx(hdcMem, 3, cy / 2 + 1, NULL);
                            LineTo(hdcMem, cx / 3, cy - 3);
                            LineTo(hdcMem, cx - 3, 4);
                        }
                        else if (i == 3) // partial
                        {
                            // Draw filled square inside.
                            RECT rcFill = {3, 3, cx - 3, cy - 3};
                            FillRect(hdcMem, &rcFill, (HBRUSH)GetStockObject(GRAY_BRUSH));
                        }
                    }

                    SelectObject(hdcMem, hOld);
                    ImageList_Add(hIml, hBmp, NULL);
                    DeleteObject(hBmp);
                    DeleteDC(hdcMem);
                    ReleaseDC(NULL, hdcScreen);
                }

                TreeView_SetImageList(state->hTree, hIml, TVSIL_STATE);
            }

            // Buttons.
            CreateWindow(L"BUTTON", L"Select All", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         10, rc.bottom - 42, 85, 30, hdlg, reinterpret_cast<HMENU>(3001), core_api::get_my_instance(), NULL);
            CreateWindow(L"BUTTON", L"Deselect All", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         105, rc.bottom - 42, 100, 30, hdlg, reinterpret_cast<HMENU>(3002), core_api::get_my_instance(), NULL);
            CreateWindow(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                         rc.right - 180, rc.bottom - 42, 80, 30, hdlg, reinterpret_cast<HMENU>(IDOK), core_api::get_my_instance(), NULL);
            CreateWindow(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         rc.right - 90, rc.bottom - 42, 80, 30, hdlg, reinterpret_cast<HMENU>(IDCANCEL), core_api::get_my_instance(), NULL);

            // Populate tree.
            CatSel_PopulateTree(state->hTree, TVI_ROOT, state->rootDir, L"", state->selected);

            // Expand top-level items.
            HTREEITEM hRoot = TreeView_GetRoot(state->hTree);
            while (hRoot)
            {
                TreeView_Expand(state->hTree, hRoot, TVE_EXPAND);
                hRoot = TreeView_GetNextSibling(state->hTree, hRoot);
            }

            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                {
                    // Collect checked paths.
                    state->selected.clear();
                    HTREEITEM hRoot = TreeView_GetRoot(state->hTree);
                    while (hRoot)
                    {
                        CatSel_CollectChecked(state->hTree, hRoot, L"", state->selected);
                        hRoot = TreeView_GetNextSibling(state->hTree, hRoot);
                    }
                    EndDialog(hdlg, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hdlg, IDCANCEL);
                    return TRUE;
                case 3001: // Select All
                {
                    HTREEITEM hRoot = TreeView_GetRoot(state->hTree);
                    while (hRoot)
                    {
                        CatSel_SetCheckState(state->hTree, hRoot, CBS_CHECKED);
                        CatSel_SetChildrenState(state->hTree, hRoot, CBS_CHECKED);
                        hRoot = TreeView_GetNextSibling(state->hTree, hRoot);
                    }
                    return TRUE;
                }
                case 3002: // Deselect All
                {
                    HTREEITEM hRoot = TreeView_GetRoot(state->hTree);
                    while (hRoot)
                    {
                        CatSel_SetCheckState(state->hTree, hRoot, CBS_UNCHECKED);
                        CatSel_SetChildrenState(state->hTree, hRoot, CBS_UNCHECKED);
                        hRoot = TreeView_GetNextSibling(state->hTree, hRoot);
                    }
                    return TRUE;
                }
            }
            break;

        case WM_NOTIFY:
        {
            NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == 2001 && nmhdr->code == NM_CLICK)
            {
                // Handle click on checkbox area — toggle directly (no TVS_CHECKBOXES).
                TVHITTESTINFO ht = {};
                DWORD pos = GetMessagePos();
                ht.pt.x = GET_X_LPARAM(pos);
                ht.pt.y = GET_Y_LPARAM(pos);
                ScreenToClient(state->hTree, &ht.pt);
                HTREEITEM hHit = TreeView_HitTest(state->hTree, &ht);
                if (hHit && (ht.flags & TVHT_ONITEMSTATEICON))
                {
                    CatSel_ToggleItem(state->hTree, hHit);
                }
                return 0;
            }
            if (nmhdr->idFrom == 2001 && nmhdr->code == TVN_KEYDOWN)
            {
                NMTVKEYDOWN* pKey = reinterpret_cast<NMTVKEYDOWN*>(lParam);
                if (pKey->wVKey == VK_SPACE)
                {
                    HTREEITEM hSel = TreeView_GetSelection(state->hTree);
                    if (hSel)
                        CatSel_ToggleItem(state->hTree, hSel);
                }
            }
            break;
        }

        case WM_SIZE:
        {
            if (state && state->hTree)
            {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                MoveWindow(state->hTree, 10, 10, w - 20, h - 60, TRUE);
            }
            break;
        }

        case WM_CLOSE:
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

void milk2_ui_element::ShowCategorySelector()
{
    InitPresetBrowser();

    // Determine root directory.
    std::wstring rootDir = m_presetLibraryPath;
    if (rootDir.empty())
        rootDir = g_plugin.GetPresetDir();
    if (rootDir.empty())
        return;
    // Ensure no trailing backslash for consistency.
    while (!rootDir.empty() && (rootDir.back() == L'\\' || rootDir.back() == L'/'))
        rootDir.pop_back();

    // Build dialog template in memory.
    struct
    {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD wndClass;
        WCHAR title[32];
    } dlgTemplate = {};

    dlgTemplate.dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
    dlgTemplate.dlg.cx = 250;
    dlgTemplate.dlg.cy = 300;
    dlgTemplate.dlg.cdit = 0;
    wcscpy_s(dlgTemplate.title, L"Select Categories");

    CatSelState state;
    state.self = this;
    state.hTree = NULL;
    state.rootDir = rootDir;
    state.selected = m_selectedCategories;

    INT_PTR result = DialogBoxIndirectParam(core_api::get_my_instance(),
                                            &dlgTemplate.dlg,
                                            get_wnd(),
                                            CatSelDlgProc,
                                            reinterpret_cast<LPARAM>(&state));

    if (result == IDOK)
    {
        m_selectedCategories = state.selected;
        SaveBrowserConfig();
        SyncFiltersToEngine();

        wchar_t buf[128];
        swprintf_s(buf, L"Categories updated: %zu selected", m_selectedCategories.size());
        g_plugin.AddError(buf, 2.0f, ERR_NOTIFY, false);
    }
}

// ============================================================================
// Favorites Manager dialog
// ============================================================================

// Favorites Manager dialog state (passed via GWLP_USERDATA).
struct FavMgrState
{
    milk2_ui_element* self;
    HWND hList;
    std::vector<FavoriteEntry> favs;
};

static void FavMgr_RefreshList(FavMgrState* state)
{
    ListView_DeleteAllItems(state->hList);
    state->favs = state->self->m_favorites.GetAll();

    for (int i = 0; i < static_cast<int>(state->favs.size()); i++)
    {
        // Show full relative path, strip .milk extension.
        std::wstring display = state->favs[i].relativePath;
        if (display.length() > 5 && _wcsicmp(display.c_str() + display.length() - 5, L".milk") == 0)
            display = display.substr(0, display.length() - 5);

        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<wchar_t*>(display.c_str());
        ListView_InsertItem(state->hList, &lvi);
    }

    // Update title with count.
    wchar_t title[64];
    swprintf_s(title, L"Favorites Manager (%zu)", state->favs.size());
    SetWindowText(GetParent(state->hList), title);
}

static INT_PTR CALLBACK FavMgrDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    FavMgrState* state = reinterpret_cast<FavMgrState*>(GetWindowLongPtr(hdlg, GWLP_USERDATA));

    switch (msg)
    {
        case WM_INITDIALOG:
        {
            state = reinterpret_cast<FavMgrState*>(lParam);
            SetWindowLongPtr(hdlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            // Create ListView.
            RECT rc;
            GetClientRect(hdlg, &rc);
            state->hList = CreateWindowEx(0, WC_LISTVIEW, L"",
                                          WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
                                          10, 10, rc.right - 20, rc.bottom - 60,
                                          hdlg, reinterpret_cast<HMENU>(1001), core_api::get_my_instance(), NULL);
            ListView_SetExtendedListViewStyle(state->hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            // Add columns.
            LVCOLUMN lvc = {};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH;
            lvc.cx = rc.right - 40;
            lvc.pszText = const_cast<wchar_t*>(L"Preset");
            ListView_InsertColumn(state->hList, 0, &lvc);

            // Create buttons.
            CreateWindow(L"BUTTON", L"Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         10, rc.bottom - 42, 80, 30, hdlg, reinterpret_cast<HMENU>(IDOK), core_api::get_my_instance(), NULL);
            CreateWindow(L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         100, rc.bottom - 42, 80, 30, hdlg, reinterpret_cast<HMENU>(IDRETRY), core_api::get_my_instance(), NULL);
            CreateWindow(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         rc.right - 90, rc.bottom - 42, 80, 30, hdlg, reinterpret_cast<HMENU>(IDCANCEL), core_api::get_my_instance(), NULL);

            FavMgr_RefreshList(state);
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDCANCEL:
                    EndDialog(hdlg, 0);
                    return TRUE;

                case IDOK: // Load
                {
                    int sel = ListView_GetNextItem(state->hList, -1, LVNI_SELECTED);
                    if (sel >= 0 && sel < static_cast<int>(state->favs.size()))
                    {
                        std::wstring fullPath = state->self->m_presetBrowser.GetRootDir() + state->favs[sel].relativePath;
                        g_plugin.LoadPreset(fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
                    }
                    return TRUE;
                }

                case IDRETRY: // Remove
                {
                    int sel = ListView_GetNextItem(state->hList, -1, LVNI_SELECTED);
                    if (sel >= 0 && sel < static_cast<int>(state->favs.size()))
                    {
                        state->self->m_favorites.Remove(state->favs[sel].relativePath);
                        state->self->m_favorites.Save();
                        FavMgr_RefreshList(state);
                    }
                    return TRUE;
                }
            }
            break;

        case WM_NOTIFY:
        {
            NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == 1001 && nmhdr->code == NM_DBLCLK)
            {
                // Double-click to load.
                int sel = ListView_GetNextItem(state->hList, -1, LVNI_SELECTED);
                if (sel >= 0 && sel < static_cast<int>(state->favs.size()))
                {
                    std::wstring fullPath = state->self->m_presetBrowser.GetRootDir() + state->favs[sel].relativePath;
                    g_plugin.LoadPreset(fullPath.c_str(), s_config.settings.m_fBlendTimeUser);
                }
                return TRUE;
            }
            break;
        }

        case WM_SIZE:
        {
            if (state && state->hList)
            {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                MoveWindow(state->hList, 10, 10, w - 20, h - 60, TRUE);
                // Resize preset column to fill.
                ListView_SetColumnWidth(state->hList, 0, w - 40);
            }
            break;
        }

        case WM_CLOSE:
            EndDialog(hdlg, 0);
            return TRUE;
    }
    return FALSE;
}

void milk2_ui_element::ShowFavoritesManager()
{
    InitPresetBrowser();

    // Build dialog template in memory.
    // This avoids needing a .rc resource entry.
    struct
    {
        DLGTEMPLATE dlg;
        WORD menu;
        WORD wndClass;
        WCHAR title[32];
    } dlgTemplate = {};

    dlgTemplate.dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
    dlgTemplate.dlg.cx = 300; // dialog units
    dlgTemplate.dlg.cy = 200;
    dlgTemplate.dlg.cdit = 0;
    wcscpy_s(dlgTemplate.title, L"Favorites Manager");

    FavMgrState state;
    state.self = this;
    state.hList = NULL;

    DialogBoxIndirectParam(core_api::get_my_instance(),
                           &dlgTemplate.dlg,
                           get_wnd(),
                           FavMgrDlgProc,
                           reinterpret_cast<LPARAM>(&state));
}

void milk2_ui_element::BrowseForPresetLibrary()
{
    // Use Windows folder picker dialog.
    BROWSEINFO bi = {};
    bi.hwndOwner = get_wnd();
    bi.lpszTitle = L"Select Preset Library Directory\n\nChoose a folder containing .milk presets organized into subfolders (categories).";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl)
    {
        wchar_t szPath[MAX_PATH] = {};
        if (SHGetPathFromIDList(pidl, szPath))
        {
            m_presetLibraryPath = szPath;
            if (!m_presetLibraryPath.empty() && m_presetLibraryPath.back() != L'\\')
                m_presetLibraryPath += L'\\';

            SaveBrowserConfig();

            // Re-scan with the new directory.
            m_browserInitialized = false;
            m_presetBrowser.ScanDirectory(m_presetLibraryPath);
            m_browserInitialized = true;

            // Reset category selections since categories have changed.
            m_selectedCategories.clear();
            m_selectedFavCategories.clear();
            SaveBrowserConfig();

            wchar_t buf[512];
            swprintf_s(buf, L"Preset library: %s (%zu presets, %zu categories)",
                       m_presetLibraryPath.c_str(),
                       m_presetBrowser.GetPresetCount(),
                       m_presetBrowser.GetCategoryCount());
            g_plugin.AddError(buf, 4.0f, ERR_NOTIFY, false);
        }
        CoTaskMemFree(pidl);
    }
}

#pragma endregion
} // namespace