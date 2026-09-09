using System;
using System.Diagnostics;
using System.IO;

internal static class JWPLCHMIDesignerLauncher
{
    [STAThread]
    private static void Main()
    {
        string root = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar);
        string script = Path.Combine(root, "Start-JWPLC-HMI-Designer.ps1");

        try
        {
            if (!File.Exists(script))
            {
                Log(root, "No se encontró Start-JWPLC-HMI-Designer.ps1.");
                return;
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + script + "\"",
                WorkingDirectory = root,
                UseShellExecute = false,
                CreateNoWindow = true,
                WindowStyle = ProcessWindowStyle.Hidden
            };

            Process.Start(startInfo);
        }
        catch (Exception ex)
        {
            Log(root, ex.ToString());
        }
    }

    private static void Log(string root, string message)
    {
        try
        {
            File.AppendAllText(
                Path.Combine(root, "launcher.log"),
                DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "  " + message + Environment.NewLine);
        }
        catch
        {
            // El launcher nunca debe bloquear al usuario por un fallo de logging.
        }
    }
}
