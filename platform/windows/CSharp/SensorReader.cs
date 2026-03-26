using LibreHardwareMonitor.Hardware;

namespace DesktopTempNotif;

class UpdateVisitor : IVisitor
{
    public void VisitComputer(IComputer computer)
    {
        computer.Traverse(this);
    }

    public void VisitHardware(IHardware hardware)
    {
        hardware.Update();
        foreach (var subHardware in hardware.SubHardware)
            subHardware.Accept(this);
    }

    public void VisitSensor(ISensor sensor) { }
    public void VisitParameter(IParameter parameter) { }
}

class SensorReader : IDisposable
{
    private readonly Computer _computer;
    private readonly UpdateVisitor _visitor = new();

    public SensorReader()
    {
        _computer = new Computer
        {
            IsCpuEnabled    = true,
            IsGpuEnabled    = true,
            IsMemoryEnabled = true,
            IsMotherboardEnabled = true,
        };
        _computer.Open();
    }

    public Dictionary<string, float> Read()
    {
        _computer.Accept(_visitor);

        var result = new Dictionary<string, float>();
        foreach (var hardware in _computer.Hardware)
            CollectTemperatures(hardware, result);
        return result;
    }

    private static void CollectTemperatures(IHardware hardware, Dictionary<string, float> result)
    {
        foreach (var sensor in hardware.Sensors)
        {
            if (sensor.SensorType == SensorType.Temperature && sensor.Value.HasValue)
                result[sensor.Name] = sensor.Value.Value;
        }
        foreach (var sub in hardware.SubHardware)
            CollectTemperatures(sub, result);
    }

    public void Dispose()
    {
        _computer.Close();
    }
}
