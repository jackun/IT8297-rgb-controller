#pragma once
#include <dbus/dbus.h>

#include <thread>
#include <functional>

namespace dbusmgr {

using callback_func = std::function<void(bool)>;

class dbus_error : public std::runtime_error
{
public:
	dbus_error(DBusError *src) : std::runtime_error(src->message)
	{
		dbus_move_error (src, &error);
	}
	virtual ~dbus_error() { dbus_error_free (&error); }
private:
	DBusError error;
};

class dbus_manager
{
public:
	dbus_manager() {}
	~dbus_manager();

	void init(callback_func func);
	void connect_to_signals();
	void disconnect_from_signals();
	void uninhibit();
	void inhibit();

private:
	void stop_thread();
	void start_thread();
	static void dbus_thread(dbus_manager *pmgr);

	DBusError error;
	DBusConnection * dbus_conn = nullptr;
	DBusMessage * dbus_msg = nullptr;
	DBusMessage * dbus_reply = nullptr;
	int inhibit_fd = -1;
	bool quit = false;
	std::thread thread;
	callback_func cb_func;
};

}