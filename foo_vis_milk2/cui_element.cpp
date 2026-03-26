/*
 * cui_element.cpp - Column UI panel support for MilkDrop 2 visualization.
 *
 * #included from ui_element.cpp inside the anonymous namespace.
 * Uses deferred DX initialization — device creation happens on first
 * WM_SIZE with valid dimensions, not in WM_CREATE.
 *
 * Copyright (c) 2025 Richard Albert Nichols III (Inhahe)
 * SPDX-License-Identifier: MIT
 */

#if HAS_COLUMNS_UI

} // close anonymous namespace for CUI SDK symbols

#pragma warning(push)
#pragma warning(disable: 4100 4127 4189 4245 4505 4996)
#include <columns_ui-sdk/ui_extension.cpp>
#pragma warning(pop)

namespace { // reopen

static const GUID guid_cui_milk2 = {
    0x304b0345, 0x4df5, 0x4b47, {0xad, 0xd3, 0x98, 0x9f, 0x81, 0x1b, 0xd9, 0xa6}
};

class milk2_cui_panel : public uie::window,
                        public CWindowImpl<milk2_cui_panel>
{
  public:
    DECLARE_WND_CLASS(TEXT("MilkDrop2Plus_CUI"));

    const GUID& get_extension_guid() const override { return guid_cui_milk2; }
    void get_name(pfc::string_base& out) const override { out = "MilkDrop 2 Plus"; }
    void get_category(pfc::string_base& out) const override { out = "Visualisations"; }
    unsigned get_type() const override { return uie::type_panel; }
    bool get_description(pfc::string_base& out) const override
    {
        out = "MilkDrop 2 Plus Visualization using DirectX 11.";
        return true;
    }
    bool is_available(const uie::window_host_ptr&) const override { return true; }

    HWND create_or_transfer_window(HWND wnd_parent, const uie::window_host_ptr& p_host,
                                   const ui_helpers::window_position_t& p_position) override
    {
        if (m_hWnd)
        {
            ShowWindow(SW_HIDE);
            SetParent(wnd_parent);
            if (m_host.is_valid())
                m_host->relinquish_ownership(m_hWnd);
            m_host = p_host;
            SetWindowPos(NULL, p_position.x, p_position.y,
                         p_position.cx, p_position.cy, SWP_NOZORDER);
        }
        else
        {
            m_host = p_host;
            CRect rc;
            p_position.convert_to_rect(rc);
            Create(wnd_parent, &rc, TEXT("MilkDrop"), WS_CHILD | WS_CLIPCHILDREN, 0);
        }
        return m_hWnd;
    }

    void destroy_window() override
    {
        if (m_hWnd)
            DestroyWindow();
        m_host.release();
    }

    HWND get_wnd() const override { return m_hWnd; }

    BEGIN_MSG_MAP_EX(milk2_cui_panel)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MESSAGE_HANDLER_EX(WM_USER + 100, OnDeferredInit)
    END_MSG_MAP()

  private:
    // Phase 1: Lightweight init only — NO DX device creation.
    int OnCreate(LPCREATESTRUCT)
    {
        try
        {
            // Initialize config/paths if this is the first MilkDrop instance.
            if (!s_milk2)
            {
                milk2_ui_element::ResolvePwd();
                s_config.init();
#ifdef TIMER_TP
                InitializeCriticalSection(&s_cs);
#endif
            }

            // Load preset library path from saved config.
            {
                std::wstring profileDir = s_config.settings.m_szPluginsDirPath;
                if (!profileDir.empty() && profileDir.back() != L'\\')
                    profileDir += L'\\';
                std::wstring configPath = profileDir + L"browser_config.ini";
                if (std::filesystem::exists(configPath))
                {
                    std::wifstream ifs(configPath);
                    std::wstring line;
                    while (std::getline(ifs, line))
                    {
                        size_t eq = line.find(L'=');
                        if (eq == std::wstring::npos) continue;
                        std::wstring key = line.substr(0, eq);
                        std::wstring value = line.substr(eq + 1);
                        while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n'))
                            value.pop_back();
                        if (key == L"PresetLibraryPath" && !value.empty())
                        {
                            wcscpy_s(s_config.settings.m_szPresetDir, value.c_str());
                        }
                    }
                }
            }

            // Create visualization stream for audio data.
            static_api_ptr_t<visualisation_manager> vis_manager;
            vis_manager->create_stream(m_vis_stream, 0);
            m_vis_stream->request_backlog(0.8);
        }
        catch (std::exception& e)
        {
            FB2K_console_print(core_api::get_my_file_name(), ": CUI OnCreate: ", e.what());
        }
        catch (...)
        {
            FB2K_console_print(core_api::get_my_file_name(), ": CUI OnCreate failed");
        }
        return 0;
    }

    void OnDestroy()
    {
        // Stop render timer first.
#ifdef TIMER_TP
        if (m_tpTimer)
        {
            SetThreadpoolTimer(m_tpTimer, NULL, 0, 0);
            WaitForThreadpoolTimerCallbacks(m_tpTimer, TRUE);
            CloseThreadpoolTimer(m_tpTimer);
            m_tpTimer = nullptr;
        }
#endif
        if (m_vis_stream.is_valid())
            m_vis_stream.release();

        // If we own the DX context, clean up.
        if (m_dxInitialized)
        {
            g_plugin.PluginQuit();
            s_milk2 = false;
            m_dxInitialized = false;
        }
    }

    // Phase 2: Deferred DX init — posted from WM_SIZE so it runs after UI is fully initialized.
    void OnSize(UINT nType, CSize size)
    {
        if (nType == SIZE_MINIMIZED || size.cx <= 0 || size.cy <= 0)
            return;

        if (!m_dxInitialized)
        {
            // Don't init synchronously in WM_SIZE — the UI might not be ready yet.
            // Post a custom message so init happens after the message queue settles.
            if (!m_initPosted)
            {
                m_initPosted = true;
                PostMessage(WM_USER + 100, size.cx, size.cy);
            }
            return;
        }

        // Normal resize after init.
        if (s_milk2)
        {
#ifdef TIMER_TP
            if (TryEnterCriticalSection(&s_cs) == 0) return;
#endif
            g_plugin.OnWindowSizeChanged(size.cx, size.cy);
#ifdef TIMER_TP
            LeaveCriticalSection(&s_cs);
#endif
        }
    }

    // Actual DX init — runs after WM_SIZE via PostMessage.
    LRESULT OnDeferredInit(UINT, WPARAM wParam, LPARAM lParam)
    {
        if (m_dxInitialized)
            return 0;

        int w = static_cast<int>(wParam);
        int h = static_cast<int>(lParam);
        if (w <= 0 || h <= 0)
        {
            CRect rc;
            GetClientRect(&rc);
            w = rc.Width();
            h = rc.Height();
        }
        if (w <= 0 || h <= 0)
            return 0;

        {
            try
            {
                if (!s_pwd.empty())
                    swprintf_s(g_plugin.m_szComponentDirPath, L"%ls", s_pwd.c_str());
                if (s_config.settings.m_szPresetDir[0])
                    wcscpy_s(g_plugin.m_szPresetDir, s_config.settings.m_szPresetDir);

                g_hWindow = m_hWnd;

                // Pre-initialize the engine (same as DUI element's Initialize).
                g_plugin.PluginPreInitialize(m_hWnd, core_api::get_my_instance());
                g_plugin.PanelSettings(&s_config.settings);
                if (!s_pwd.empty())
                    swprintf_s(g_plugin.m_szComponentDirPath, L"%ls", s_pwd.c_str());
                g_plugin.SetWinampWindow(m_hWnd);

                FB2K_console_print(core_api::get_my_file_name(), ": CUI init w=", w, " h=", h);

                if (!s_milk2)
                {
                    int result = g_plugin.PluginInitialize(w, h);
                    if (FALSE != result)
                    {
                        s_milk2 = true;
                        m_dxInitialized = true;
                        FB2K_console_print(core_api::get_my_file_name(), ": CUI MilkDrop initialized (", w, "x", h, ")");
                    }
                    else
                    {
                        // Init failed — restore window handle so we don't break other windows.
                        g_hWindow = NULL;
                        FB2K_console_print(core_api::get_my_file_name(), ": CUI PluginInitialize failed");
                        m_initPosted = false; // allow retry
                        return 0;
                    }
                }
                else
                {
                    // Engine already initialized (e.g. from DUI) — swap to our window.
#ifdef TIMER_TP
                    EnterCriticalSection(&s_cs);
#endif
                    g_plugin.OnWindowSwap(m_hWnd, w, h);
#ifdef TIMER_TP
                    LeaveCriticalSection(&s_cs);
#endif
                    m_dxInitialized = true;
                }

                // Start render timer only after successful DX init.
#ifdef TIMER_TP
                m_tpTimer = CreateThreadpoolTimer(CuiTimerCb, this, NULL);
                if (m_tpTimer)
                {
                    FILETIME ft;
                    LARGE_INTEGER li;
                    li.QuadPart = -330000LL; // 33ms
                    ft.dwLowDateTime = li.LowPart;
                    ft.dwHighDateTime = li.HighPart;
                    SetThreadpoolTimer(m_tpTimer, &ft, 33, 0);
                }
#endif
            }
            catch (std::exception& e)
            {
                g_hWindow = NULL;
                m_initPosted = false; // allow retry on next WM_SIZE
                FB2K_console_print(core_api::get_my_file_name(), ": CUI deferred init: ", e.what());
            }
            catch (...)
            {
                g_hWindow = NULL;
                m_initPosted = false;
                FB2K_console_print(core_api::get_my_file_name(), ": CUI deferred init failed");
            }
        }
        return 0;
    }

    BOOL OnEraseBkgnd(CDCHandle) { return TRUE; }

    // Render tick — builds audio data and calls MilkDrop render.
    void CuiTick()
    {
#ifdef TIMER_TP
        if (TryEnterCriticalSection(&s_cs) == 0)
            return;
#endif
        try
        {
        if (m_vis_stream.is_valid())
        {
            double time;
            if (m_vis_stream->get_absolute_time(time))
            {
                double dt = time - m_last_time;
                m_last_time = time;
                if (dt < 1.0 / 1000.0) dt = 1.0 / 1000.0;
                if (dt > 1.0 / 10.0) dt = 1.0 / 10.0;

                audio_chunk_impl chunk;
                if (m_vis_stream->get_chunk_absolute(chunk, time - dt, dt))
                {
                    const audio_sample* data = chunk.get_data();
                    size_t samples = chunk.get_sample_count();
                    size_t channels = chunk.get_channel_count();
                    for (size_t i = 0; i < NUM_AUDIO_BUFFER_SAMPLES; i++)
                    {
                        size_t idx = i * samples / NUM_AUDIO_BUFFER_SAMPLES;
                        m_waves[0][i] = (idx < samples && channels >= 1) ? static_cast<float>(data[idx * channels]) : 0.0f;
                        m_waves[1][i] = (idx < samples && channels >= 2) ? static_cast<float>(data[idx * channels + 1]) : m_waves[0][i];
                    }
                }
            }
        }

        g_plugin.PluginRender(m_waves[0].data(), m_waves[1].data());
        }
        catch (...) {}

#ifdef TIMER_TP
        LeaveCriticalSection(&s_cs);
#endif
    }

    static VOID CALLBACK CuiTimerCb(PTP_CALLBACK_INSTANCE, PVOID ctx, PTP_TIMER) noexcept
    {
        auto* self = static_cast<milk2_cui_panel*>(ctx);
        if (!self || !self->m_hWnd || !s_milk2 || !self->m_dxInitialized) return;
        self->CuiTick();
    }

    uie::window_host_ptr m_host;
    visualisation_stream_v3::ptr m_vis_stream;
    std::array<std::array<float, NUM_AUDIO_BUFFER_SAMPLES>, 2> m_waves = {};
    double m_last_time = 0.0;
    bool m_dxInitialized = false;
    bool m_initPosted = false;
#ifdef TIMER_TP
    PTP_TIMER m_tpTimer = nullptr;
#endif
};

} // close anonymous namespace

static uie::window_factory_single<milk2_cui_panel> g_cui_milk2_factory;

namespace { // reopen

#endif // HAS_COLUMNS_UI
