/**
 * Hello EV3
 *
 * This is a program used to test the whole platform.
 */

#include "ev3api.h"
#include "app.h"
#include "platform.h"
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/mperrno.h"
#include "lib/utils/interrupt_char.h"
#include "fatfs_dri.h"

void stop_user_script(){
	mp_keyboard_interrupt();
}

static char heap[16384];

void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {

    printf(str);
}

mp_lexer_t *mp_lexer_new_from_file(const char *filename)
{
	FILE *sdfile = fopen(filename, "rb");
	if (sdfile == NULL) {
		mp_raise_OSError(MP_ENOENT);
	}
	fseek(sdfile, 0, SEEK_END);
	long fsize = ftell(sdfile);
	fseek(sdfile, 0, SEEK_SET);

	char *string = malloc(fsize + 1);
	fread(string, 1, fsize, sdfile);
	fclose(sdfile);

	string[fsize] = 0;

    return mp_lexer_new_from_str_len(qstr_from_str(filename), string, strlen(string), false);
}

mp_obj_t execute_from_file(const char *file) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr src_name = 1/*MP_QSTR_*/;
        // mp_lexer_t *lex = mp_lexer_new_from_str_len(src_name, str, strlen(str), false);
		mp_lexer_t *lex = mp_lexer_new_from_file(file);
        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&pt, src_name, MP_EMIT_OPT_NONE, false);
        mp_call_function_0(module_fun);
        nlr_pop();
        return 0;
    } else {
        // uncaught exception
        return (mp_obj_t)nlr.ret_val;
    }
}

void mainpy(const char *file) {

    ev3_font_get_size(MENU_FONT, &default_menu_font_width, &default_menu_font_height);

    // Initialized stack limit
    mp_stack_set_limit(40000 * (BYTES_PER_WORD / 4));
    // Initialize heap
    gc_init(heap, heap + sizeof(heap));
    // Initialize interpreter
    mp_init();

    if (execute_from_file(file)) {
        printf("process mpy traceback\r\n");
    }

	mp_deinit();
}



FILE *fio;

int32_t default_menu_font_width;
int32_t default_menu_font_height;

static void draw_menu_entry(const CliMenu *cm, int index, bool_t selected, int offset_x, int offset_y, lcdfont_t font) {
	// Get font information
	int32_t fontw, fonth;
	ev3_lcd_set_font(font);
	ev3_font_get_size(font, &fontw, &fonth);

	ev3_lcd_draw_string(cm->entry_tab[index].title, offset_x + fontw + 2, offset_y + fonth * index);
	if (selected)
		ev3_lcd_draw_string(">", offset_x, offset_y + fonth * index);
	else
		ev3_lcd_draw_string(" ", offset_x, offset_y + fonth * index);
}


void show_cli_menu(const CliMenu *cm, int offset_x, int offset_y, lcdfont_t font) {
	// Get font information
	int32_t fontw, fonth;
	ev3_lcd_set_font(font);
	ev3_font_get_size(font, &fontw, &fonth);

	// Clear menu area
	ev3_lcd_fill_rect(offset_x, offset_y, EV3_LCD_WIDTH, EV3_LCD_HEIGHT, EV3_LCD_WHITE);

	// Draw title
	if (EV3_LCD_WIDTH - offset_x > strlen(cm->title) * fontw)
		offset_x += (EV3_LCD_WIDTH - offset_x - strlen(cm->title) * fontw) / 2;
	ev3_lcd_draw_string(cm->title, offset_x, offset_y);
	ev3_lcd_draw_line(0, offset_y + fonth - 1, EV3_LCD_WIDTH, offset_y + fonth - 1);
//    fprintf(fio, "%s\n", cm->title);

}

const CliMenuEntry* select_menu_entry(const CliMenu *cm, int offset_x, int offset_y, lcdfont_t font) {
	// Get font information
	int32_t fontw, fonth;
	ev3_lcd_set_font(font);
	ev3_font_get_size(font, &fontw, &fonth);

	// Draw menu entries
	for(SIZE i = 0; i < cm->entry_num; ++i)
		draw_menu_entry(cm, i, false, offset_x, offset_y, font);
//    for(SIZE i = 0; i < cm->entry_num; ++i)
//        fprintf(fio, "[%c] %s\n", cm->entry_tab[i].key, cm->entry_tab[i].title);

	int current = 0;
//    syslog(LOG_NOTICE, "Please enter an option.");

	bool_t select_finished = false;
	while (!select_finished) {
		draw_menu_entry(cm, current, true, offset_x, offset_y, font);
		while(1) {
			if (ev3_button_is_pressed(UP_BUTTON)) {
				while(ev3_button_is_pressed(UP_BUTTON));
				draw_menu_entry(cm, current, false, offset_x, offset_y, font);
				current = (current - 1) % cm->entry_num;
				break;
			}
			if (ev3_button_is_pressed(DOWN_BUTTON)) {
				while(ev3_button_is_pressed(DOWN_BUTTON));
				draw_menu_entry(cm, current, false, offset_x, offset_y, font);
				current = (current + 1) % cm->entry_num;
				break;
			}
			if (ev3_button_is_pressed(ENTER_BUTTON)) {
				while(ev3_button_is_pressed(ENTER_BUTTON));
				select_finished = true;
				break;
			}
			if (ev3_button_is_pressed(BACK_BUTTON)) {
				while(ev3_button_is_pressed(BACK_BUTTON));
			    for(SIZE i = 0; i < cm->entry_num; ++i) {
			        if(toupper(cm->entry_tab[i].key) == toupper((int8_t)'Q')) { // BACK => 'Q'
			        	current = i;
			        	select_finished = true;
			        }
			    }
				break;
			}
		}
	}
	

	assert(current >= 0 && current < cm->entry_num);
	return &cm->entry_tab[current];
}



static
void test_audio_files() {
#define TMAX_FILE_NUM (9)
#define SD_RES_FOLDER "/scripts"

	static fileinfo_t fileinfos[TMAX_FILE_NUM];

	int filenos = 0;

	int dirid = ev3_sdcard_opendir(SD_RES_FOLDER);

	while (filenos < TMAX_FILE_NUM && ev3_sdcard_readdir(dirid, &fileinfos[filenos]) == E_OK) {
        if (fileinfos[filenos].is_dir) // Skip folders
            continue;
        const char *dot = strrchr(fileinfos[filenos].name, '.');
        if (dot != NULL && strlen(dot) == strlen(".py")) { // Only PY files are accepted.
        	if (toupper((unsigned char)dot[1]) != 'P' || toupper((unsigned char)dot[2]) != 'Y')
        		continue;
    		filenos++;
        }
	}
	ev3_sdcard_closedir(dirid);

	if (filenos == 0) {
		show_message_box("File Not Found", "No script (py) file is found in '" SD_RES_FOLDER "'.", true);
		return;
	}

	static CliMenuEntry entry_tab[TMAX_FILE_NUM + 1];
	for (int i = 0; i < filenos; ++i) {
		entry_tab[i].key = '1' + i;
		entry_tab[i].title = fileinfos[i].name;
		entry_tab[i].exinf = i;
	}
	entry_tab[filenos].key = 'Q';
	entry_tab[filenos].title = "Cancel";
	entry_tab[filenos].exinf = -1;

	static CliMenu climenu = {
		.title     = "Pybricks",
		.entry_tab = entry_tab,
		.entry_num = sizeof(entry_tab) / sizeof(CliMenuEntry),
	};
	climenu.entry_num = filenos + 1;

	static char filepath[256 * 2];
	while(1) {
		show_cli_menu(&climenu, 0, 0, MENU_FONT);
		const CliMenuEntry *cme = select_menu_entry(&climenu, 0, MENU_FONT_HEIGHT, MENU_FONT);
		if(cme != NULL) {
			switch(cme->exinf) {
			case -1:
				return;

			default: {
				
				strcpy(filepath, SD_RES_FOLDER);
				strcat(filepath, "/");
				strcat(filepath, fileinfos[cme->exinf].name);
				show_message_box("Running script", filepath, false);
				mainpy(filepath);
				// sprintf(msgbuf, "File '%s' is now being played.", filepath);
				
			    } break;
			}
		}
	}
}

static void shutdown(intptr_t unused) {
    if (try_acquire_mmcsd() != E_OK) {
	    syslog(LOG_ERROR, "Please eject the USB!");
        return;
    }
	syslog(LOG_NOTICE, "Shutdown EV3...");
	ext_ker();
}

static const CliMenuEntry entry_tab[] = {
	{ .key = '5', .title = "Scripts", .handler = test_audio_files },
	// { .key = '5', .title = "Terminal", .handler = toggle_terminal },
	{ .key = '6', .title = "Shutdown", .handler = shutdown },// or do Q?
};


const CliMenu climenu_pb = {
	.title     = "Pybricks",
	.entry_tab = entry_tab,
	.entry_num = sizeof(entry_tab) / sizeof(CliMenuEntry),
};


void poll_task(intptr_t unused) {

    assert(loc_mtx(MTX1) == E_OK);
	// Do something
	assert(unl_mtx(MTX1) == E_OK);

}

void task_activator(intptr_t tskid) {
    ER ercd = act_tsk(tskid);
    assert(ercd == E_OK);
}

void pbio_do_a_thing() {
    assert(loc_mtx(MTX1) == E_OK);
	// Do something
    assert(unl_mtx(MTX1) == E_OK);
}



void init_task(intptr_t unused) {
	while (!platform_is_ready());
	ev3_led_set_color(LED_ORANGE);
	ev3_button_set_on_clicked(BACK_BUTTON, stop_user_script, BACK_BUTTON);
    assert(act_tsk(MAIN_TASK) == E_OK);
	assert(ev3_sta_cyc(CYC_POLL_TASK) == E_OK);
}


void main_task(intptr_t unused) {
	while (!platform_is_ready());

    ev3_font_get_size(MENU_FONT, &default_menu_font_width, &default_menu_font_height);
	while(1) {
		show_cli_menu(&climenu_pb, 0, 0, MENU_FONT);
		const CliMenuEntry *cme = select_menu_entry(&climenu_pb, 0, MENU_FONT_HEIGHT, MENU_FONT);
		if(cme != NULL) {
			assert(cme->handler != NULL);
			cme->handler(cme->exinf);
		}
	}


}

void show_message_box(const char *title, const char *msg, bool_t wait) {
	int offset_x = 0, offset_y = 0;

	// Clear menu area
	ev3_lcd_fill_rect(offset_x, offset_y, EV3_LCD_WIDTH, EV3_LCD_HEIGHT, EV3_LCD_WHITE);

	// Draw title
	if (EV3_LCD_WIDTH - offset_x > strlen(title) * MENU_FONT_WIDTH)
		offset_x += (EV3_LCD_WIDTH - offset_x - strlen(title) * MENU_FONT_WIDTH) / 2;
	ev3_lcd_draw_string(title, offset_x, offset_y);
	ev3_lcd_draw_line(0, offset_y + MENU_FONT_HEIGHT - 1, EV3_LCD_WIDTH, offset_y + MENU_FONT_HEIGHT - 1);
	offset_y += MENU_FONT_HEIGHT;

	// Draw message
	offset_x = MENU_FONT_WIDTH, offset_y += MENU_FONT_HEIGHT;
	while (*msg != '\0') {
		if (*msg == '\n' || offset_x + MENU_FONT_WIDTH > EV3_LCD_WIDTH) { // newline
			offset_x = MENU_FONT_WIDTH;
			offset_y += MENU_FONT_HEIGHT;
		}
		if (*msg != '\n') {
			char buf[2] = { *msg, '\0' };
			ev3_lcd_draw_string(buf, offset_x, offset_y);
			offset_x += MENU_FONT_WIDTH;
		}
		msg++;
	}
	if (wait) {
	// Draw & wait 'OK' button
	ev3_lcd_draw_string("--- OK ---", (EV3_LCD_WIDTH - strlen("--- OK ---") * MENU_FONT_WIDTH) / 2, EV3_LCD_HEIGHT - MENU_FONT_HEIGHT - 5);
	while(!ev3_button_is_pressed(ENTER_BUTTON));
	while(ev3_button_is_pressed(ENTER_BUTTON));
	}
}
