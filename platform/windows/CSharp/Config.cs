namespace DesktopTempNotif;

record Threshold(float Warning, float Critical);

class Config
{
    public int PollIntervalSeconds { get; set; } = 30;
    public int NotificationCooldownSeconds { get; set; } = 300;
    public Dictionary<string, Threshold> Thresholds { get; } = new();

    public static Config LoadDefaults()
    {
        var cfg = new Config();
        cfg.Thresholds["Core (Tctl/Tdie)"] = new Threshold(85, 95);
        cfg.Thresholds["DIMM #1"] = new Threshold(50, 80);
        cfg.Thresholds["GPU VR SoC"] = new Threshold(80, 100);
        cfg.Thresholds["Temperature #1"] = new Threshold(60, 79);
        cfg.Thresholds["Temperature #2"] = new Threshold(60, 79);
        return cfg;
    }

    public static Config Load(string path)
    {
        var cfg = LoadDefaults();
        string[] lines;
        try
        {
            lines = File.ReadAllLines(path);
        }
        catch
        {
            Console.Error.WriteLine($"[config] Cannot open '{path}', using defaults.");
            return cfg;
        }

        // Temporary storage for partial threshold entries
        var warnings = new Dictionary<string, float>();
        var criticals = new Dictionary<string, float>();

        foreach (var rawLine in lines)
        {
            var line = rawLine.Trim();
            if (line.Length == 0 || line[0] == '#')
                continue;

            var eqIdx = line.IndexOf('=');
            if (eqIdx < 0)
                continue;

            var key = line[..eqIdx].Trim();
            var value = line[(eqIdx + 1)..].Trim();

            if (key == "poll_interval")
            {
                if (int.TryParse(value, out int v)) cfg.PollIntervalSeconds = v;
            }
            else if (key == "notification_cooldown")
            {
                if (int.TryParse(value, out int v)) cfg.NotificationCooldownSeconds = v;
            }
            else if (key.EndsWith(".warning", StringComparison.OrdinalIgnoreCase))
            {
                var sensor = key[..^".warning".Length];
                if (float.TryParse(value, System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out float f))
                    warnings[sensor] = f;
            }
            else if (key.EndsWith(".critical", StringComparison.OrdinalIgnoreCase))
            {
                var sensor = key[..^".critical".Length];
                if (float.TryParse(value, System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out float f))
                    criticals[sensor] = f;
            }
        }

        // Merge warning/critical pairs into thresholds
        var allSensors = new HashSet<string>(warnings.Keys);
        allSensors.UnionWith(criticals.Keys);
        foreach (var sensor in allSensors)
        {
            warnings.TryGetValue(sensor, out float w);
            criticals.TryGetValue(sensor, out float c);

            // If only one side is specified, carry over the existing default if present
            if (!warnings.ContainsKey(sensor) && cfg.Thresholds.TryGetValue(sensor, out var existing))
                w = existing.Warning;
            if (!criticals.ContainsKey(sensor) && cfg.Thresholds.TryGetValue(sensor, out existing))
                c = existing.Critical;

            cfg.Thresholds[sensor] = new Threshold(w, c);
        }

        return cfg;
    }
}
