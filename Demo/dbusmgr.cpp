#include <cstddef>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include "dbusmgr.h"

namespace dbusmgr {

using ms = std::chrono::milliseconds;

void dbus_manager::init(callback_func func)
{
	cb_func = func;

	::dbus_threads_init_default();

	::dbus_error_init(&error);

	if ( nullptr == (dbus_conn = ::dbus_bus_get(DBUS_BUS_SYSTEM, &error)) ) {
		throw std::runtime_error(error.message);
	}
	std::cout << "Connected to D-Bus as \"" << ::dbus_bus_get_unique_name(dbus_conn) << "\"." << std::endl;

	connect_to_signals();
	inhibit();
}

dbus_manager::~dbus_manager()
{
	// unreference system bus connection instead of closing it
	if (dbus_conn) {
		disconnect_from_signals();
		::dbus_connection_unref(dbus_conn);
		dbus_conn = nullptr;
	}
	uninhibit();
	::dbus_error_free(&error);
}

void dbus_manager::connect_to_signals()
{
	::dbus_bus_add_match(dbus_conn, "type='signal',interface='org.freedesktop.login1.Manager'", &error);
	if (::dbus_error_is_set(&error)) {
		::perror(error.name);
		::perror(error.message);
		::dbus_error_free(&error);
		return;
	}

	start_thread();
}

void dbus_manager::disconnect_from_signals()
{
	::dbus_bus_remove_match(dbus_conn, "type='signal',interface='org.freedesktop.login1.Manager'", &error);
	if (dbus_error_is_set(&error)) {
		::perror(error.name);
		::perror(error.message);
		::dbus_error_free(&error);
	}

	stop_thread();
}

void dbus_manager::uninhibit()
{
	if (inhibit_fd != -1) {
		close(inhibit_fd);
		inhibit_fd = -1;
	}
}

void dbus_manager::inhibit()
{
	const char *v_STRINGS[] = {
		"shutdown:sleep", //what
		"rgblights", //who
		"Disabling rgblights before sleep/shutdown ...", //why
		"delay" //mode: block or delay
	};

	if (nullptr == (dbus_msg = ::dbus_message_new_method_call("org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Inhibit"))) {
		throw std::runtime_error("unable to allocate memory for dbus message");
	}

	if (!dbus_message_append_args (dbus_msg, DBUS_TYPE_STRING, &v_STRINGS[0], DBUS_TYPE_STRING, &v_STRINGS[1],
					DBUS_TYPE_STRING, &v_STRINGS[2], DBUS_TYPE_STRING, &v_STRINGS[3], DBUS_TYPE_INVALID)) {
		::dbus_message_unref(dbus_msg);
		throw dbus_error(&error);
	}

	if (nullptr == (dbus_reply = ::dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_USE_DEFAULT, &error))) {
		::dbus_message_unref(dbus_msg);
		throw dbus_error(&error);
	}

	if (!::dbus_message_get_args(dbus_reply, &error, DBUS_TYPE_UNIX_FD, &inhibit_fd, DBUS_TYPE_INVALID)) {
		::dbus_message_unref(dbus_msg);
		::dbus_message_unref(dbus_reply);
		throw dbus_error(&error);
	}

	::dbus_message_unref(dbus_msg);
	::dbus_message_unref(dbus_reply);
}

void dbus_manager::stop_thread()
{
	quit = true;
	if (thread.joinable())
		thread.join();
}

void dbus_manager::start_thread()
{
	stop_thread();
	quit = false;
	thread = std::thread(dbus_thread, this);
}

void dbus_manager::dbus_thread(dbus_manager *pmgr)
{
	DBusError error;
	DBusMessage *msg = nullptr;
	int arg = 0;

	::dbus_error_init(&error);

	// loop listening for signals being emmitted
	while (!pmgr->quit) {

		// non blocking read of the next available message
		if (!::dbus_connection_read_write(pmgr->dbus_conn, 0))
			return; // connection closed

		msg = ::dbus_connection_pop_message(pmgr->dbus_conn);

		// loop again if we haven't read a message
		if (nullptr == msg) {
			std::this_thread::sleep_for(ms(10));
			continue;
		}

		if (::dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForSleep")
			|| ::dbus_message_is_signal(msg, "org.freedesktop.login1.Manager", "PrepareForShutdown"))
		{

			if (::dbus_message_get_args(msg, &error, DBUS_TYPE_BOOLEAN, &arg, DBUS_TYPE_INVALID)) {
				std::cout << "Got signal with value " << arg << std::endl;
				pmgr->cb_func(arg);
				if (arg)
					pmgr->uninhibit();
				else
					pmgr->inhibit();
			}

			if (dbus_error_is_set(&error)) {
				std::cerr << error.message << std::endl;
				dbus_error_free(&error);
			}
		}

		// free the message
		dbus_message_unref(msg);
	}
}

} // namespace