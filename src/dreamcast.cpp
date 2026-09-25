#ifdef __DREAMCAST__

#include "dreamcast.h"
#include "setup.h"
#ifdef PROFILE_INPUT_PLAYBACK
#include "menu.h"
#endif
#include <dc/fs_vmu.h>
#include <mp3/sndserver.h>

static bool mp3_initialized = false;

bool dreamcast_default_60hz()
{
	return flashrom_get_region() != FLASHROM_REGION_EUROPE;
}

int dreamcast_mp3_start(const char* file, int loop)
{
	if (!mp3_initialized)
	{
		if (snd_stream_init() < 0 || mp3_init() < 0)
			return -1;
		mp3_initialized = true;
	}

	return mp3_start(file, loop);
}

void dreamcast_mp3_stop()
{
	if (mp3_initialized)
		mp3_stop();
}

void dreamcast_mp3_shutdown()
{
	if (!mp3_initialized)
		return;

	mp3_stop();
	mp3_shutdown();
	mp3_initialized = false;
}

std::string loadFromVMU(FILE* f)
{
	std::string data;
	int c;

	setvbuf(f, NULL, _IONBF, 0);
	while((c = fgetc(f)) != EOF && c != '\0')
		data += (char)c;

	return data;
}

void saveToVMU(FILE* f, const char* data, const char* longdesc)
{
	vmu_pkg_t pkg;
	size_t data_len = strlen(data);
	memset(&pkg, 0, sizeof(pkg));

	snprintf(pkg.desc_short, sizeof(pkg.desc_short), "SuperTux");
	snprintf(pkg.desc_long, sizeof(pkg.desc_long), "%s", longdesc);
	snprintf(pkg.app_id, sizeof(pkg.app_id), "SuperTux");
	pkg.icon_cnt = 0;
	pkg.icon_anim_speed = 0;
	pkg.eyecatch_type = VMUPKG_EC_NONE;
	pkg.data_len = data_len;
	pkg.data = (uint8*)data;

	if(fs_vmu_set_header(fileno(f), &pkg) < 0)
		printf("Could not set VMU file metadata\n");

	if(fwrite(data, 1, data_len, f) != data_len)
		printf("Could not write complete VMU save data\n");
}


uint32 lastPressed[4] = {0, 0, 0, 0};
uint32 btns[] = {
	CONT_A,
	CONT_B,
	CONT_X,
	CONT_Y,
	CONT_START,
	CONT_DPAD_UP,
	CONT_DPAD_DOWN,
	CONT_DPAD_LEFT,
	CONT_DPAD_RIGHT,
};
uint32 btnSize = 9;

#ifdef PROFILE_INPUT_PLAYBACK
static DreamcastProfileInputContext profile_input_context = PROFILE_INPUT_TITLE;
static uint64 profile_context_start = 0;
static uint64 profile_press_start = 0;
static uint32 profile_press_button = 0;
static uint64 profile_next_press = 0;

void dreamcast_profile_input_begin()
{
	profile_input_context = PROFILE_INPUT_TITLE;
	profile_context_start = timer_ms_gettime64();
	profile_press_start = 0;
	profile_press_button = 0;
	profile_next_press = profile_context_start + 500;
}

void dreamcast_profile_input_set_context(DreamcastProfileInputContext context)
{
	profile_input_context = context;
	profile_context_start = timer_ms_gettime64();
	profile_press_start = 0;
	profile_press_button = 0;
	profile_next_press = profile_context_start + 500;
}

static uint32 profile_input_buttons()
{
	const uint64 now = timer_ms_gettime64();
	if(profile_press_button)
	{
		if(now - profile_press_start < 100)
			return profile_press_button;
		profile_press_button = 0;
		profile_next_press = now + 250;
		return 0;
	}

	if(now < profile_next_press)
		return 0;

	uint32 button = 0;
	Menu* menu = Menu::current();
	if(menu == vmu_menu)
		button = menu->get_active_item_id() == MNID_SLOTC1 ? CONT_A : CONT_DPAD_DOWN;
	else if(menu == main_menu)
		button = CONT_A;
	else if(menu == load_game_menu)
		button = CONT_A;
	else if(menu == game_menu)
		button = menu->get_active_item_id() == MNID_ABORTLEVEL ? CONT_A : CONT_DPAD_DOWN;
	else if(profile_input_context == PROFILE_INPUT_WORLDMAP)
		button = CONT_A;
	else if(profile_input_context == PROFILE_INPUT_GAME &&
	        now - profile_context_start >= 2000)
		button = CONT_START;
	else if(profile_input_context == PROFILE_INPUT_TITLE)
		button = CONT_START;

	if(button)
	{
		profile_press_button = button;
		profile_press_start = now;
	}
	return button;
}
#endif

uint32 getButtons(int port)
{
#ifdef PROFILE_INPUT_PLAYBACK
	(void)port;
	return profile_input_buttons();
#else
	maple_device_t *cont = maple_enum_type(port, MAPLE_FUNC_CONTROLLER);
	if(!cont)
		return 0;

	cont_state_t *state = (cont_state_t *)maple_dev_status(cont);
	return state ? state->buttons : 0;
#endif
}

// check if a button has been pressed (NOT when held down, useful for menus or something)
uint32 getPressed(int port)
{
	uint32 pressed = lastPressed[port];
	bool update = false;
	uint32 buttons = getButtons(port);

	for (uint32 i=0; i<btnSize; i++)
	{
		if (buttons & btns[i] && !(lastPressed[port] & btns[i]))
		{
			pressed |= btns[i];
			update = true;
		}
		else if (buttons & btns[i] && lastPressed[port] & btns[i])
			pressed &= ~btns[i];
		else if (!(buttons & btns[i]) && lastPressed[port] & btns[i])
		{
			pressed &= ~btns[i];
			update = true;
		}
	}

	if (update)
		lastPressed[port] = pressed;

	return pressed;
}

#endif // __DREAMCAST__
