#include "battery.h"
#include "compat-filesystem.h"
#include "gmenu2x.h"
#include "surfacecollection.h"
#include "utilities.h"

#include <SDL.h>
#include <cstdio>
#include <sstream>


/**
 * Reads the current battery state and returns a number representing its level
 * of charge.
 * @return A number representing battery charge: 0 means fully discharged,
 * 5 means fully charged, 6 represents running on external power.
 */
unsigned short Battery::getBatteryLevel()
{
	unsigned long voltage_min = 0, voltage_max = 1, voltage_now = 1;
	FILE *handle;

	std::string path = powerSupplySysfs + "/status";
	handle = fopen(path.c_str(), "r");
	if (handle) {
		char buf[16];

		buf[sizeof(buf) - 1] = '\0';

		fread(buf, 1, 16, handle);
		fclose(handle);
		if ((std::string)buf == "Charging")
			return 6;
	}

	path = powerSupplySysfs + "/online";
	handle = fopen(path.c_str(), "r");
	if (handle) {
		int usbval = 0;
		fscanf(handle, "%d", &usbval);
		fclose(handle);
		if (usbval == 1)
			return 6;
	}

	path = batterySysfs + "/capacity";
	handle = fopen(path.c_str(), "r");
	if (handle) {
		int battval = 0;
		fscanf(handle, "%d", &battval);
		fclose(handle);

		if (battval>90) return 5;
		if (battval>70) return 4;
		if (battval>50) return 3;
		if (battval>30) return 2;
		if (battval>10) return 1;

		return 0;
	}

	/*
	 * No 'capacity' file in sysfs - Do a dumb approximation of the capacity
	 * using the current voltage reported and the min/max voltages of the
	 * battery.
	 */

	path = batterySysfs + "/voltage_max_design";
	handle = fopen(path.c_str(), "r");
	if (handle) {
		fscanf(handle, "%lu", &voltage_max);
		fclose(handle);
	}

	path = batterySysfs + "/voltage_min_design";
	handle = fopen(path.c_str(), "r");
	if (handle) {
		fscanf(handle, "%lu", &voltage_min);
		fclose(handle);
	}

	path = batterySysfs + "/voltage_now";
	handle = fopen(path.c_str(), "r");
	if (handle) {
		fscanf(handle, "%lu", &voltage_now);
		fclose(handle);
	}

	return (voltage_now - voltage_min) * 6 / (voltage_max - voltage_min);
}

Battery::Battery(GMenu2X& gmenu2x) : sc(gmenu2x.sc)
{
	std::vector<std::string> bat_paths;
	std::vector<std::string> pw_paths;

	split(pw_paths, gmenu2x.confStr["powerSupplySysfs"], ",");
	split(bat_paths, gmenu2x.confStr["batterySysfs"], ",");

	/* Default values */
	batterySysfs = "/sys/class/power_supply/BAT0";
	powerSupplySysfs = "/sys/class/power_supply/AC0";

	for (const std::string &path : pw_paths) {
		if (compat::filesystem::is_directory(path)) {
			powerSupplySysfs = path;
			break;
		}
	}

	for (const std::string &path : bat_paths) {
		if (compat::filesystem::is_directory(path)) {
			batterySysfs = path;
			break;
		}
	}

	lastUpdate = SDL_GetTicks();
	update();
}

const OffscreenSurface *Battery::getIcon()
{
	// Check battery status every 60 seconds.
	unsigned int now = SDL_GetTicks();
	if (now - lastUpdate >= 60000) {
		lastUpdate = now;
		update();
	}

	return sc.skinRes(iconPath);
}

void Battery::update()
{
	unsigned short battlevel = getBatteryLevel();
	if (battlevel > 5) {
		iconPath = "imgs/battery/ac.png";
	} else {
		std::stringstream ss;
		ss << "imgs/battery/" << battlevel << ".png";
		ss >> iconPath;
	}
}
