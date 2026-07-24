#ifdef ENABLE_INOTIFY
#include <memory>

#include <sys/inotify.h>
#include <SDL/SDL.h>
#include <unistd.h>

#include "debug.h"
#include "inputmanager.h"
#include "mediamonitor.h"
#include "menu.h"
#include "utilities.h"

using namespace std;

MediaMonitor::MediaMonitor(string dir, Menu *menu) :
	Monitor(dir, menu, IN_MOVE | IN_DELETE | IN_CREATE | IN_ONLYDIR)
{
}

bool MediaMonitor::event_accepted(
			struct inotify_event &event __attribute__((unused)))
{
	return true;
}

void MediaMonitor::inject_event(bool is_add, const char *path)
{
	/* Sleep for a bit, to ensure that the media will be mounted
	 * on the mountpoint before we start looking for OPKs */
	sleep(1);

	if (is_add)
		//menu->openPackagesFromDir(((string)path + "/apps").c_str());
		menu->openPackagesFromDir(((string)path).c_str());
	else
		menu->removePackageLink(path);

	request_repaint();
}

#endif /* ENABLE_INOTIFY */
