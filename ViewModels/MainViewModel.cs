using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Win32;
using RhinoThumbs.NET.Services;
using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;

namespace RhinoThumbs.NET.ViewModels
{
    public partial class MainViewModel : ObservableObject
    {
        private readonly ThumbnailService _thumbnailService;
        private ObservableCollection<ThumbnailItemViewModel> _allFiles = new();
        private CancellationTokenSource? _searchCts;
        private string _currentFolderPath = string.Empty;

        public ObservableCollection<FileSystemItemViewModel> Drives { get; } = new();
        public ObservableCollection<ThumbnailItemViewModel> Files { get; } = new();

        [ObservableProperty]
        private string _statusMessage = "Ready";

        [ObservableProperty]
        private bool _isLoading;

        [ObservableProperty]
        private ThumbnailItemViewModel? _selectedFile;

        private string _searchText = string.Empty;
        public string SearchText
        {
            get => _searchText;
            set
            {
                if (SetProperty(ref _searchText, value))
                {
                    PerformSearch();
                }
            }
        }

        public MainViewModel()
        {
            _thumbnailService = new ThumbnailService();
            LoadDrives();
        }

        private void LoadDrives()
        {
            foreach (var drive in DriveInfo.GetDrives())
            {
                try
                {
                    if (drive.IsReady || drive.DriveType == DriveType.Network)
                    {
                        string label = string.Empty;
                        try
                        {
                            if (drive.IsReady)
                            {
                                label = drive.VolumeLabel;
                            }
                        }
                        catch
                        {
                            // Some mapped network drives might throw when reading VolumeLabel even if IsReady is true
                        }

                        string displayName = string.IsNullOrEmpty(label) ? drive.Name : $"{label} ({drive.Name})";
                        Drives.Add(new FileSystemItemViewModel(drive.Name, displayName, true, OnFolderSelected));
                    }
                }
                catch
                {
                    // Ignore inaccessible drives mapping exception
                }
            }
        }

        private void OnFolderSelected(string path)
        {
            _currentFolderPath = path;
            
            if (string.IsNullOrEmpty(SearchText))
            {
                // If it's already empty, the setter won't fire, so we invoke search manually
                PerformSearch();
            }
            else
            {
                // This will trigger PerformSearch() via the property setter
                SearchText = string.Empty; 
            }
        }

        private async void PerformSearch()
        {
            if (string.IsNullOrEmpty(_currentFolderPath)) return;

            // Cancel any previous search
            _searchCts?.Cancel();
            _searchCts = new CancellationTokenSource();
            var token = _searchCts.Token;

            Files.Clear();
            _allFiles.Clear();
            IsLoading = true;
            
            bool isSearching = !string.IsNullOrWhiteSpace(SearchText);
            string searchLower = isSearching ? SearchText.ToLowerInvariant() : string.Empty;
            
            StatusMessage = isSearching ? $"Searching {_currentFolderPath}..." : $"Loading {_currentFolderPath}...";

            try
            {
                await Task.Run(() => SearchDirectory(_currentFolderPath, searchLower, isSearching, token), token);

                if (!token.IsCancellationRequested)
                {
                    StatusMessage = isSearching ? $"{Files.Count} matching items" : $"{Files.Count} items";
                }
            }
            catch (OperationCanceledException)
            {
                // Expected when user types new letters or changes folder
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
            }
            finally
            {
                if (!token.IsCancellationRequested)
                {
                    IsLoading = false;
                }
            }
        }

        private void SearchDirectory(string path, string searchLower, bool recursive, CancellationToken token)
        {
            if (token.IsCancellationRequested) return;

            try
            {
                var dirInfo = new DirectoryInfo(path);

                // Check files in current dir
                var files = dirInfo.EnumerateFiles("*.3dm");
                foreach (var file in files)
                {
                    if (token.IsCancellationRequested) return;

                    if (string.IsNullOrEmpty(searchLower) || file.Name.ToLowerInvariant().Contains(searchLower))
                    {
                        Application.Current.Dispatcher.InvokeAsync(() =>
                        {
                            if (token.IsCancellationRequested) return;

                            var item = new ThumbnailItemViewModel(file.FullName);
                            Files.Add(item);
                            _ = LoadThumbnailAsync(item);
                            
                            // Optional live update
                            // StatusMessage = recursive ? $"Found {Files.Count} items..." : $"{Files.Count} items";
                        }, DispatcherPriority.Background);
                    }
                }

                if (recursive)
                {
                    // Recurse into subdirectories
                    var subDirs = dirInfo.EnumerateDirectories();
                    foreach (var subDir in subDirs)
                    {
                        if (token.IsCancellationRequested) return;

                        // Skip hidden/system directories
                        if ((subDir.Attributes & (FileAttributes.Hidden | FileAttributes.System)) == 0)
                        {
                            SearchDirectory(subDir.FullName, searchLower, recursive, token);
                        }
                    }
                }
            }
            catch (UnauthorizedAccessException) { /* Ignore permissions errors */ }
            catch (PathTooLongException) { /* Ignore path errors */ }
            catch (Exception) { /* Ignore other enumeration errors safely */ }
        }

        private async Task LoadThumbnailAsync(ThumbnailItemViewModel item)
        {
            item.IsLoading = true;
            var thumb = await _thumbnailService.GetThumbnailAsync(item.FilePath);
            if (thumb != null)
            {
                item.Thumbnail = thumb;
            }
            item.IsLoading = false;
        }

        [RelayCommand]
        private void ClearSearch()
        {
            SearchText = string.Empty;
        }
        
        [RelayCommand]
        private void Copy(object? item)
        {
            string? path = null;
            if (item is ThumbnailItemViewModel thumb) path = thumb.FilePath;
            else if (item is FileSystemItemViewModel folder) path = folder.FullPath;

            if (path != null)
            {
                var dropList = new System.Collections.Specialized.StringCollection { path };
                Clipboard.SetFileDropList(dropList);
            }
        }

        [RelayCommand]
        private void OpenInExplorer(object? item)
        {
            string? path = null;
            if (item is ThumbnailItemViewModel thumb) path = thumb.FilePath;
            else if (item is FileSystemItemViewModel folder) path = folder.FullPath;

            if (path != null)
            {
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "explorer.exe",
                    Arguments = $"/select,\"{path}\"",
                    UseShellExecute = true
                });
            }
        }
        [RelayCommand]
        private void Compress7z(object? item)
        {
            string? path = null;
            if (item is ThumbnailItemViewModel thumb) path = thumb.FilePath;
            else if (item is FileSystemItemViewModel folder) path = folder.FullPath;

            if (path != null && (File.Exists(path) || Directory.Exists(path)))
            {
                var isDirectory = Directory.Exists(path);
                var defaultFileName = isDirectory ? new DirectoryInfo(path).Name : Path.GetFileNameWithoutExtension(path);
                
                var dialog = new SaveFileDialog
                {
                    Title = "Save Compressed Archive",
                    Filter = "7-Zip Archive (*.7z)|*.7z",
                    FileName = $"{defaultFileName}.7z"
                };

                if (dialog.ShowDialog() == true)
                {
                    string targetPath = dialog.FileName;
                    // The project file copies Tools\7za.exe next to the app's .exe; also accept a Tools subfolder.
                    string sevenZipPath = System.IO.Path.Combine(AppContext.BaseDirectory, "7za.exe");
                    if (!File.Exists(sevenZipPath))
                        sevenZipPath = System.IO.Path.Combine(AppContext.BaseDirectory, "Tools", "7za.exe");

                    if (!File.Exists(sevenZipPath))
                    {
                        MessageBox.Show("The bundled 7-Zip component (7za.exe) is missing from the application folder.", "Missing Dependency", MessageBoxButton.OK, MessageBoxImage.Warning);
                        return;
                    }

                    try
                    {
                        var processInfo = new System.Diagnostics.ProcessStartInfo
                        {
                            FileName = sevenZipPath,
                            Arguments = $"a \"{targetPath}\" \"{path}\"",
                            UseShellExecute = false,
                            CreateNoWindow = true
                        };
                        
                        // Launch 7-zip in background
                        StatusMessage = $"Compressing {defaultFileName}...";
                        Task.Run(() => 
                        {
                            using (var process = System.Diagnostics.Process.Start(processInfo))
                            {
                                process?.WaitForExit();
                                
                                Application.Current.Dispatcher.Invoke(() => 
                                {
                                    StatusMessage = $"Saved {targetPath}";
                                });
                            }
                        });
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Failed to compress file:\n{ex.Message}", "Compression Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
            }
        }

        [RelayCommand]
        private void OpenViewer(ThumbnailItemViewModel item)
        {
            if (item == null || string.IsNullOrEmpty(item.FilePath)) return;

            var viewer = new RhinoThumbs.NET.Views.ViewerWindow(item.FilePath);
            viewer.Owner = Application.Current.MainWindow;
            viewer.Show();
        }
    }
}
