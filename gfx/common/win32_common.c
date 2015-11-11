/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../general.h"
#include "win32_common.h"

#if !defined(_XBOX)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <windows.h>
#include <commdlg.h>
#include "../../retroarch.h"

#ifdef HAVE_OPENGL
#include "../drivers_wm/win32_shader_dlg.h"
#endif

#ifndef _XBOX
/* Power Request APIs */

typedef REASON_CONTEXT POWER_REQUEST_CONTEXT, *PPOWER_REQUEST_CONTEXT, *LPPOWER_REQUEST_CONTEXT;

WINBASEAPI
HANDLE
WINAPI
PowerCreateRequest (
    PREASON_CONTEXT Context
    );

WINBASEAPI
BOOL
WINAPI
PowerSetRequest (
    HANDLE PowerRequest,
    POWER_REQUEST_TYPE RequestType
    );

WINBASEAPI
BOOL
WINAPI
PowerClearRequest (
    HANDLE PowerRequest,
    POWER_REQUEST_TYPE RequestType
    );

#ifndef MAX_MONITORS
#define MAX_MONITORS 9
#endif

static HMONITOR win32_monitor_last;
static unsigned win32_monitor_count;
static HMONITOR win32_monitor_all[MAX_MONITORS];

static BOOL CALLBACK win32_monitor_enum_proc(HMONITOR hMonitor,
      HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
   win32_monitor_all[win32_monitor_count++] = hMonitor;
   return TRUE;
}

void win32_monitor_init(void)
{
   win32_monitor_count = 0;
   EnumDisplayMonitors(NULL, NULL, win32_monitor_enum_proc, 0);
}

void win32_monitor_from_window(HWND data, bool destroy)
{
   win32_monitor_last = MonitorFromWindow(data, MONITOR_DEFAULTTONEAREST);
   if (destroy)
      DestroyWindow(data);
}

void win32_monitor_get_info(void)
{
   MONITORINFOEX current_mon;

   memset(&current_mon, 0, sizeof(current_mon));
   current_mon.cbSize = sizeof(MONITORINFOEX);

   GetMonitorInfo(win32_monitor_last, (MONITORINFO*)&current_mon);
   ChangeDisplaySettingsEx(current_mon.szDevice, NULL, NULL, 0, NULL);
}

void win32_monitor_info(void *data, void *hm_data)
{
   unsigned fs_monitor;
   settings_t *settings = config_get_ptr();
   MONITORINFOEX *mon   = (MONITORINFOEX*)data;
   HMONITOR *hm_to_use  = (HMONITOR*)hm_data;

   if (!win32_monitor_last)
      win32_monitor_from_window(GetDesktopWindow(), false);

   *hm_to_use = win32_monitor_last;
   fs_monitor = settings->video.monitor_index;

   if (fs_monitor && fs_monitor <= win32_monitor_count
         && win32_monitor_all[fs_monitor - 1])
      *hm_to_use = win32_monitor_all[fs_monitor - 1];

   memset(mon, 0, sizeof(*mon));
   mon->cbSize = sizeof(MONITORINFOEX);
   GetMonitorInfo(*hm_to_use, (MONITORINFO*)mon);
}
#endif

static bool win32_browser(
      HWND owner,
      char *filename,
      const char *extensions,
      const char *title,
      const char *initial_dir)
{
	OPENFILENAME ofn;

	memset((void*)&ofn, 0, sizeof(OPENFILENAME));

	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = owner;
	ofn.lpstrFilter     = extensions;
	ofn.lpstrFile       = filename;
	ofn.lpstrTitle      = title;
	ofn.lpstrInitialDir = TEXT(initial_dir);
	ofn.lpstrDefExt     = "";
	ofn.nMaxFile        = PATH_MAX;
	ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (!GetOpenFileName(&ofn))
		return false;

	return true;
}

LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   WPARAM mode         = wparam & 0xffff;
   enum event_command cmd         = EVENT_CMD_NONE;
   bool do_wm_close     = false;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   (void)global;

	switch (mode)
   {
      case ID_M_LOAD_CORE:
      case ID_M_LOAD_CONTENT:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            const char *extensions  = NULL;
            const char *title       = NULL;
            const char *initial_dir = NULL;

            if      (mode == ID_M_LOAD_CORE)
            {
               extensions  = "All Files\0*.*\0 Libretro core(.dll)\0*.dll\0";
               title       = "Load Core";
               initial_dir = settings->libretro_directory;
            }
            else if (mode == ID_M_LOAD_CONTENT)
            {
               extensions  = "All Files\0*.*\0\0";
               title       = "Load Content";
               initial_dir = settings->menu_content_directory;
            }

            if (win32_browser(owner, win32_file, extensions, title, initial_dir))
            {
               switch (mode)
               {
                  case ID_M_LOAD_CORE:
                     strlcpy(settings->libretro, win32_file, sizeof(settings->libretro));
                     cmd = EVENT_CMD_LOAD_CORE;
                     break;
                  case ID_M_LOAD_CONTENT:
                     strlcpy(global->path.fullpath, win32_file, sizeof(global->path.fullpath));
                     cmd = EVENT_CMD_LOAD_CONTENT;
                     do_wm_close = true;
                     break;
               }
            }
         }
         break;
      case ID_M_RESET:
         cmd = EVENT_CMD_RESET;
         break;
      case ID_M_MUTE_TOGGLE:
         cmd = EVENT_CMD_AUDIO_MUTE_TOGGLE;
         break;
      case ID_M_MENU_TOGGLE:
         cmd = EVENT_CMD_MENU_TOGGLE;
         break;
      case ID_M_PAUSE_TOGGLE:
         cmd = EVENT_CMD_PAUSE_TOGGLE;
         break;
      case ID_M_LOAD_STATE:
         cmd = EVENT_CMD_LOAD_STATE;
         break;
      case ID_M_SAVE_STATE:
         cmd = EVENT_CMD_SAVE_STATE;
         break;
      case ID_M_DISK_CYCLE:
         cmd = EVENT_CMD_DISK_EJECT_TOGGLE;
         break;
      case ID_M_DISK_NEXT:
         cmd = EVENT_CMD_DISK_NEXT;
         break;
      case ID_M_DISK_PREV:
         cmd = EVENT_CMD_DISK_PREV;
         break;
      case ID_M_FULL_SCREEN:
         cmd = EVENT_CMD_FULLSCREEN_TOGGLE;
         break;
#ifdef HAVE_OPENGL
      case ID_M_SHADER_PARAMETERS:
         shader_dlg_show(owner);
         break;
#endif
      case ID_M_MOUSE_GRAB:
         cmd = EVENT_CMD_GRAB_MOUSE_TOGGLE;
         break;
      case ID_M_TAKE_SCREENSHOT:
         cmd = EVENT_CMD_TAKE_SCREENSHOT;
         break;
      case ID_M_QUIT:
         do_wm_close = true;
         break;
      default:
         if (mode >= ID_M_WINDOW_SCALE_1X && mode <= ID_M_WINDOW_SCALE_10X)
         {
            unsigned idx = (mode - (ID_M_WINDOW_SCALE_1X-1));
            global->pending.windowed_scale = idx;
            cmd = EVENT_CMD_RESIZE_WINDOWED_SCALE;
         }
         else if (mode == ID_M_STATE_INDEX_AUTO)
         {
            signed idx = -1;
            settings->state_slot = idx;
         }
         else if (mode >= (ID_M_STATE_INDEX_AUTO+1) && mode <= (ID_M_STATE_INDEX_AUTO+10))
         {
            signed idx = (mode - (ID_M_STATE_INDEX_AUTO+1));
            settings->state_slot = idx;
         }
         break;
   }

	if (cmd != EVENT_CMD_NONE)
		event_command(cmd);

	if (do_wm_close)
		PostMessage(owner, WM_CLOSE, 0, 0);
	
	return 0L;
}
#endif

bool win32_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
#ifdef _XBOX
   return false;
#else
   HDC monitor            = GetDC(NULL);
   int pixels_x           = GetDeviceCaps(monitor, HORZRES);
   int pixels_y           = GetDeviceCaps(monitor, VERTRES);
   int physical_width     = GetDeviceCaps(monitor, HORZSIZE);
   int physical_height    = GetDeviceCaps(monitor, VERTSIZE);

   ReleaseDC(NULL, monitor);

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         /* 25.4 mm in an inch. */
         *value = 254 * pixels_x / physical_width / 10;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
#endif
}

void win32_show_cursor(bool state)
{
#ifndef _XBOX
   if (state)
      while (ShowCursor(TRUE) < 0);
   else
      while (ShowCursor(FALSE) >= 0);
#endif
}

void win32_check_window(void)
{
#ifndef _XBOX
   MSG msg;

   while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
#endif
}

bool win32_suppress_screensaver(void *data, bool enable)
{
#ifdef _XBOX
   return false;
#else
   typedef HANDLE (WINAPI * PowerCreateRequestPtr)(REASON_CONTEXT *context);
   typedef BOOL   (WINAPI * PowerSetRequestPtr)(HANDLE PowerRequest, POWER_REQUEST_TYPE RequestType);
   HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
   PowerCreateRequestPtr powerCreateRequest =
     (PowerCreateRequestPtr)GetProcAddress(kernel32, "PowerCreateRequest");
   PowerSetRequestPtr    powerSetRequest =
     (PowerSetRequestPtr)GetProcAddress(kernel32, "PowerSetRequest");

   if(enable)
   {
      if(powerCreateRequest && powerSetRequest)
      {
         /* Windows 7, 8, 10 codepath */
         POWER_REQUEST_CONTEXT RequestContext;
        HANDLE Request;

         RequestContext.Version = POWER_REQUEST_CONTEXT_VERSION;
         RequestContext.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
         RequestContext.Reason.SimpleReasonString = L"RetroArch running";

         Request = PowerCreateRequest(&RequestContext);

         powerSetRequest( Request, PowerRequestDisplayRequired);
         return true;
      }
     else
      {
         /* XP / Vista codepath */
         SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
         return true;
      }
   }

   return false;
#endif
}
