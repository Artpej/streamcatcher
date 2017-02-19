/*

 See original version here:
 https://github.com/Shirakumo/libmonitors

 License: Artistic License 2.0

*/

#ifndef __LIBMONITORS_H__
#define __LIBMONITORS_H__
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#  if _MSC_VER <= 1600
#    define bool int
#    define true 1
#    define false 0
#  else
#    include <stdbool.h>
#  endif
#else
#  include <stdbool.h>
#endif

#ifdef _MSC_VER
#  ifdef MONITORS_STATIC_DEFINE
#    define MONITORS_EXPORT
#  else
#    ifndef MONITORS_EXPORT
#      define MONITORS_EXPORT __declspec(dllexport)
#    endif
#  endif
#else
#  define MONITORS_EXPORT
#endif

struct libmonitors_monitor_data;
struct libmonitors_mode_data;
struct libmonitors_monitor;

MONITORS_EXPORT struct libmonitors_mode
{
    struct libmonitors_monitor *monitor;
    int width;
    int height;
    int refresh;
    struct libmonitors_mode_data *_data;
};

MONITORS_EXPORT struct libmonitors_monitor
{
    char *name;
    bool primary;
    int width;
    int height;
    int mode_count;
    struct libmonitors_mode *current_mode;
    struct libmonitors_mode *modes;
    struct libmonitors_monitor_data *_data;
};

MONITORS_EXPORT bool libmonitors_init();
MONITORS_EXPORT void libmonitors_deinit();
MONITORS_EXPORT bool libmonitors_detect(int *count, struct libmonitors_monitor ***monitors);
MONITORS_EXPORT bool libmonitors_make_mode_current(struct libmonitors_mode *mode);
MONITORS_EXPORT void libmonitors_free_monitor(struct libmonitors_monitor *monitor);
MONITORS_EXPORT void libmonitors_free_monitors(int count, struct libmonitors_monitor **monitors);

#define MONITOR struct libmonitors_monitor
#define MONITOR_DATA struct libmonitors_monitor_data
#define MODE struct libmonitors_mode
#define MODE_DATA struct libmonitors_mode_data

MONITOR **alloc_monitors(int count);
MONITOR *alloc_monitor(int data);
MODE *alloc_modes(int count);
bool is_duplicate_mode(MODE *mode, int count, MODE *modes);

#ifdef __cplusplus
}
#endif
#endif

// IMPLEMENTATION


MONITOR **alloc_monitors(int count)
{
    return calloc(count, sizeof(MONITOR*));
}

MONITOR *alloc_monitor(int data)
{
    MONITOR *monitor = calloc(1, sizeof(MONITOR));
    monitor->_data = calloc(1, data);
    return monitor;
}

MODE *alloc_modes(int count)
{
    return calloc(count, sizeof(MODE));
}

void free_modes(int count, MODE *modes)
{
    for(int i=0; i<count; ++i)
    {
        if(modes[i]._data != NULL)
            free(modes[i]._data);
    }
    free(modes);
}

MONITORS_EXPORT void libmonitors_free_monitor(MONITOR *monitor)
{
    if(monitor->name != NULL)
        free(monitor->name);
    if(monitor->_data != NULL)
        free(monitor->_data);
    if(monitor->modes != NULL)
        free_modes(monitor->mode_count, monitor->modes);
    free(monitor);
}

MONITORS_EXPORT void libmonitors_free_monitors(int count, MONITOR **monitors)
{
    for(int i=0; i<count; ++i)
    {
        libmonitors_free_monitor(monitors[i]);
    }
    free(monitors);
}

bool is_duplicate_mode(MODE *mode, int count, MODE *modes)
{
    for(int i=0; i<count; ++i)
    {
        if(&modes[i] != mode
                && modes[i].width == mode->width
                && modes[i].height == mode->height
                && modes[i].refresh == mode->refresh)
            return true;
    }
    return false;
}

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)

#include <stdio.h>
#include <windows.h>
//#pragma comment(lib, "user32.lib")

MODE_DATA
{
};

MONITOR_DATA
{
    WCHAR adapter_name[32];
    WCHAR display_name[32];
    bool modes_pruned;
};

bool get_device(DISPLAY_DEVICEW *device, WCHAR *parent, int index)
{
    ZeroMemory(device, sizeof(DISPLAY_DEVICEW));
    device->cb = sizeof(DISPLAY_DEVICEW);
    return EnumDisplayDevicesW(parent, index, device, 0);
}

bool get_settings(DEVMODEW *settings, WCHAR *parent, int index)
{
    ZeroMemory(settings, sizeof(DEVMODEW));
    settings->dmSize = sizeof(DEVMODEW);
    return EnumDisplaySettingsW(parent, index, settings);
}

bool is_acceptable_mode(MONITOR *monitor, DEVMODEW *settings)
{
    if(15 < settings->dmBitsPerPel)
        return false;

    if(monitor->_data->modes_pruned)
    {
        if(ChangeDisplaySettingsExW(monitor->_data->adapter_name, settings, NULL, CDS_TEST, NULL)
                != DISP_CHANGE_SUCCESSFUL)
        {
            return false;
        }
    }
    return true;
}

bool is_comparable_setting(DEVMODEW a, DEVMODEW b)
{
    return (a.dmPelsWidth == b.dmPelsWidth
            && a.dmPelsHeight == b.dmPelsHeight
            && a.dmDisplayFrequency == b.dmDisplayFrequency);
}

void detect_modes(MONITOR *monitor)
{
    int count = 0;
    MODE *modes = NULL;
    DEVMODEW settings;
    DEVMODEW current;

    get_settings(&current, monitor->_data->adapter_name, ENUM_CURRENT_SETTINGS);

    // Count modes
    for(int i=0; get_settings(&settings, monitor->_data->adapter_name, i); ++i)
    {
        if(is_acceptable_mode(monitor, &settings))
        {
            ++count;
        }
    }

    // Initialize modes
    modes = alloc_modes(count);
    int mode = 0;
    for(int i=0; get_settings(&settings, monitor->_data->adapter_name, i); ++i)
    {
        if(is_acceptable_mode(monitor, &settings))
        {
            modes[mode].monitor = monitor;
            modes[mode].width = settings.dmPelsWidth;
            modes[mode].height = settings.dmPelsHeight;
            modes[mode].refresh = settings.dmDisplayFrequency;
            if(is_duplicate_mode(&modes[mode], mode, modes))
            {
                --count;
            }
            else
            {
                if(is_comparable_setting(settings, current))
                {
                    monitor->current_mode = &modes[mode];
                }
                ++mode;
            }
        }
    }

    monitor->mode_count = count;
    monitor->modes = modes;
}

MONITOR *process_monitor(DISPLAY_DEVICEW *adapter, DISPLAY_DEVICEW *display)
{
    MONITOR *monitor = alloc_monitor(sizeof(MONITOR_DATA));

    monitor->name = calloc(128, sizeof(char));
    WCHAR *name = display? display->DeviceString : adapter->DeviceString;
    for(int i=0; i<128; ++i)monitor->name[i] = (name[i]<128)? name[i]: '?';

    if (adapter->StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        monitor->primary = true;

    HDC device_context = CreateDCW(L"DISPLAY", adapter->DeviceName, NULL, NULL);
    monitor->width = GetDeviceCaps(device_context, HORZSIZE);
    monitor->height = GetDeviceCaps(device_context, VERTSIZE);
    DeleteDC(device_context);

    wcscpy_s(monitor->_data->adapter_name, 32, adapter->DeviceName);
    if(display)
        wcscpy_s(monitor->_data->display_name, 32, display->DeviceName);
    if(adapter->StateFlags & DISPLAY_DEVICE_MODESPRUNED)
        monitor->_data->modes_pruned = true;

    detect_modes(monitor);

    return monitor;
}

MONITORS_EXPORT bool libmonitors_detect(int *ext_count, MONITOR ***ext_monitors)
{
    int count = 0;
    MONITOR **monitors = NULL;
    DISPLAY_DEVICEW adapter, display;
    bool hasDisplays = false;

    // Detect if any displays at all
    for(int i=0; get_device(&adapter, NULL, i); i++)
    {
        if(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE)
        {
            if(get_device(&display, adapter.DeviceName, 0))
            {
                hasDisplays = true;
                break;
            }
        }
    }

    // Count monitors
    for(int i=0; get_device(&adapter, NULL, i); i++)
    {
        if((adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
        {
            if(hasDisplays)
            {
                for(int j=0; get_device(&display, adapter.DeviceName, j); j++)
                {
                    ++count;
                }
            }
            else
            {
                ++count;
            }
        }
    }

    // Initialize monitors
    monitors = alloc_monitors(count);
    int monitor = 0;
    for(int i=0; get_device(&adapter, NULL, i); i++)
    {
        if((adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
        {

            if(hasDisplays)
            {
                for(int j=0; get_device(&display, adapter.DeviceName, j); j++)
                {
                    monitors[monitor] = process_monitor(&adapter, &display);
                    monitor++;
                }
            }
            else
            {
                monitors[monitor] = process_monitor(&adapter, NULL);
                monitor++;
            }
        }
    }

    *ext_count = count;
    *ext_monitors = monitors;
    return true;
}

MONITORS_EXPORT bool libmonitors_make_mode_current(MODE *mode)
{
    if(mode->monitor->current_mode != mode)
    {
        DEVMODEW settings;
        ZeroMemory(&settings, sizeof(DEVMODEW));
        settings.dmSize = sizeof(DEVMODEW);
        settings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
        settings.dmPelsWidth = mode->width;
        settings.dmPelsHeight = mode->height;
        settings.dmDisplayFrequency = mode->refresh;

        if(ChangeDisplaySettingsExW(mode->monitor->_data->adapter_name,
                                    &settings,
                                    NULL,
                                    CDS_FULLSCREEN,
                                    NULL) == DISP_CHANGE_SUCCESSFUL)
        {
            mode->monitor->current_mode = mode;
        }
        else
        {
            return false;
        }
    }
    return true;
}

MONITORS_EXPORT bool libmonitors_init()
{
    return true;
}

MONITORS_EXPORT void libmonitors_deinit()
{

}

#elif defined(__APPLE__) && defined(__MACH__)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <CoreVideo/CVBase.h>
#include <CoreVideo/CVDisplayLink.h>

MODE_DATA
{
    int32_t mode_id;
};

MONITOR_DATA
{
    CGDirectDisplayID display_id;
};

char *copy_str(char *string)
{
    char *copy = NULL;
    int count = 0;
    while(string[count] != 0) ++count;

    copy = calloc(count, sizeof(char));
    for(; count>=0; --count) copy[count]=string[count];
    return copy;
}

bool process_mode(MODE *mode, CGDisplayModeRef display_mode, CVDisplayLinkRef display_link)
{
    bool result = false;

    uint32_t mode_flags = CGDisplayModeGetIOFlags(display_mode);
    if((mode_flags & kDisplayModeValidFlag)
            && (mode_flags & kDisplayModeSafeFlag)
            && !(mode_flags & kDisplayModeInterlacedFlag)
            && !(mode_flags & kDisplayModeStretchedFlag))
    {

        CFStringRef format = CGDisplayModeCopyPixelEncoding(display_mode);
        if(!CFStringCompare(format, CFSTR(IO16BitDirectPixels), 0)
                || !CFStringCompare(format, CFSTR(IO32BitDirectPixels), 0))
        {

            if(mode->_data == NULL)
                mode->_data = calloc(1, sizeof(MODE_DATA));
            mode->_data->mode_id = CGDisplayModeGetIODisplayModeID(display_mode);

            mode->width = (int)CGDisplayModeGetWidth(display_mode);
            mode->height = (int)CGDisplayModeGetHeight(display_mode);
            mode->refresh = (int)CGDisplayModeGetRefreshRate(display_mode);

            // Attempt to recover by calculation if possible
            if(mode->refresh == 0)
            {
                const CVTime time = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(display_link);
                if(!(time.flags & kCVTimeIsIndefinite))
                {
                    mode->refresh = (int)(time.timeScale / (double)time.timeValue);
                }
            }
            result = true;
        }
        CFRelease(format);
    }

    return result;
}

void detect_modes(MONITOR *monitor)
{
    int count = 0;
    MODE *modes = NULL;

    CGDisplayModeRef current_mode = CGDisplayCopyDisplayMode(monitor->_data->display_id);
    CVDisplayLinkRef display_link;
    CVDisplayLinkCreateWithCGDisplay(monitor->_data->display_id, &display_link);
    CFArrayRef display_modes = CGDisplayCopyAllDisplayModes(monitor->_data->display_id, NULL);
    int mode_count = CFArrayGetCount(display_modes);

    modes = alloc_modes(mode_count);
    for(int i=0; i<mode_count; ++i)
    {
        CGDisplayModeRef display_mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(display_modes, i);
        modes[count].monitor = monitor;
        if(process_mode(&modes[count], display_mode, display_link))
        {
            if(CGDisplayModeGetIODisplayModeID(display_mode) == CGDisplayModeGetIODisplayModeID(current_mode))
            {
                monitor->current_mode = &modes[count];
            }
            ++count;
        }
    }

    CFRelease(display_modes);
    CVDisplayLinkRelease(display_link);
    CGDisplayModeRelease(current_mode);

    monitor->mode_count = count;
    monitor->modes = modes;
}

MONITOR *process_monitor(CGDirectDisplayID display)
{
    if(!CGDisplayIsAsleep(display))
    {
        MONITOR *monitor = alloc_monitor(sizeof(MONITOR_DATA));

        CFDictionaryRef info = IODisplayCreateInfoDictionary(CGDisplayIOServicePort(display),
                               kIODisplayOnlyPreferredName);
        CFDictionaryRef names = CFDictionaryGetValue(info, CFSTR(kDisplayProductName));
        CFStringRef value;
        if(names == NULL || !CFDictionaryGetValueIfPresent(names, CFSTR("en_US"), (const void**) &value))
        {
            monitor->name = copy_str("Unknown");
        }
        else
        {
            CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
                           kCFStringEncodingASCII);
            char *name = calloc(size+1, sizeof(char));
            CFStringGetCString(value, name, size, kCFStringEncodingASCII);
            monitor->name = name;
        }
        CFRelease(info);

        monitor->primary = CGDisplayIsMain(display);

        const CGSize size = CGDisplayScreenSize(display);
        monitor->width = size.width;
        monitor->height = size.height;

        monitor->_data->display_id = display;

        detect_modes(monitor);

        return monitor;
    }
    return NULL;
}

MONITORS_EXPORT bool libmonitors_detect(int *ext_count, MONITOR ***ext_monitors)
{
    MONITOR **monitors = NULL;
    int count = 0;

    uint32_t display_count = 0;
    CGGetOnlineDisplayList(0, NULL, &display_count);
    CGDirectDisplayID *display_ids = calloc(display_count, sizeof(CGDirectDisplayID));
    CGGetOnlineDisplayList(display_count, display_ids, &display_count);

    monitors = alloc_monitors(display_count);
    for(int i=0; i<display_count; ++i)
    {
        MONITOR *monitor = process_monitor(display_ids[i]);
        if(monitor != NULL)
        {
            monitors[count] = monitor;
            ++count;
        }
    }

    free(display_ids);

    *ext_monitors = monitors;
    *ext_count = count;
    return true;
}

MONITORS_EXPORT bool libmonitors_make_mode_current(MODE *mode)
{
    if(mode->monitor->current_mode != mode)
    {
        int success = false;

        CGDirectDisplayID display_id = mode->monitor->_data->display_id;

        CVDisplayLinkRef display_link;
        CVDisplayLinkCreateWithCGDisplay(display_id, &display_link);
        CFArrayRef display_modes = CGDisplayCopyAllDisplayModes(display_id, NULL);
        CFIndex mode_count = CFArrayGetCount(display_modes);

        CGDisplayModeRef chosen = NULL;
        for(int i=0; i<mode_count; ++i)
        {
            CGDisplayModeRef display_mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(display_modes, i);
            if(mode->_data->mode_id == CGDisplayModeGetIODisplayModeID(display_mode))
            {
                chosen = display_mode;
                break;
            }
        }

        if(chosen != NULL)
        {
            if(CGDisplaySetDisplayMode(display_id, chosen, NULL) == kCGErrorSuccess)
            {
                success = true;
            }
        }

        CFRelease(display_modes);
        CVDisplayLinkRelease(display_link);

        return success;
    }

    return true;
}

MONITORS_EXPORT bool libmonitors_init()
{
    return true;
}

MONITORS_EXPORT void libmonitors_deinit()
{
}

#else

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>

static Display *display = NULL;
static Window root = 0;
static int screen = 0;

MODE_DATA
{
    RRMode rrmode;
};

MONITOR_DATA
{
    RROutput rroutput;
    RRCrtc rrcrtc;
};

char *copy_str(char *string)
{
    char *copy = NULL;
    int count = 0;
    while(string[count] != 0) ++count;

    copy = calloc(count, sizeof(char));
    for(; count>=0; --count) copy[count]=string[count];
    return copy;
}


bool test_xrandr()
{
    int eventBase, errorBase, major, minor;
    if(XRRQueryExtension(display, &eventBase, &errorBase)
            && XRRQueryVersion(display, &major, &minor))
    {
        return (major > 1 || minor >= 3);
    }
    return false;
}

bool process_mode(MODE *mode, XRRScreenResources *screen_resources,
                  XRRCrtcInfo *crtc_info, RRMode output_mode)
{
    XRRModeInfo mode_info;

    if(mode->_data == NULL)
        mode->_data = calloc(1, sizeof(MODE_DATA));
    mode->_data->rrmode = output_mode;

    for(int j=0; j<screen_resources->nmode; ++j)
    {
        mode_info = screen_resources->modes[j];
        if(mode_info.id == output_mode)
            break;
    }

    if((mode_info.modeFlags & RR_Interlace) == 0)
    {
        if(crtc_info->rotation != RR_Rotate_90 && crtc_info->rotation != RR_Rotate_270)
        {
            mode->width = mode_info.width;
            mode->height = mode_info.height;
        }
        else
        {
            mode->width = mode_info.height;
            mode->height = mode_info.width;
        }

        if(mode_info.hTotal && mode_info.vTotal)
        {
            mode->refresh = (int)((double)mode_info.dotClock /
                                  ((double)mode_info.hTotal *
                                   (double)mode_info.vTotal));
        }
        else
        {
            mode->refresh = -1;
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool process_mode_default(MODE *mode)
{
    mode->width = DisplayWidth(display, screen);
    mode->height = DisplayWidth(display, screen);
    mode->refresh = -1;
    return true;
}

void detect_modes(MONITOR *monitor, RRCrtc crtc, RROutput output)
{
    MODE *modes = NULL;
    int count = 0;

    if(test_xrandr())
    {
        XRRScreenResources *screen_resources = XRRGetScreenResourcesCurrent(display, root);
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, crtc);
        XRROutputInfo *output_info = XRRGetOutputInfo(display, screen_resources, output);

        modes = alloc_modes(output_info->nmode);

        for(int i=0; i<output_info->nmode; ++i)
        {
            if(process_mode(&modes[count], screen_resources, crtc_info, output_info->modes[i]))
            {
                if(!is_duplicate_mode(&modes[count], count, modes))
                {
                    modes[count].monitor = monitor;
                    if(crtc_info->mode == output_info->modes[i])
                        monitor->current_mode = &modes[count];
                    ++count;
                }
            }
        }

        XRRFreeOutputInfo(output_info);
        XRRFreeCrtcInfo(crtc_info);
        XRRFreeScreenResources(screen_resources);
    }
    else
    {
        count = 1;
        modes = alloc_modes(count);
        process_mode_default(&modes[0]);
    }

    monitor->modes = modes;
    monitor->mode_count = count;
}

MONITOR *process_monitor(XRRScreenResources *screen_resources, RRCrtc crtc, RROutput output)
{
    XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, crtc);
    XRROutputInfo *output_info = XRRGetOutputInfo(display, screen_resources, output);
    MONITOR *monitor = NULL;

    if(output_info->connection == RR_Connected)
    {
        monitor = alloc_monitor(sizeof(MONITOR_DATA));
        monitor->name = copy_str(output_info->name);
        monitor->_data->rroutput = output;
        monitor->_data->rrcrtc = crtc;

        if(crtc_info->rotation != RR_Rotate_90 && crtc_info->rotation != RR_Rotate_270)
        {
            monitor->width = output_info->mm_width;
            monitor->height = output_info->mm_height;
        }
        else
        {
            monitor->width = output_info->mm_height;
            monitor->height = output_info->mm_width;
        }

        detect_modes(monitor, output_info->crtc, output);
    }

    XRRFreeOutputInfo(output_info);
    XRRFreeCrtcInfo(crtc_info);
    return monitor;
}

bool process_monitor_default(MONITOR *monitor)
{
    monitor->name = copy_str("Display");
    monitor->width = DisplayWidthMM(display, screen);
    monitor->height = DisplayWidthMM(display, screen);
    return true;
}

MONITORS_EXPORT bool libmonitors_detect(int *ext_count, MONITOR ***ext_monitors)
{
    if(!display) return false;

    MONITOR **monitors = NULL;
    int count = 0;

    if(test_xrandr())
    {
        XRRScreenResources *screen_resources = XRRGetScreenResources(display, root);
        RROutput primary_output = XRRGetOutputPrimary(display, root);

        monitors = alloc_monitors(screen_resources->noutput);
        for(int i=0; i<screen_resources->ncrtc; ++i)
        {
            RRCrtc crtc = screen_resources->crtcs[i];
            XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, crtc);

            for(int j=0; j<crtc_info->noutput; ++j)
            {
                RROutput output = crtc_info->outputs[j];
                MONITOR *monitor = process_monitor(screen_resources, crtc, output);
                if(monitor != NULL)
                {
                    if(output == primary_output)
                    {
                        monitor->primary = true;
                    }
                    monitors[count] = monitor;
                    ++count;
                }
            }

            XRRFreeCrtcInfo(crtc_info);
        }
        XRRFreeScreenResources(screen_resources);
    }
    else
    {
        count = 1;
        monitors = alloc_monitors(count);
        monitors[0]->primary = true;
        process_monitor_default(monitors[0]);
    }

    *ext_monitors = monitors;
    *ext_count = count;
    return true;
}

MONITORS_EXPORT bool libmonitors_make_mode_current(MODE *mode)
{
    if(!display || !test_xrandr()) return false;

    if(mode->monitor->current_mode != mode)
    {
        int success = false;
        XRRScreenResources *screen_resources = XRRGetScreenResources(display, root);
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display,
                                                screen_resources,
                                                mode->monitor->_data->rrcrtc);

        if(!XRRSetCrtcConfig(display,
                             screen_resources,
                             mode->monitor->_data->rrcrtc,
                             CurrentTime,
                             crtc_info->x,
                             crtc_info->y,
                             mode->_data->rrmode,
                             crtc_info->rotation,
                             crtc_info->outputs,
                             crtc_info->noutput))
        {
            success = true;
            mode->monitor->current_mode = mode;
        }

        XRRFreeCrtcInfo(crtc_info);
        XRRFreeScreenResources(screen_resources);
        return success;
    }

    return true;
}

MONITORS_EXPORT bool libmonitors_init()
{
    if(!display)
    {
        display = XOpenDisplay(NULL);
        if(!display) return false;

        root = XRootWindow(display, screen);
        screen = XDefaultScreen(display);
    }
    return true;
}

MONITORS_EXPORT void libmonitors_deinit()
{
    if(display)
    {
        XCloseDisplay(display);
        display = NULL;
        root = 0;
        screen = 0;
    }
}

#endif

