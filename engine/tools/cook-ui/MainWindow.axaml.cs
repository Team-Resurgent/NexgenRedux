using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Text.Json;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;

namespace NexgenCook;

public partial class MainWindow : Window
{
    private readonly ObservableCollection<Row> _rows = [];
    private readonly DispatcherTimer _settle = new() { Interval = TimeSpan.FromSeconds(0.6) };
    private CancellationTokenSource? _stop;
    private FileSystemWatcher? _watcher;
    private bool _ownTo;        // whether "To" was chosen, and not only suggested
    private bool _suggesting;

    public bool Busy => _stop is not null;

    // Where it is only shown, as in the video, nothing is kept.
    public static bool Remember { get; set; } = true;

    public MainWindow()
    {
        InitializeComponent();
        Rows.ItemsSource = _rows;
        FromFolder.Click += async (_, _) => await ChooseFolder("The media folder", SetFrom);
        FromFile.Click += async (_, _) => await ChooseFile();
        ToFolder.Click += async (_, _) => await ChooseFolder("The folder to make", p => To.Text = p);
        From.TextChanged += (_, _) => Suggest();
        To.TextChanged += (_, _) => _ownTo = _suggesting ? _ownTo : Target.Length > 0;
        Platform.SelectionChanged += (_, _) => Suggest();
        Go.Click += (_, _) =>
        {
            if (Busy)
            {
                _stop!.Cancel();
            }
            else
            {
                _ = Cook();
            }
        };
        Open.Click += (_, _) => ShowFolder();
        Watch.IsCheckedChanged += (_, _) => Rewatch();
        // Saving often writes a file more than once.
        _settle.Tick += (_, _) =>
        {
            if (!Busy)
            {
                _settle.Stop();
                _ = Cook();
            }
        };
        AddHandler(DragDrop.DragOverEvent, (_, e) => e.DragEffects = e.DataTransfer.Contains(DataFormat.File) ? DragDropEffects.Copy : DragDropEffects.None);
        AddHandler(DragDrop.DropEvent, (_, e) =>
        {
            if (e.DataTransfer.TryGetFiles()?.FirstOrDefault()?.TryGetLocalPath() is { } path)
            {
                SetFrom(path);
            }
        });
        Load();
        Closing += (_, _) =>
        {
            _stop?.Cancel();
            _watcher?.Dispose();
            Save();
        };
    }

    public void SetFrom(string path)
    {
        _ownTo = false;
        From.Text = path;
    }

    private string Source => (From.Text ?? "").Trim().Trim('"');
    private string Target => (To.Text ?? "").Trim().Trim('"');

    // Beside it, unless one was chosen.
    private void Suggest()
    {
        if (_ownTo || Source.Length == 0)
        {
            return;
        }
        string trimmed = Path.TrimEndingDirectorySeparator(Source);
        _suggesting = true;
        if (Directory.Exists(trimmed))
        {
            To.Text = trimmed + (Platform.SelectedIndex == 1 ? "-xbox" : "-cooked");
        }
        else if (File.Exists(trimmed))
        {
            To.Text = Path.GetDirectoryName(trimmed);
        }
        _suggesting = false;
    }

    private async Task ChooseFolder(string title, Action<string> chosen)
    {
        IReadOnlyList<IStorageFolder> folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions { Title = title });
        if (folders.FirstOrDefault()?.TryGetLocalPath() is { } path)
        {
            chosen(path);
        }
    }

    private async Task ChooseFile()
    {
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "A model, an image or a sound",
            FileTypeFilter =
            [
                new FilePickerFileType("All that can be cooked") { Patterns = ["*.glb", "*.gltf", "*.fbx", "*.obj", "*.stl", "*.ply", "*.png", "*.jpg", "*.jpeg", "*.tga", "*.bmp", "*.gif", "*.wav", "*.ogg", "*.mp3"] },
                FilePickerFileTypes.All,
            ],
        });
        if (files.FirstOrDefault()?.TryGetLocalPath() is { } path)
        {
            SetFrom(path);
        }
    }

    public async Task Cook()
    {
        if (Busy)
        {
            return;
        }
        if (Source.Length == 0 || Target.Length == 0)
        {
            Say("Choose what to cook, and the folder to make.", true);
            return;
        }
        List<string> arguments = ["--platform", Platform.SelectedIndex == 1 ? "xboxog" : "desktop"];
        foreach ((CheckBox box, string what) in new[] { (Images, "images"), (Sounds, "sounds"), (Models, "models") })
        {
            if (box.IsChecked != true)
            {
                arguments.AddRange(["--skip", what]);
            }
        }
        if (Force.IsChecked == true)
        {
            arguments.Add("--force");
        }
        // Whole, so that a name that begins with a dash isn't an option.
        arguments.AddRange([Path.GetFullPath(Source), Path.GetFullPath(Target)]);
        CancellationTokenSource stop = _stop = new CancellationTokenSource();
        _rows.Clear();
        Empty.IsVisible = false;
        Go.Content = "Stop";
        Progress.IsIndeterminate = true;
        Progress.Maximum = 1;
        Progress.Value = 0;
        Say("Cooking…");
        int seen = 0;
        Cooker cooker = new()
        {
            OnCount = count => Dispatcher.UIThread.Post(() =>
            {
                Progress.IsIndeterminate = false;
                Progress.Maximum = Math.Max(1, count);
            }),
            OnRow = row => Dispatcher.UIThread.Post(() =>
            {
                Progress.Value = ++seen;
                if (row.What != "unchanged")
                {
                    // What went wrong is at the top, where it is seen.
                    _rows.Insert(row.Failed ? _rows.Count(r => r.Failed) : _rows.Count, row);
                    Rows.ScrollIntoView(_rows[^1]);
                }
            }),
        };
        Result? result = await Task.Run(() => cooker.Run(arguments, stop.Token));
        await Dispatcher.UIThread.InvokeAsync(() => { }, DispatcherPriority.Background);
        _stop = null;
        stop.Dispose();
        Go.Content = "Cook";
        Progress.IsIndeterminate = false;
        Progress.Value = result is null ? 0 : Progress.Maximum;
        if (result is null)
        {
            Say(cooker.Error, true);
        }
        else
        {
            if (result.Failed > 0 && _rows.Count > 0)
            {
                Rows.ScrollIntoView(_rows[0]);
            }
            string saved = result.Before > 0 && result.After < result.Before ? $", {100 - result.After * 100 / result.Before}% smaller" : "";
            Say($"{result.Cooked} cooked, {result.Copied} copied, {result.Unchanged} unchanged" + (result.Removed > 0 ? $", {result.Removed} removed" : "") + (result.Failed > 0 ? $", {result.Failed} failed" : "") +
                $".  {Row.Kilobytes(result.Before)} became {Row.Kilobytes(result.After)}{saved}." + (Watch.IsChecked == true ? "  Waiting for changes." : ""), result.Failed > 0);
            Open.IsEnabled = true;
        }
        Rewatch();
        Save();
    }

    private void Say(string text, bool wrong = false)
    {
        Status.Text = text;
        Status.Foreground = new SolidColorBrush(Color.Parse(wrong ? "#F0625D" : "#DCE3F2"));
    }

    private void Rewatch()
    {
        _watcher?.Dispose();
        _watcher = null;
        _settle.Stop();
        if (Watch.IsChecked != true || !Directory.Exists(Source))
        {
            return;
        }
        void Changed() => Dispatcher.UIThread.Post(() =>
        {
            _settle.Stop();
            _settle.Start();
        });
        _watcher = new FileSystemWatcher(Source) { IncludeSubdirectories = true, NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite | NotifyFilters.Size };
        _watcher.Changed += (_, _) => Changed();
        _watcher.Created += (_, _) => Changed();
        _watcher.Deleted += (_, _) => Changed();
        _watcher.Renamed += (_, _) => Changed();
        _watcher.EnableRaisingEvents = true;
    }

    private void ShowFolder()
    {
        string folder = Directory.Exists(Target) ? Target : Path.GetDirectoryName(Target) ?? "";
        if (!Directory.Exists(folder))
        {
            return;
        }
        string with = OperatingSystem.IsWindows() ? "explorer.exe" : OperatingSystem.IsMacOS() ? "open" : "xdg-open";
        try
        {
            Process.Start(new ProcessStartInfo(with) { ArgumentList = { Path.GetFullPath(folder) } })?.Dispose();
        }
        catch (System.ComponentModel.Win32Exception)
        {
            Say($"The folder is {folder}");
        }
    }

    // --- From one time to the next ---

    private static string Kept => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "NexgenCook", "settings.json");

    private void Load()
    {
        if (!Remember)
        {
            return;
        }
        try
        {
            using JsonDocument j = JsonDocument.Parse(File.ReadAllText(Kept));
            string Text(string key) => j.RootElement.TryGetProperty(key, out JsonElement v) && v.ValueKind == JsonValueKind.String ? v.GetString()! : "";
            Platform.SelectedIndex = Text("platform") == "xboxog" ? 1 : 0;
            From.Text = Text("from");
            To.Text = Text("to");
            _ownTo = Target.Length > 0;
        }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
        {
            // The first time.
        }
    }

    private void Save()
    {
        if (!Remember)
        {
            return;
        }
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Kept)!);
            using FileStream file = File.Create(Kept);
            using Utf8JsonWriter j = new(file);
            j.WriteStartObject();
            j.WriteString("platform", Platform.SelectedIndex == 1 ? "xboxog" : "desktop");
            j.WriteString("from", Source);
            j.WriteString("to", Target);
            j.WriteEndObject();
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            // It is only asked again.
        }
    }
}
