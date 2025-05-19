#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WendigoApp WendigoApp;

#ifdef __cplusplus
}
#endif

void wendigo_popup_callback(void *context);
void wendigo_display_popup(WendigoApp *app, char *header, char*body);