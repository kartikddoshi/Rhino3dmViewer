using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;

namespace RhinoThumbs.NET.ViewModels
{
    public partial class FileSystemItemViewModel : ObservableObject
    {
        public string FullPath { get; }
        public string DisplayName { get; }
        public ObservableCollection<FileSystemItemViewModel> Children { get; } = new();

        [ObservableProperty]
        private bool _isExpanded;

        [ObservableProperty]
        private bool _isSelected;

        private bool _hasDummyChild;

        public event Action<string>? FolderSelected;

        public FileSystemItemViewModel(string path, string displayName, bool isDrive, Action<string>? onFolderSelected)
        {
            FullPath = path;
            DisplayName = displayName;
            FolderSelected = onFolderSelected;

            if (isDrive)
            {
                // Always allow expansion for drives so disconnected network drives can attempt to connect when expanded.
                Children.Add(new FileSystemItemViewModel("DUMMY", "Loading...", false, null));
                _hasDummyChild = true;
            }
            else if (Directory.Exists(path))
            {
                // Add dummy item to allow expansion
                try
                {
                    if (Directory.EnumerateDirectories(path).Any())
                    {
                        Children.Add(new FileSystemItemViewModel("DUMMY", "Loading...", false, null));
                        _hasDummyChild = true;
                    }
                }
                catch { }
            }
        }

        partial void OnIsExpandedChanged(bool value)
        {
            if (value && _hasDummyChild)
            {
                Children.Clear();
                _hasDummyChild = false;
                LoadChildren();
            }
        }

        partial void OnIsSelectedChanged(bool value)
        {
            if (value)
            {
                FolderSelected?.Invoke(FullPath);
            }
        }

        private void LoadChildren()
        {
            try
            {
                var directories = Directory.GetDirectories(FullPath);
                foreach (var dir in directories)
                {
                    var dirInfo = new DirectoryInfo(dir);
                    if ((dirInfo.Attributes & FileAttributes.Hidden) == FileAttributes.Hidden) continue;

                    Children.Add(new FileSystemItemViewModel(dir, dirInfo.Name, false, FolderSelected));
                }
            }
            catch { }
        }
    }
}
