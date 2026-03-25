#include "linux_notifier.hpp"
#include <iostream>
#include <sstream>

#ifdef USE_LIBNOTIFY
#include <libnotify/notify.h>

LinuxNotifier::LinuxNotifier() {
    notify_init("desktop_temp_notif");
}

LinuxNotifier::~LinuxNotifier() {
    notify_uninit();
}

void LinuxNotifier::send(const std::string& sensor, float temp, float threshold,
                         NotificationLevel level) {
    std::ostringstream title, body;
    title << "Temperature " << (level == NotificationLevel::Critical ? "CRITICAL" : "WARNING")
          << ": " << sensor;
    body  << sensor << " is at " << temp << "\xc2\xb0""C"
          << " (threshold: " << threshold << "\xc2\xb0""C)";

    const char* icon    = (level == NotificationLevel::Critical) ? "dialog-error"
                                                                  : "dialog-warning";
    NotifyUrgency urgency = (level == NotificationLevel::Critical) ? NOTIFY_URGENCY_CRITICAL
                                                                    : NOTIFY_URGENCY_NORMAL;

    NotifyNotification* n = notify_notification_new(title.str().c_str(),
                                                    body.str().c_str(),
                                                    icon);
    notify_notification_set_urgency(n, urgency);

    GError* err = nullptr;
    if (!notify_notification_show(n, &err)) {
        std::cerr << "notify error: " << err->message << "\n";
        g_error_free(err);
    }
    g_object_unref(n);
}

#else  // USE_LIBNOTIFY — fall back to notify-send via fork/exec

#include <sys/wait.h>
#include <unistd.h>

void LinuxNotifier::send(const std::string& sensor, float temp, float threshold,
                         NotificationLevel level) {
    std::ostringstream title, body;
    title << "Temperature " << (level == NotificationLevel::Critical ? "CRITICAL" : "WARNING")
          << ": " << sensor;
    body  << sensor << " is at " << temp << "\xc2\xb0""C"
          << " (threshold: " << threshold << "\xc2\xb0""C)";

    const char* urgency = (level == NotificationLevel::Critical) ? "critical" : "normal";

    pid_t pid = fork();
    if (pid == 0) {
        // child: exec notify-send with each arg as a separate string (no shell injection)
        execlp("notify-send", "notify-send",
               "--app-name=TempMonitor",
               ("--urgency=" + std::string(urgency)).c_str(),
               title.str().c_str(),
               body.str().c_str(),
               nullptr);
        _exit(1); // exec failed
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    } else {
        std::cerr << "fork failed for notify-send\n";
    }
}

#endif // USE_LIBNOTIFY
