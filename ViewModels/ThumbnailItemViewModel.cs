using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Windows.Media.Imaging;

namespace RhinoThumbs.NET.ViewModels
{
    public partial class ThumbnailItemViewModel : ObservableObject
    {
        [ObservableProperty]
        private string _fileName;

        [ObservableProperty]
        private string _filePath;

        [ObservableProperty]
        private string _fileSize;

        [ObservableProperty]
        private string _dateModified;

        [ObservableProperty]
        private BitmapSource? _thumbnail;

        [ObservableProperty]
        private bool _isLoading;

        public ThumbnailItemViewModel(string filePath)
        {
            _filePath = filePath;
            _fileName = System.IO.Path.GetFileName(filePath);
            
            var fileInfo = new System.IO.FileInfo(filePath);
            _dateModified = fileInfo.LastWriteTime.ToString("yyMMdd HH:mm");
            
            double sizeInMb = fileInfo.Length / (1024.0 * 1024.0);
            _fileSize = sizeInMb < 1 ? $"{(fileInfo.Length / 1024.0):F2} KB" : $"{sizeInMb:F2} MB";
        }
    }
}
