using DesktopTempNotif;

bool verbose    = false;
string? cfgPath = null;

foreach (var arg in args)
{
    if (arg == "--verbose" || arg == "-v")
        verbose = true;
    else
        cfgPath = arg;
}

var config  = cfgPath is not null ? Config.Load(cfgPath) : Config.LoadDefaults();
var notifier = new Notifier();

// Per-sensor cooldown tracking: sensor name → (last level, last notification time)
var lastNotif = new Dictionary<string, (NotificationLevel Level, DateTime Time)>();

Console.WriteLine("[desktop_temp_notif] Starting. Poll interval: " +
    $"{config.PollIntervalSeconds}s, cooldown: {config.NotificationCooldownSeconds}s");

using var reader = new SensorReader();

while (true)
{
    var readings = reader.Read();

    foreach (var (sensor, threshold) in config.Thresholds)
    {
        if (!readings.TryGetValue(sensor, out float temp))
        {
            if (verbose)
                Console.WriteLine($"[skip] {sensor}: not found in sensor data");
            continue;
        }

        NotificationLevel? level = null;
        if (temp >= threshold.Critical)
            level = NotificationLevel.Critical;
        else if (temp >= threshold.Warning)
            level = NotificationLevel.Warning;

        if (level is null)
        {
            if (verbose)
                Console.WriteLine($"[ok]   {sensor}: {temp:F1}°C (below warning {threshold.Warning:F1}°C)");
            continue;
        }

        // Check cooldown
        if (lastNotif.TryGetValue(sensor, out var prev))
        {
            bool withinCooldown = (DateTime.UtcNow - prev.Time).TotalSeconds < config.NotificationCooldownSeconds;
            bool notEscalating  = level <= prev.Level;
            if (withinCooldown && notEscalating)
            {
                if (verbose)
                    Console.WriteLine($"[skip] {sensor}: {temp:F1}°C — cooldown active");
                continue;
            }
        }

        float usedThreshold = level == NotificationLevel.Critical
            ? threshold.Critical
            : threshold.Warning;

        Console.WriteLine($"[{level!.ToString()!.ToUpperInvariant()}] {sensor}: {temp:F1}°C " +
            $"(threshold: {usedThreshold:F1}°C)");

        notifier.Send(sensor, temp, usedThreshold, level.Value);
        lastNotif[sensor] = (level.Value, DateTime.UtcNow);
    }

    if (verbose)
        Console.WriteLine($"[poll] sleeping {config.PollIntervalSeconds}s...");

    Thread.Sleep(config.PollIntervalSeconds * 1000);
}
