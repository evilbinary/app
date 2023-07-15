#ifndef __BATTERY_H__
#define __BATTERY_H__

#include <string>

class GMenu2X;
class OffscreenSurface;
class SurfaceCollection;


/**
 * Keeps track of the battery status.
 */
class Battery {
public:
	Battery(GMenu2X& gmenu2x);

	/**
	 * Gets the icon that reflects the current battery status.
	 */
	const OffscreenSurface *getIcon();

private:
	void update();
	unsigned short getBatteryLevel();

	SurfaceCollection& sc;
	std::string iconPath;
	unsigned int lastUpdate;

	std::string batterySysfs;
	std::string powerSupplySysfs;
};

#endif /* __BATTERY_H__ */
