using Microsoft.Toolkit.Uwp.Notifications;

namespace DesktopTempNotif;

enum NotificationLevel { Warning, Critical }

class Notifier
{
    public void Send(string sensor, float temp, float threshold, NotificationLevel level)
    {
        string levelStr = level == NotificationLevel.Critical ? "CRITICAL" : "WARNING";
        string title    = $"Temperature {levelStr}: {sensor}";
        string body     = $"{sensor} is at {temp:F1}\u00B0C (threshold: {threshold:F1}\u00B0C)";

        new ToastContentBuilder()
            .AddText(title)
            .AddText(body)
            .Show();
    }
}
