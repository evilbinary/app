#ifndef __MONITOR_H__
#define __MONITOR_H__
#ifdef ENABLE_INOTIFY

#include <pthread.h>
#include <string>
#include <sys/inotify.h>

class Menu;

class Monitor {
public:
	Monitor(std::string path, Menu *menu,
		unsigned int flags = IN_MOVE |
		IN_CLOSE_WRITE | IN_DELETE | IN_CREATE |
		IN_DELETE_SELF | IN_MOVE_SELF);
	virtual ~Monitor();

	int run();
	const std::string getPath() { return path; }

private:
	std::string path;
	pthread_t thd;

protected:
	Menu *menu;
	unsigned int mask;
	virtual bool event_accepted(struct inotify_event &event);
	virtual void inject_event(bool is_add, const char *path);
};

#endif
#endif /* __MONITOR_H__ */
