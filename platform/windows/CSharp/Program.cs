using System.Globalization;
using DesktopTempNotif;

// Always use invariant culture so decimal separator is '.' regardless of system locale
CultureInfo.DefaultThreadCurrentCulture   = CultureInfo.InvariantCulture;
CultureInfo.DefaultThreadCurrentUICulture = CultureInfo.InvariantCulture;

bool verbose     = false;
bool listSensors = false;
string? cfgPath  = null;

foreach (var arg in args)
{
    if (arg == "--verbose" || arg == "-v")
        verbose = true;
    else if (arg == "--list-sensors")
        listSensors = true;
    else
        cfgPath = arg;
}

using var reader = new SensorReader();

if (listSensors)
{
    var sensors  = reader.ListAllTemperatures();
    var dumpPath = "sensors_dump.txt";
    var lines    = new List<string>
    {
        $"# Temperature sensors — {DateTime.Now:yyyy-MM-dd HH:mm:ss}",
        $"# Total: {sensors.Count}",
        "#",
        $"# {"Hardware",-40} {"Type",-15} {"Sensor Name",-35} Temp (°C)",
        $"# {new string('-', 40)} {new string('-', 15)} {new string('-', 35)} ---------",
    };
    foreach (var s in sensors)
        lines.Add($"  {s.HardwareName,-40} {s.HardwareType,-15} {s.SensorName,-35} {s.Value:F1}");

    File.WriteAllLines(dumpPath, lines);

    foreach (var line in lines)
        Console.WriteLine(line);

    Console.WriteLine();
    Console.WriteLine($"Saved to: {Path.GetFullPath(dumpPath)}");
    return;
}

var config   = cfgPath is not null ? Config.Load(cfgPath) : Config.LoadDefaults();
var notifier = new Notifier();

// Per-sensor cooldown tracking: sensor name → (last level, last notification time)
var lastNotif = new Dictionary<string, (NotificationLevel Level, DateTime Time)>();

Console.WriteLine("[desktop_temp_notif] Starting. Poll interval: " +
    $"{config.PollIntervalSeconds}s, cooldown: {config.NotificationCooldownSeconds}s");

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
