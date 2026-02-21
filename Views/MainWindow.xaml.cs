using System.Windows;
using RhinoThumbs.NET.ViewModels;

namespace RhinoThumbs.NET.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            DataContext = new MainViewModel();
        }
    }
}