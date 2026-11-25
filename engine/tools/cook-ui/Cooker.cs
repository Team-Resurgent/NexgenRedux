using System.Diagnostics;
using System.Text;
using System.Text.Json;
using Avalonia.Media;

namespace NexgenCook;

// What cook told of a file.
public sealed class Row
{
    public string File { get; init; } = "";
    public string What { get; init; } = "";
    public string Why { get; init; } = "";
    public long Before { get; init; }
    public long After { get; init; }

    public bool Failed => What == "failed";
    public string Sizes => What is "failed" or "removed" ? "" : $"{Kilobytes(Before)}  →  {Kilobytes(After)}";
    public string Saved => What is "failed" or "removed" or "copied" || Before == 0 ? "" : After <= Before ? $"−{100 - After * 100 / Before}%" : $"+{After * 100 / Before - 100}%";
    public string Detail => Failed ? Why : Sizes;

    public IBrush Tint => SolidColorBrush.Parse(What switch
    {
        "model" => "#F2A65A",
        "image" => "#5AA9F2",
        "sound" => "#8BD17C",
        "failed" => "#F0625D",
        "removed" => "#B08BD9",
        _ => "#77809A",
    });

    public IBrush SavedTint => SolidColorBrush.Parse(After <= Before ? "#8BD17C" : "#77809A");

    public static string Kilobytes(long bytes) => bytes < 1024 * 1024 ? $"{(bytes + 1023) / 1024} KB" : $"{bytes / 1048576.0:0.0} MB";
}

public sealed record Result(int Cooked, int Copied, int Unchanged, int Removed, int Failed, long Before, long After);

// Runs the cook tool, which does all the work, and reads the lines of JSON that it writes.
public sealed class Cooker
{
    public Action<int>? OnCount;
    public Action<Row>? OnRow;
    public string Error { get; private set; } = "";

    public static string? Find()
    {
        string name = OperatingSystem.IsWindows() ? "cook.exe" : "cook";
        string here = AppContext.BaseDirectory;
        string? asked = Environment.GetEnvironmentVariable("NEXGEN_COOK");
        // In a macOS app it is beside the program; unpacked, beside the app.
        string[] places = [asked ?? "", Path.Combine(here, name), Path.Combine(here, "..", "..", "..", name)];
        return places.FirstOrDefault(p => p.Length > 0 && File.Exists(p));
    }

    public async Task<Result?> Run(IEnumerable<string> arguments, CancellationToken stop)
    {
        Error = "";
        string? tool = Find();
        if (tool is null)
        {
            Error = "The cook tool isn't beside this program.";
            return null;
        }
        ProcessStartInfo start = new(tool)
        {
            RedirectStandardOutput = true, RedirectStandardError = true, UseShellExecute = false, CreateNoWindow = true,
            StandardOutputEncoding = new UTF8Encoding(false), StandardErrorEncoding = new UTF8Encoding(false),
        };
        start.ArgumentList.Add("--json");
        foreach (string a in arguments)
        {
            start.ArgumentList.Add(a);
        }
        Result? result = null;
        try
        {
            using Process process = Process.Start(start)!;
            using CancellationTokenRegistration kill = stop.Register(() =>
            {
                try
                {
                    process.Kill();
                }
                catch (InvalidOperationException)
                {
                    // It had ended.
                }
            });
            Task<string> errors = process.StandardError.ReadToEndAsync(CancellationToken.None);
            while (await process.StandardOutput.ReadLineAsync(CancellationToken.None) is { } line)
            {
                result = Read(line) ?? result;
            }
            string said = (await errors).Trim();
            await process.WaitForExitAsync(CancellationToken.None);
            if (stop.IsCancellationRequested)
            {
                Error = "Stopped.";
                result = null;
            }
            else if (result is null && Error.Length == 0)
            {
                Error = said.Length > 0 ? said : $"The cook tool ended with {process.ExitCode}.";
            }
        }
        catch (System.ComponentModel.Win32Exception e)
        {
            Error = e.Message;
        }
        return result;
    }

    private Result? Read(string line)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(line);
            JsonElement j = document.RootElement;
            string Text(string key) => j.TryGetProperty(key, out JsonElement v) ? v.GetString() ?? "" : "";
            long Number(string key) => j.TryGetProperty(key, out JsonElement v) ? v.GetInt64() : 0;
            if (j.TryGetProperty("files", out _))
            {
                OnCount?.Invoke((int)Number("files"));
            }
            else if (j.TryGetProperty("error", out _))
            {
                Error = Text("error");
            }
            else if (j.TryGetProperty("done", out _))
            {
                return new Result((int)Number("cooked"), (int)Number("copied"), (int)Number("unchanged"), (int)Number("removed"), (int)Number("failed"), Number("before"), Number("after"));
            }
            else
            {
                OnRow?.Invoke(new Row { File = Text("file"), What = Text("what"), Why = Text("why"), Before = Number("before"), After = Number("after") });
            }
        }
        catch (JsonException)
        {
            // Something else that it said.
        }
        return null;
    }
}
