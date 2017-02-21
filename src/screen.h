#ifndef __SCREEN_H__
#define __SCREEN_H__

//#include "monitor.h"

typedef struct _screen {

    char * name;
    int width;
    int height;

} screen;

typedef struct _screens {
    int count;
    screen ** list;
} screens;

typedef char * bitmap;


screens* screens_get();

bitmap * screens_grab(screen * s);

bitmap * screens_resize(bitmap * src, int width, int height);


#endif

#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__)

#include <stdio.h>
#include <windows.h>
//#pragma comment(lib, "user32.lib")
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

screen *get_screen(DISPLAY_DEVICEW *adapter, DISPLAY_DEVICEW *display)
{
    screen *screen = malloc(sizeof(screen));
    DEVMODEW current;

    screen->name = calloc(128, sizeof(char));
    WCHAR *name = display? display->DeviceString : adapter->DeviceString;
    for(int i=0; i<128; ++i)screen->name[i] = (name[i]<128)? name[i]: '?';

    get_settings(&current, adapter->DeviceName, ENUM_CURRENT_SETTINGS);
    screen->width = current.dmPelsWidth;
    screen->height = current.dmPelsHeight;

    return screen;
}

screens* screens_get()
{
    int count = 0;
    DISPLAY_DEVICEW adapter, display;
    bool hasDisplays = false;

    // Detect if any displays at all
    for(int i=0; get_device(&adapter, NULL, i); i++) {
        if(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE) {
            if(get_device(&display, adapter.DeviceName, 0)) {
                hasDisplays = true;
                break;
            }
        }
    }

    // Count monitors
    for(int i=0; get_device(&adapter, NULL, i); i++) {
        if((adapter.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
            if(hasDisplays) {
                for(int j=0; get_device(&display, adapter.DeviceName, j); j++) {
                    ++count;
                }
            } else {
                ++count;
            }
        }
    }

    // Initialize monitors
    screens * screens = malloc(sizeof(screens));
    screens->count = count;
    screens->list = malloc(count*sizeof(screen));
    int index = 0;
    for(int i=0; get_device(&adapter, NULL, i); i++) {
        if((adapter.StateFlags & DISPLAY_DEVICE_ACTIVE)) {

            if(hasDisplays) {
                for(int j=0; get_device(&display, adapter.DeviceName, j); j++) {
                    screens->list[index] = get_screen(&adapter, &display);
                    index++;
                }
            } else {
                screens->list[index] = get_screen(&adapter, NULL);
                index++;
            }
        }
    }
    return screens;
}

bitmap * screens_grab(screen * s)
{

    return NULL;
}

bitmap * screens_resize(bitmap * src, int width, int height)
{

    return NULL;
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


char * get_screen_name(CGDirectDisplayID display)
{
    char *name;
    CFDictionaryRef info = IODisplayCreateInfoDictionary(CGDisplayIOServicePort(display),
                           kIODisplayOnlyPreferredName);
    CFDictionaryRef names = CFDictionaryGetValue(info, CFSTR(kDisplayProductName));
    CFStringRef value;
    if(names == NULL || !CFDictionaryGetValueIfPresent(names, CFSTR("en_US"), (const void**) &value)) {
        name = "Unknown";
    } else {
        CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
                       kCFStringEncodingASCII);
        name = calloc(size+1, sizeof(char));
        CFStringGetCString(value, name, size, kCFStringEncodingASCII);
    }
    CFRelease(info);
    return name;
}

screens* screens_get()
{

    uint32_t display_count = 0;
    CGGetOnlineDisplayList(0, NULL, &display_count);
    CGDirectDisplayID *display_ids = calloc(display_count, sizeof(CGDirectDisplayID));
    CGGetOnlineDisplayList(display_count, display_ids, &display_count);


    screens * screens = malloc(sizeof(screens));
    screens->list =  malloc(display_count * sizeof(screen));

    for(int i=0; i<display_count; ++i) {
        screen * screen = malloc(sizeof(screen));
        screen->name = get_screen_name(display_ids[i]);
        CGDisplayModeRef current_mode = CGDisplayCopyDisplayMode(display_ids[i]);
        screen->width = (int)CGDisplayModeGetWidth(current_mode);
        screen->height = (int)CGDisplayModeGetHeight(current_mode);
        screens->list[i] = screen;
    }


    return NULL;
}

bitmap * screens_grab(screen * s)
{

    return NULL;
}

bitmap * screens_resize(bitmap * src, int width, int height)
{

    return NULL;
}


#else

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>

char *copy_str(char *string)
{
    char *copy = NULL;
    int count = 0;
    while(string[count] != 0) ++count;

    copy = calloc(count, sizeof(char));
    for(; count>=0; --count) copy[count]=string[count];
    return copy;
}


screens* screens_get()
{
    Display * display = XOpenDisplay(NULL);
    Window root = XRootWindow(display, XDefaultScreen(display));
    int scr_count = 0;

    XRRScreenResources *screen_resources = XRRGetScreenResources(display, root);
    screens * screens = malloc(screen_resources->ncrtc * sizeof(screens));
    for(int i=0; i<screen_resources->ncrtc; ++i) {
        RRCrtc crtc = screen_resources->crtcs[i];
        XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources, crtc);
        RROutput output = crtc_info->outputs[i];
        XRROutputInfo *output_info = XRRGetOutputInfo(display, screen_resources, output);
        if(output_info->connection == RR_Connected) {
            screen *screen = malloc(sizeof(screen));
            screen->name = copy_str(output_info->name);
            screen->width =	crtc_info->width;
            screen->height = crtc_info->height;
            screens->list[scr_count] = screen;
            scr_count++;
        }

        XRRFreeCrtcInfo(crtc_info);
    }
    screens-> count = scr_count ;
    XRRFreeScreenResources(screen_resources);
    return screens;
}

bitmap * screens_grab(screen * s)
{

    return NULL;
}

bitmap * screens_resize(bitmap * src, int width, int height)
{

    return NULL;
}






#endif
