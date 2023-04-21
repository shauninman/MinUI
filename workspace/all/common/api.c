#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

///////////////////////////////

void LOG_note(int level, const char* fmt, ...) {
	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	switch(level) {
#ifdef DEBUG
	case LOG_DEBUG:
		printf("[DEBUG] %s", buf);
		break;
#endif
	case LOG_INFO:
		printf("[INFO] %s", buf);
		break;
	case LOG_WARN:
		fprintf(stderr, "[WARN] %s", buf);
		break;
	case LOG_ERROR:
		fprintf(stderr, "[ERROR] %s", buf);
		break;
	default:
		break;
	}
	fflush(stdout);
}

///////////////////////////////

uint32_t RGB_WHITE;
uint32_t RGB_BLACK;
uint32_t RGB_LIGHT_GRAY;
uint32_t RGB_GRAY;
uint32_t RGB_DARK_GRAY;

static struct GFX_Context {
	SDL_Surface* screen;
	SDL_Surface* assets;

	int mode;
	int vsync;
} gfx;

static SDL_Rect asset_rects[] = {
	[ASSET_WHITE_PILL]		= (SDL_Rect){SCALE4( 1, 1,30,30)},
	[ASSET_BLACK_PILL]		= (SDL_Rect){SCALE4(33, 1,30,30)},
	[ASSET_DARK_GRAY_PILL]	= (SDL_Rect){SCALE4(65, 1,30,30)},
	[ASSET_OPTION]			= (SDL_Rect){SCALE4(97, 1,20,20)},
	[ASSET_BUTTON]			= (SDL_Rect){SCALE4( 1,33,20,20)},
	[ASSET_PAGE_BG]			= (SDL_Rect){SCALE4(64,33,15,15)},
	[ASSET_STATE_BG]		= (SDL_Rect){SCALE4(23,54, 8, 8)},
	[ASSET_PAGE]			= (SDL_Rect){SCALE4(39,54, 6, 6)},
	[ASSET_BAR]				= (SDL_Rect){SCALE4(33,58, 4, 4)},
	[ASSET_BAR_BG]			= (SDL_Rect){SCALE4(15,55, 4, 4)},
	[ASSET_BAR_BG_MENU]		= (SDL_Rect){SCALE4(85,56, 4, 4)},
	[ASSET_UNDERLINE]		= (SDL_Rect){SCALE4(85,51, 3, 3)},
	[ASSET_DOT]				= (SDL_Rect){SCALE4(33,54, 2, 2)},
	
	[ASSET_BRIGHTNESS]		= (SDL_Rect){SCALE4(23,33,19,19)},
	[ASSET_VOLUME_MUTE]		= (SDL_Rect){SCALE4(44,33,10,16)},
	[ASSET_VOLUME]			= (SDL_Rect){SCALE4(44,33,18,16)},
	[ASSET_BATTERY]			= (SDL_Rect){SCALE4(47,51,17,10)},
	[ASSET_BATTERY_LOW]		= (SDL_Rect){SCALE4(66,51,17,10)},
	[ASSET_BATTERY_FILL]	= (SDL_Rect){SCALE4(81,33,12, 6)},
	[ASSET_BATTERY_FILL_LOW]= (SDL_Rect){SCALE4( 1,55,12, 6)},
	[ASSET_BATTERY_BOLT]	= (SDL_Rect){SCALE4(81,41,12, 6)},
	
	[ASSET_SCROLL_UP]		= (SDL_Rect){SCALE4(97,23,24, 6)},
	[ASSET_SCROLL_DOWN]		= (SDL_Rect){SCALE4(97,31,24, 6)},
};
static uint32_t asset_rgbs[ASSET_COLORS];
GFX_Fonts font;

///////////////////////////////

static struct POW_Context {
	int can_poweroff;
	int can_autosleep;
	
	pthread_t battery_pt;
	int is_charging;
	int charge;
	int should_warn;

	SDL_Surface* overlay;
} pow;

///////////////////////////////

static int _;

SDL_Surface* GFX_init(int mode) {
	gfx.screen = PLAT_initVideo();
	gfx.vsync = VSYNC_STRICT;
	gfx.mode = mode;
	
	RGB_WHITE		= SDL_MapRGB(gfx.screen->format, TRIAD_WHITE);
	RGB_BLACK		= SDL_MapRGB(gfx.screen->format, TRIAD_BLACK);
	RGB_LIGHT_GRAY	= SDL_MapRGB(gfx.screen->format, TRIAD_LIGHT_GRAY);
	RGB_GRAY		= SDL_MapRGB(gfx.screen->format, TRIAD_GRAY);
	RGB_DARK_GRAY	= SDL_MapRGB(gfx.screen->format, TRIAD_DARK_GRAY);
	
	asset_rgbs[ASSET_WHITE_PILL]	= RGB_WHITE;
	asset_rgbs[ASSET_BLACK_PILL]	= RGB_BLACK;
	asset_rgbs[ASSET_DARK_GRAY_PILL]= RGB_DARK_GRAY;
	asset_rgbs[ASSET_OPTION]		= RGB_DARK_GRAY;
	asset_rgbs[ASSET_BUTTON]		= RGB_WHITE;
	asset_rgbs[ASSET_PAGE_BG]		= RGB_WHITE;
	asset_rgbs[ASSET_STATE_BG]		= RGB_WHITE;
	asset_rgbs[ASSET_PAGE]			= RGB_BLACK;
	asset_rgbs[ASSET_BAR]			= RGB_WHITE;
	asset_rgbs[ASSET_BAR_BG]		= RGB_BLACK;
	asset_rgbs[ASSET_BAR_BG_MENU]	= RGB_DARK_GRAY;
	asset_rgbs[ASSET_UNDERLINE]		= RGB_GRAY;
	asset_rgbs[ASSET_DOT]			= RGB_LIGHT_GRAY;
	
	char asset_path[MAX_PATH];
	sprintf(asset_path, RES_PATH "/assets@%ix.png", FIXED_SCALE);
	gfx.assets = IMG_Load(asset_path);
	
	TTF_Init();
	font.large 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_LARGE));
	font.medium = TTF_OpenFont(FONT_PATH, SCALE1(FONT_MEDIUM));
	font.small 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_SMALL));
	font.tiny 	= TTF_OpenFont(FONT_PATH, SCALE1(FONT_TINY));
	
	return gfx.screen;
}
void GFX_quit(void) {
	TTF_CloseFont(font.large);
	TTF_CloseFont(font.medium);
	TTF_CloseFont(font.small);
	TTF_CloseFont(font.tiny);
	
	SDL_FreeSurface(gfx.assets);
	
	GFX_clearAll();

	PLAT_quitVideo();
}

void GFX_setMode(int mode) {
	gfx.mode = mode;
}
int GFX_getVsync(void) {
	return gfx.vsync;
}
void GFX_setVsync(int vsync) {
	PLAT_setVsync(vsync);
	gfx.vsync = vsync;
}

#define FRAME_BUDGET 17 // 60fps
static uint32_t frame_start = 0;
void GFX_startFrame(void) {
	frame_start = SDL_GetTicks();
}
SDL_Surface* GFX_resize(int w, int h, int pitch) {
	gfx.screen = PLAT_resizeVideo(w,h,pitch);
	return gfx.screen;
}
void GFX_flip(SDL_Surface* screen) {
	int should_vsync = (gfx.vsync!=VSYNC_OFF && (gfx.vsync==VSYNC_STRICT || frame_start==0 || SDL_GetTicks()-frame_start<FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);
}
void GFX_sync(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (gfx.vsync!=VSYNC_OFF) {
		// this limiting condition helps SuperFX chip games
		if (gfx.vsync==VSYNC_STRICT || frame_start==0 || frame_duration<FRAME_BUDGET) { // only wait if we're under frame budget
			PLAT_vsync();
		}
	}
	else {
		if (frame_duration<FRAME_BUDGET) SDL_Delay(FRAME_BUDGET-frame_duration);
	}
}

int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding) {
	int text_width;
	strcpy(out_name, in_name);
	TTF_SizeUTF8(font, out_name, &text_width, NULL);
	text_width += padding;
	
	while (text_width>max_width) {
		int len = strlen(out_name);
		strcpy(&out_name[len-4], "...\0");
		TTF_SizeUTF8(font, out_name, &text_width, NULL);
		text_width += padding;
	}
	
	return text_width;
}
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines) {
	if (!str) return 0;
	
	int line_width;
	int max_line_width = 0;
	char* line = str;
	char buffer[MAX_PATH];
	
	TTF_SizeUTF8(font, line, &line_width, NULL);
	if (line_width<=max_width) {
		line_width = GFX_truncateText(font,line,buffer,max_width,0);
		strcpy(line,buffer);
		return line_width;
	}
	
	char* prev = NULL;
	char* tmp = line;
	int lines = 1;
	int i = 0;
	while (!max_lines || lines<max_lines) {
		tmp = strchr(tmp, ' ');
		if (!tmp) {
			if (prev) {
				TTF_SizeUTF8(font, line, &line_width, NULL);
				if (line_width>=max_width) {
					if (line_width>max_line_width) max_line_width = line_width;
					prev[0] = '\n';
					line = prev + 1;
				}
			}
			break;
		}
		tmp[0] = '\0';
		
		TTF_SizeUTF8(font, line, &line_width, NULL);

		if (line_width>=max_width) { // wrap
			if (line_width>max_line_width) max_line_width = line_width;
			tmp[0] = ' ';
			tmp += 1;
			prev[0] = '\n';
			prev += 1;
			line = prev;
			lines += 1;
		}
		else { // continue
			tmp[0] = ' ';
			prev = tmp;
			tmp += 1;
		}
		i += 1;
	}
	
	line_width = GFX_truncateText(font,line,buffer,max_width,0);
	strcpy(line,buffer);
	
	if (line_width>max_line_width) max_line_width = line_width;
	return max_line_width;
}

void GFX_blitAsset(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Rect* rect = &asset_rects[asset];
	SDL_Rect adj_rect = {
		.x = rect->x,
		.y = rect->y,
		.w = rect->w,
		.h = rect->h,
	};
	if (src_rect) {
		adj_rect.x += src_rect->x;
		adj_rect.y += src_rect->y;
		adj_rect.w  = src_rect->w;
		adj_rect.h  = src_rect->h;
	}
	SDL_BlitSurface(gfx.assets, &adj_rect, dst, dst_rect);
}
void GFX_blitPill(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;

	if (h==0) h = asset_rects[asset].h;
	
	int r = h / 2;
	if (w < h) w = h;
	w -= h;
	
	GFX_blitAsset(asset, &(SDL_Rect){0,0,r,h}, dst, &(SDL_Rect){x,y});
	x += r;
	if (w>0) {
		SDL_FillRect(dst, &(SDL_Rect){x,y,w,h}, asset_rgbs[asset]);
		x += w;
	}
	GFX_blitAsset(asset, &(SDL_Rect){r,0,r,h}, dst, &(SDL_Rect){x,y});
}
void GFX_blitRect(int asset, SDL_Surface* dst, SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;
	int c = asset_rgbs[asset];
	
	SDL_Rect* rect = &asset_rects[asset];
	int d = rect->w;
	int r = d / 2;

	GFX_blitAsset(asset, &(SDL_Rect){0,0,r,r}, dst, &(SDL_Rect){x,y});
	SDL_FillRect(dst, &(SDL_Rect){x+r,y,w-d,r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r,0,r,r}, dst, &(SDL_Rect){x+w-r,y});
	SDL_FillRect(dst, &(SDL_Rect){x,y+r,w,h-d}, c);
	GFX_blitAsset(asset, &(SDL_Rect){0,r,r,r}, dst, &(SDL_Rect){x,y+h-r});
	SDL_FillRect(dst, &(SDL_Rect){x+r,y+h-r,w-d,r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r,r,r,r}, dst, &(SDL_Rect){x+w-r,y+h-r});
}
void GFX_blitBattery(SDL_Surface* dst, SDL_Rect* dst_rect) {
	// LOG_info("dst: %p\n", dst);
	
	if (!dst_rect) dst_rect = &(SDL_Rect){0,0,0,0};
	
	SDL_Rect rect = asset_rects[ASSET_BATTERY];
	int x = dst_rect->x;
	int y = dst_rect->y;
	x += (SCALE1(PILL_SIZE) - (rect.w + FIXED_SCALE)) / 2;
	y += (SCALE1(PILL_SIZE) - rect.h) / 2;
	
	if (pow.is_charging) {
		GFX_blitAsset(ASSET_BATTERY, NULL, dst, &(SDL_Rect){x,y});
		GFX_blitAsset(ASSET_BATTERY_BOLT, NULL, dst, &(SDL_Rect){x+SCALE1(3),y+SCALE1(2)});
	}
	else {
		int percent = pow.charge;
		GFX_blitAsset(percent<=10?ASSET_BATTERY_LOW:ASSET_BATTERY, NULL, dst, &(SDL_Rect){x,y});
		
		rect = asset_rects[ASSET_BATTERY_FILL];
		SDL_Rect clip = rect;
		clip.w *= percent;
		clip.w /= 100;
		if (clip.w<=0) return;
		clip.x = rect.w - clip.w;
		clip.y = 0;
		
		GFX_blitAsset(percent<=20?ASSET_BATTERY_FILL_LOW:ASSET_BATTERY_FILL, &clip, dst, &(SDL_Rect){x+SCALE1(3)+clip.x,y+SCALE1(2)});
	}
}
int GFX_getButtonWidth(char* hint, char* button) {
	int button_width = 0;
	int width;
	
	int special_case = !strcmp(button,BRIGHTNESS_BUTTON_LABEL); // TODO: oof
	
	if (strlen(button)==1) {
		button_width += SCALE1(BUTTON_SIZE);
	}
	else {
		button_width += SCALE1(BUTTON_SIZE) / 2;
		TTF_SizeUTF8(special_case ? font.large : font.tiny, button, &width, NULL);
		button_width += width;
	}
	button_width += SCALE1(BUTTON_MARGIN);
	
	TTF_SizeUTF8(font.small, hint, &width, NULL);
	button_width += width + SCALE1(BUTTON_MARGIN);
	return button_width;
}
void GFX_blitButton(char* hint, char*button, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Surface* text;
	int ox = 0;
	
	int special_case = !strcmp(button,BRIGHTNESS_BUTTON_LABEL); // TODO: oof
	
	// button
	if (strlen(button)==1) {
		GFX_blitAsset(ASSET_BUTTON, NULL, dst, dst_rect);

		// label
		text = TTF_RenderUTF8_Blended(font.medium, button, COLOR_BUTTON_TEXT);
		SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){dst_rect->x+(SCALE1(BUTTON_SIZE)-text->w)/2,dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2});
		ox += SCALE1(BUTTON_SIZE);
		SDL_FreeSurface(text);
	}
	else {
		text = TTF_RenderUTF8_Blended(special_case ? font.large : font.tiny, button, COLOR_BUTTON_TEXT);
		GFX_blitPill(ASSET_BUTTON, dst, &(SDL_Rect){dst_rect->x,dst_rect->y,SCALE1(BUTTON_SIZE)/2+text->w,SCALE1(BUTTON_SIZE)});
		ox += SCALE1(BUTTON_SIZE)/4;
		
		int oy = special_case ? SCALE1(-2) : 0;
		SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2,text->w,text->h});
		ox += text->w;
		ox += SCALE1(BUTTON_SIZE)/4;
		SDL_FreeSurface(text);
	}
	
	ox += SCALE1(BUTTON_MARGIN);

	// hint text
	text = TTF_RenderUTF8_Blended(font.small, hint, COLOR_WHITE);
	SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){ox+dst_rect->x,dst_rect->y+(SCALE1(BUTTON_SIZE)-text->h)/2,text->w,text->h});
	SDL_FreeSurface(text);
}
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, SDL_Rect* dst_rect) {
	if (dst_rect==NULL) dst_rect = &(SDL_Rect){0,0,dst->w,dst->h};
	
	SDL_Surface* text;
#define TEXT_BOX_MAX_ROWS 16
#define LINE_HEIGHT 24
	char* rows[TEXT_BOX_MAX_ROWS];
	int row_count = 0;

	char* tmp;
	rows[row_count++] = msg;
	while ((tmp=strchr(rows[row_count-1], '\n'))!=NULL) {
		if (row_count+1>=TEXT_BOX_MAX_ROWS) return; // TODO: bail
		rows[row_count++] = tmp+1;
	}
	
	int rendered_height = SCALE1(LINE_HEIGHT) * row_count;
	int y = dst_rect->y;
	y += (dst_rect->h - rendered_height) / 2;
	
	char line[256];
	for (int i=0; i<row_count; i++) {
		int len;
		if (i+1<row_count) {
			len = rows[i+1]-rows[i]-1;
			if (len) strncpy(line, rows[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(rows[i]);
			strcpy(line, rows[i]);
		}
		
		
		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, COLOR_WHITE);
			int x = dst_rect->x;
			x += (dst_rect->w - text->w) / 2;
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x,y});
			SDL_FreeSurface(text);
		}
		y += SCALE1(LINE_HEIGHT);
	}
}

int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting) {
	int ox;
	int oy;
	int ow = 0;
	
	int setting_value;
	int setting_min;
	int setting_max;
	
	if (show_setting) {
		ow = SCALE1(PILL_SIZE + SETTINGS_WIDTH + PADDING + 4);
		ox = dst->w - SCALE1(PADDING) - ow;
		oy = SCALE1(PADDING);
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst, &(SDL_Rect){
			ox,
			oy,
			ow,
			SCALE1(PILL_SIZE)
		});
		
		if (show_setting==1) {
			setting_value = GetBrightness();
			setting_min = BRIGHTNESS_MIN;
			setting_max = BRIGHTNESS_MAX;
		}
		else {
			setting_value = GetVolume();
			setting_min = VOLUME_MIN;
			setting_max = VOLUME_MAX;
		}
		
		int asset = show_setting==1?ASSET_BRIGHTNESS:(setting_value>0?ASSET_VOLUME:ASSET_VOLUME_MUTE);
		int ax = ox + (show_setting==1 ? SCALE1(6) : SCALE1(8));
		int ay = oy + (show_setting==1 ? SCALE1(5) : SCALE1(7));
		GFX_blitAsset(asset, NULL, dst, &(SDL_Rect){ax,ay});
		
		ox += SCALE1(PILL_SIZE);
		oy += SCALE1((PILL_SIZE - SETTINGS_SIZE) / 2);
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_BAR_BG : ASSET_BAR_BG_MENU, dst, &(SDL_Rect){
			ox,
			oy,
			SCALE1(SETTINGS_WIDTH),
			SCALE1(SETTINGS_SIZE)
		});
		
		float percent = ((float)(setting_value-setting_min) / (setting_max-setting_min));
		if (show_setting==1 || setting_value>0) {
			GFX_blitPill(ASSET_BAR, dst, &(SDL_Rect){
				ox,
				oy,
				SCALE1(SETTINGS_WIDTH) * percent,
				SCALE1(SETTINGS_SIZE)
			});
		}
	}
	else {
		ow = SCALE1(PILL_SIZE);
		ox = dst->w - SCALE1(PADDING) - ow;
		oy = SCALE1(PADDING);
		GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst, &(SDL_Rect){
			ox,
			oy,
			ow,
			SCALE1(PILL_SIZE)
		});
		GFX_blitBattery(dst, &(SDL_Rect){ox,oy});
	}
	
	return ow;
}
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting) {
	if (BTN_MOD_VOLUME==BTN_SELECT && BTN_MOD_BRIGHTNESS==BTN_START) {
		if (show_setting==1) GFX_blitButtonGroup((char*[]){ "SELECT","VOLUME",  NULL }, dst, 0);
		else GFX_blitButtonGroup((char*[]){ "START","BRIGHTNESS",  NULL }, dst, 0);
	}
	else {
		if (show_setting==1) GFX_blitButtonGroup((char*[]){ BRIGHTNESS_BUTTON_LABEL,"BRIGHTNESS",  NULL }, dst, 0);
		else GFX_blitButtonGroup((char*[]){ "MENU","BRIGHTNESS",  NULL }, dst, 0);
	}
	
}

int GFX_blitButtonGroup(char** pairs, SDL_Surface* dst, int align_right) {
	int ox;
	int oy;
	int ow;
	char* hint;
	char* button;

	struct Hint {
		char* hint;
		char* button;
		int ow;
	} hints[2]; 
	int w = 0; // individual button dimension
	int h = 0; // hints index
	ow = 0; // full pill width
	ox = align_right ? dst->w - SCALE1(PADDING) : SCALE1(PADDING);
	oy = dst->h - SCALE1(PADDING + PILL_SIZE);
	
	for (int i=0; i<2; i++) {
		if (!pairs[i*2]) break;
		
		button = pairs[i * 2];
		hint = pairs[i * 2 + 1];
		w = GFX_getButtonWidth(hint, button);
		hints[h].hint = hint;
		hints[h].button = button;
		hints[h].ow = w;
		h += 1;
		ow += SCALE1(BUTTON_MARGIN) + w;
	}
	
	ow += SCALE1(BUTTON_MARGIN);
	if (align_right) ox -= ow;
	GFX_blitPill(gfx.mode==MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst, &(SDL_Rect){
		ox,
		oy,
		ow,
		SCALE1(PILL_SIZE)
	});
	
	ox += SCALE1(BUTTON_MARGIN);
	oy += SCALE1(BUTTON_MARGIN);
	for (int i=0; i<h; i++) {
		GFX_blitButton(hints[i].hint, hints[i].button, dst, &(SDL_Rect){ox,oy});
		ox += hints[i].ow + SCALE1(BUTTON_MARGIN);
	}
	return ow;
}

#define MAX_TEXT_LINES 16
void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h) {
	char* lines[MAX_TEXT_LINES];
	int count = 0;

	char* tmp;
	lines[count++] = str;
	while ((tmp=strchr(lines[count-1], '\n'))!=NULL) {
		if (count+1>MAX_TEXT_LINES) break; // TODO: bail?
		lines[count++] = tmp+1;
	}
	*h = count * leading;
	
	int mw = 0;
	char line[256];
	for (int i=0; i<count; i++) {
		int len;
		if (i+1<count) {
			len = lines[i+1]-lines[i]-1;
			if (len) strncpy(line, lines[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}
		
		if (len) {
			int lw;
			TTF_SizeUTF8(font, line, &lw, NULL);
			if (lw>mw) mw = lw;
		}
	}
	*w = mw;
}
void GFX_blitText(TTF_Font* font, char* str, int leading, SDL_Color color, SDL_Surface* dst, SDL_Rect* dst_rect) {
	if (dst_rect==NULL) dst_rect = &(SDL_Rect){0,0,dst->w,dst->h};
	
	char* lines[MAX_TEXT_LINES];
	int count = 0;

	char* tmp;
	lines[count++] = str;
	while ((tmp=strchr(lines[count-1], '\n'))!=NULL) {
		if (count+1>MAX_TEXT_LINES) break; // TODO: bail?
		lines[count++] = tmp+1;
	}
	int x = dst_rect->x;
	int y = dst_rect->y;
	
	SDL_Surface* text;
	char line[256];
	for (int i=0; i<count; i++) {
		int len;
		if (i+1<count) {
			len = lines[i+1]-lines[i]-1;
			if (len) strncpy(line, lines[i], len);
			line[len] = '\0';
		}
		else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}
		
		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, color);
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x+((dst_rect->w-text->w)/2),y+(i*leading)});
			SDL_FreeSurface(text);
		}
	}
}

///////////////////////////////

// based on picoarch's audio 
// implementation, rewritten 
// to (try to) understand it 
// better

#define MAX_SAMPLE_RATE 48000
#define BATCH_SIZE 100

typedef int (*SND_Resampler)(const SND_Frame frame);
static struct SND_Context {
	double frame_rate;
	
	int sample_rate_in;
	int sample_rate_out;
	
	int buffer_seconds;     // current_audio_buffer_size
	SND_Frame* buffer;	// buf
	size_t frame_count; 	// buf_len
	
	int frame_in;     // buf_w
	int frame_out;    // buf_r
	int frame_filled; // max_buf_w
	
	SND_Resampler resample;
} snd;
static void SND_audioCallback(void* userdata, uint8_t* stream, int len) { // plat_sound_callback
	if (snd.frame_count==0) return;
	
	int16_t *out = (int16_t *)stream;
	len /= (sizeof(int16_t) * 2);
	
	while (snd.frame_out!=snd.frame_in && len>0) {
		*out++ = snd.buffer[snd.frame_out].left;
		*out++ = snd.buffer[snd.frame_out].right;
		
		snd.frame_filled = snd.frame_out;
		
		snd.frame_out += 1;
		len -= 1;
		
		if (snd.frame_out>=snd.frame_count) snd.frame_out = 0;
	}
	
	while (len>0) {
		*out++ = 0;
		*out++ = 0;
		len -= 1;
	}
}
static void SND_resizeBuffer(void) { // plat_sound_resize_buffer
	snd.frame_count = snd.buffer_seconds * snd.sample_rate_in / snd.frame_rate;
	if (snd.frame_count==0) return;
	
	SDL_LockAudio();
	
	int buffer_bytes = snd.frame_count * sizeof(SND_Frame);
	snd.buffer = realloc(snd.buffer, buffer_bytes);
	
	memset(snd.buffer, 0, buffer_bytes);
	
	snd.frame_in = 0;
	snd.frame_out = 0;
	snd.frame_filled = snd.frame_count - 1;
	
	SDL_UnlockAudio();
}
static int SND_resampleNone(SND_Frame frame) { // audio_resample_passthrough
	snd.buffer[snd.frame_in++] = frame;
	if (snd.frame_in >= snd.frame_count) snd.frame_in = 0;
	return 1;
}
static int SND_resampleNear(SND_Frame frame) { // audio_resample_nearest
	static int diff = 0;
	int consumed = 0;

	if (diff < snd.sample_rate_out) {
		snd.buffer[snd.frame_in++] = frame;
		if (snd.frame_in >= snd.frame_count) snd.frame_in = 0;
		diff += snd.sample_rate_in;
	}

	if (diff >= snd.sample_rate_out) {
		consumed++;
		diff -= snd.sample_rate_out;
	}

	return consumed;
}
static void SND_selectResampler(void) { // plat_sound_select_resampler
	if (snd.sample_rate_in==snd.sample_rate_out) {
		snd.resample =  SND_resampleNone;
	}
	else {
		snd.resample = SND_resampleNear;
	}
}
size_t SND_batchSamples(const SND_Frame* frames, size_t frame_count) { // plat_sound_write / plat_sound_write_resample
	if (snd.frame_count==0) return 0;
	
	SDL_LockAudio();

	int consumed = 0;
	while (frame_count > 0) {
		int tries = 0;
		int amount = MIN(BATCH_SIZE, frame_count);

		while (tries < 10 && snd.frame_in==snd.frame_filled) {
			tries++;
			SDL_UnlockAudio();
			SDL_Delay(1);
			SDL_LockAudio();
		}

		while (amount && snd.frame_in != snd.frame_filled) {
			consumed = snd.resample(*frames);
			frames += consumed;
			amount -= consumed;
			frame_count -= consumed;
		}
	}
	SDL_UnlockAudio();
	
	return consumed;
}

void SND_init(double sample_rate, double frame_rate) { // plat_sound_init
	LOG_info("SND_init\n");
	
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	
	snd.frame_rate = frame_rate;

	SDL_AudioSpec spec_in;
	SDL_AudioSpec spec_out;

	spec_in.freq = PLAT_pickSampleRate(sample_rate, MAX_SAMPLE_RATE);
	spec_in.format = AUDIO_S16;
	spec_in.channels = 2;
	spec_in.samples = 512;
	spec_in.callback = SND_audioCallback;
	
	SDL_OpenAudio(&spec_in, &spec_out);
	
	snd.buffer_seconds = 5;
	snd.sample_rate_in  = sample_rate;
	snd.sample_rate_out = spec_out.freq;
	
	SND_selectResampler();
	SND_resizeBuffer();
	
	SDL_PauseAudio(0);

	LOG_info("sample rate: %i (req) %i (rec)\n", snd.sample_rate_in, snd.sample_rate_out);
}
void SND_quit(void) { // plat_sound_finish
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	
	if (snd.buffer) {
		free(snd.buffer);
		snd.buffer = NULL;
	}
}

///////////////////////////////

static struct PAD_Context {
	int is_pressed;
	int just_pressed;
	int just_released;
	int just_repeated;
	uint32_t repeat_at[BTN_ID_COUNT];
} pad;
#define PAD_REPEAT_DELAY	300
#define PAD_REPEAT_INTERVAL 100
void PAD_reset(void) {
	pad.just_pressed = BTN_NONE;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;
}
void PAD_poll(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
	
	// the actual poll
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int btn = BTN_NONE;
		int id = -1;
		if (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
			// TODO: can this be switched to use SDLK_*?
			uint8_t code = event.key.keysym.scancode;
				 if (code==CODE_UP) 	{ btn = BTN_UP; 		id = BTN_ID_UP; }
 			else if (code==CODE_DOWN)	{ btn = BTN_DOWN; 		id = BTN_ID_DOWN; }
			else if (code==CODE_LEFT)	{ btn = BTN_LEFT; 		id = BTN_ID_LEFT; }
			else if (code==CODE_RIGHT)	{ btn = BTN_RIGHT; 		id = BTN_ID_RIGHT; }
			else if (code==CODE_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
			else if (code==CODE_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
			else if (code==CODE_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
			else if (code==CODE_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
			else if (code==CODE_START)	{ btn = BTN_START; 		id = BTN_ID_START; }
			else if (code==CODE_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
			else if (code==CODE_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
			else if (code==CODE_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
			else if (code==CODE_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
			else if (code==CODE_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
			else if (code==CODE_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
			else if (code==CODE_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
			else if (code==CODE_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
			else if (code==CODE_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
		}
		
		if (btn==BTN_NONE) continue;
		
		if (event.type==SDL_KEYUP) {
			pad.is_pressed		&= ~btn; // unset
			pad.just_repeated	&= ~btn; // unset
			pad.just_released	|= btn; // set
		}
		else if ((pad.is_pressed & btn)==BTN_NONE) {
			pad.just_pressed	|= btn; // set
			pad.just_repeated	|= btn; // set
			pad.is_pressed		|= btn; // set
			pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
		}
	}
}

int PAD_anyPressed(void)		{ return pad.is_pressed!=BTN_NONE; }
int PAD_justPressed(int btn)	{ return pad.just_pressed & btn; }
int PAD_isPressed(int btn)		{ return pad.is_pressed & btn; }
int PAD_justReleased(int btn)	{ return pad.just_released & btn; }
int PAD_justRepeated(int btn)	{ return pad.just_repeated & btn; }

int PAD_tappedMenu(uint32_t now) {
	#define MENU_DELAY 250 // also in POW_update()
	static uint32_t menu_start = 0;
	static int ignore_menu = 0; 
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
		menu_start = now;
	}
	else if (PAD_isPressed(BTN_MENU) && BTN_MOD_BRIGHTNESS==BTN_MENU && (PAD_justPressed(BTN_MOD_PLUS) || PAD_justPressed(BTN_MOD_MINUS))) {
		ignore_menu = 1;
	}
	return (!ignore_menu && PAD_justReleased(BTN_MENU) && now-menu_start<MENU_DELAY);
}

///////////////////////////////

static struct VIB_Context {
	pthread_t pt;
	int queued_strength;
	int strength;
} vib;
// based on eggs retroarch miyoomini rumble
static void miyoomini_rumble(uint16_t strength) {
   static char lastvalue = 0;
   const char str_export[2] = "48";
   const char str_direction[3] = "out";
   char value[1];
   int fd;

   value[0] = (strength == 0 ? 0x31 : 0x30); // '0' : '1'
   if (lastvalue != value[0]) {
      fd = open("/sys/class/gpio/export", O_WRONLY);
      if (fd > 0) { write(fd, str_export, 2); close(fd); }
      fd = open("/sys/class/gpio/gpio48/direction", O_WRONLY);
      if (fd > 0) { write(fd, str_direction, 3); close(fd); }
      fd = open("/sys/class/gpio/gpio48/value", O_WRONLY);
      if (fd > 0) { write(fd, value, 1); close(fd); }
      lastvalue = value[0];
   }
}
static void* VIB_thread(void *arg) {
#define DEFER_FRAMES 3
	static int defer = 0;
	while(1) {
		SDL_Delay(17);
		if (vib.queued_strength!=vib.strength) {
			if (defer<DEFER_FRAMES && vib.queued_strength==0) { // minimize vacillation between 0 and some number (which this motor doesn't like)
				defer += 1;
				continue;
			}
			vib.strength = vib.queued_strength;
			defer = 0;

			PLAT_setRumble(vib.strength);
		}
	}
	return 0;
}
void VIB_init(void) {
	vib.queued_strength = vib.strength = 0;
	pthread_create(&vib.pt, NULL, &VIB_thread, NULL);
}
void VIB_quit(void) {
	VIB_setStrength(0);
	pthread_cancel(vib.pt);
	pthread_join(vib.pt, NULL);
}
void VIB_setStrength(int strength) {
	if (vib.queued_strength==strength) return;
	vib.queued_strength = strength;
}
int VIB_getStrength(void) {
	return vib.strength;
}

///////////////////////////////

static void POW_initOverlay(void) {
	// setup surface
	pow.overlay = PLAT_initOverlay();

	// draw battery
	SDL_SetAlpha(gfx.assets, 0,0);
	GFX_blitAsset(ASSET_BLACK_PILL, NULL, pow.overlay, NULL);
	SDL_SetAlpha(gfx.assets, SDL_SRCALPHA,0);
	GFX_blitBattery(pow.overlay, NULL);
}

static void POW_updateBatteryStatus(void) {
	PLAT_getBatteryStatus(&pow.is_charging, &pow.charge);
	PLAT_enableOverlay(pow.should_warn && pow.charge<=POW_LOW_CHARGE);
}

static void* POW_monitorBattery(void *arg) {
	while(1) {
		// TODO: the frequency of checking should depend on whether 
		// we're in game (less frequent) or menu (more frequent)
		sleep(1);
		POW_updateBatteryStatus();
	}
	return NULL;
}

void POW_init(void) {
	pow.can_poweroff = 1;
	pow.can_autosleep = 1;
	pow.should_warn = 0;
	pow.charge = POW_LOW_CHARGE;
	
	POW_initOverlay();

	POW_updateBatteryStatus();
	pthread_create(&pow.battery_pt, NULL, &POW_monitorBattery, NULL);
}
void POW_quit(void) {
	PLAT_quitOverlay();
	
	// cancel battery thread
	pthread_cancel(pow.battery_pt);
	pthread_join(pow.battery_pt, NULL);
}
void POW_warn(int enable) {
	pow.should_warn = enable;
	PLAT_enableOverlay(pow.should_warn && pow.charge<=POW_LOW_CHARGE);
}

// TODO: generalize this into BTN_MOD_VOLUME, BTN_MOD_BRIGHTNESS, BTN_MOD_PLUS, and BTN_MOD_MINUS
// TODO: rg35x, no BTN_MOD_VOLUME, BTN_MOD_BRIGHTNESS is BTN_MENU, plus/minus are volume so only has to worry about mod
// TODO: mini BTN_MOD_VOLUME is BTN_SELECT, BTN_MOD_BRIGHTNESS is BTN_START, plus/minus are L1 and R1, must worry about any combo
// TODO: create a POW_ignoreSettingInput(btn) if btn==BTN_MOD_PLUS and () couldn't it just be if show_setting && (btn==PLUS || btn==MINUS)?

int POW_ignoreSettingInput(int btn, int show_setting) {
	return show_setting && (btn==BTN_MOD_PLUS || btn==BTN_MOD_MINUS);
}

void POW_update(int* _dirty, int* _show_setting, POW_callback_t before_sleep, POW_callback_t after_sleep) {
	int dirty = _dirty ? *_dirty : 0;
	int show_setting = _show_setting ? *_show_setting : 0;
	
	static uint32_t cancel_start = 0;
	static uint32_t power_start = 0;
	
	// TODO: menu_start and MENU_DELAY are now more accurately mod_start and MOD_DELAY...
	static uint32_t menu_start = 0;
	static uint32_t setting_start = 0;
	static uint32_t charge_start = 0;
	static int was_charging = -1;
	if (was_charging==-1) was_charging = pow.is_charging;

	uint32_t now = SDL_GetTicks();
	if (cancel_start==0) cancel_start = now;
	if (charge_start==0) charge_start = now;
	
	if (PAD_anyPressed()) cancel_start = now;
	
	#define CHARGE_DELAY 1000
	if (dirty || now-charge_start>=CHARGE_DELAY) {
		int is_charging = pow.is_charging;
		if (was_charging!=is_charging) {
			was_charging = is_charging;
			dirty = 1;
		}
		charge_start = now;
	}
	
	if (power_start && now-power_start>=1000) {
		if (before_sleep) before_sleep();
		POW_powerOff();
	}
	if ((BTN_SLEEP==BTN_COMBO && PAD_isPressed(BTN_L1) && PAD_isPressed(BTN_R1) && PAD_justPressed(BTN_MENU)) || PAD_justPressed(BTN_SLEEP)) {
		power_start = now;
	}
	
	#define SLEEP_DELAY 30000
	if (now-cancel_start>=SLEEP_DELAY && POW_preventAutosleep()) cancel_start = now;
	
	if (now-cancel_start>=SLEEP_DELAY || (BTN_SLEEP==BTN_COMBO && PAD_isPressed(BTN_L1) && PAD_isPressed(BTN_R1) && PAD_justReleased(BTN_MENU)) || PAD_justReleased(BTN_SLEEP)) {
		if (before_sleep) before_sleep();
		POW_fauxSleep();
		if (after_sleep) after_sleep();
		
		cancel_start = now = SDL_GetTicks();
		power_start = 0;
		dirty = 1;
	}
	
	int was_dirty = dirty; // dirty list (not including settings/battery)
	
	int delay_settings = BTN_MOD_VOLUME==BTN_MENU || BTN_MOD_BRIGHTNESS==BTN_MENU;
	#define SETTING_DELAY 500
	if (show_setting && (now-setting_start>=SETTING_DELAY || !delay_settings) && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		show_setting = 0;
		dirty = 1;
	}
	
	if (!show_setting && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		menu_start = now; // this is weird, updates until pressed
	}
	
	#define MENU_DELAY 250 // also in PAD_tappedMenu()
	if (((PAD_isPressed(BTN_MOD_VOLUME) || PAD_isPressed(BTN_MOD_BRIGHTNESS)) && (!delay_settings || now-menu_start>=MENU_DELAY)) || 
		((!BTN_MOD_VOLUME || !BTN_MOD_BRIGHTNESS) && (PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS)))) {
	// if (PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS) || ((PAD_isPressed(BTN_MOD_VOLUME) || PAD_isPressed(BTN_MOD_BRIGHTNESS)) && now-menu_start>=MENU_DELAY)) {
		setting_start = now;
		if (PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
			show_setting = 1;
		}
		else {
			show_setting = 2;
		}
	}
	if (show_setting) dirty = 1; // shm is slow or keymon is catching input on the next frame

	if (_dirty) *_dirty = dirty;
	if (_show_setting) *_show_setting = show_setting;
}

void POW_disablePowerOff(void) {
	pow.can_poweroff = 0;
}
void POW_powerOff(void) {
	if (pow.can_poweroff) {
		GFX_resize(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH); // TODO: only if necessary (which depends on platform.c implementation...)
		// GFX_clear(gfx.screen); // TODO: performed by resize
		
		char* msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npowering off" : "Powering off";
		GFX_blitMessage(font.large, msg, gfx.screen, NULL);
		GFX_flip(gfx.screen);
		PLAT_powerOff();
	}
}

static void POW_enterSleep(void) {
	SetRawVolume(0);
	PLAT_enableBacklight(0);
	system("killall -STOP keymon.elf");
	
	sync();
}
static void POW_exitSleep(void) {
	system("killall -CONT keymon.elf");
	PLAT_enableBacklight(1);
	SetVolume(GetVolume());
	
	sync();
}
// TODO: this needs to account for devices without power buttons
static void POW_waitForWake(void) {
	SDL_Event event;
	int wake = 0;
	uint32_t sleep_ticks = SDL_GetTicks();
	while (!wake) {
		while (SDL_PollEvent(&event)) {
			if (event.type==SDL_KEYUP) {
				SDLKey key = event.key.keysym.sym;
				if ((BTN_WAKE==BTN_POWER && key==BUTTON_POWER) || (BTN_WAKE==BTN_MENU && key==BUTTON_MENU)) {
					wake = 1;
					break;
				}
			}
		}
		SDL_Delay(200);
		if (pow.can_poweroff && SDL_GetTicks()-sleep_ticks>=120000) { // increased to two minutes
			if (pow.is_charging) sleep_ticks += 60000; // check again in a minute
			else POW_powerOff(); // TODO: not working...
		}
	}
	return;
}
void POW_fauxSleep(void) {
	GFX_clear(gfx.screen);
	PAD_reset();
	POW_enterSleep();
	POW_waitForWake();
	POW_exitSleep();
}

void POW_disableAutosleep(void) {
	pow.can_autosleep = 0;
}
void POW_enableAutosleep(void) {
	pow.can_autosleep = 1;
}
int POW_preventAutosleep(void) {
	return pow.is_charging || !pow.can_autosleep;
}

// updated by POW_updateBatteryStatus()
int POW_isCharging(void) {
	return pow.is_charging;
}
int POW_getBattery(void) { // 10-100 in 10-20% fragments
	return pow.charge;
}
