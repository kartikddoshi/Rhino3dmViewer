using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Media3D;
using HelixToolkit.Wpf;
using RhinoThumbs.NET.Services;

namespace RhinoThumbs.NET.Views
{
    public partial class ViewerWindow : Window
    {
        private readonly string _filePath;

        // Measurement State
        private bool _isMeasuring = false;
        private Point3D? _measureStartPoint = null;
        private SphereVisual3D? _startMarker = null;
        private SphereVisual3D? _endMarker = null;
        private LinesVisual3D? _measureLine = null;
        private TextVisual3D? _measureText = null;

        public ViewerWindow(string filePath)
        {
            InitializeComponent();
            _filePath = filePath;
            
            Title = $"3D Preview - {System.IO.Path.GetFileName(filePath)}";
            Loaded += ViewerWindow_Loaded;
            
            // Subscribe to mouse down for measuring
            Viewport3D.MouseDown += Viewport_MouseDown;
        }

        private async void ViewerWindow_Loaded(object sender, RoutedEventArgs e)
        {
            LoadingOverlay.Visibility = Visibility.Visible;

            var modelGroup = await Task.Run(() => GeometryExtractor.ExtractModel(_filePath));

            ModelHost.Content = modelGroup;
            Viewport3D.ZoomExtents();

            LoadingOverlay.Visibility = Visibility.Collapsed;
        }

        // --- CAMERA CONTROLS ---
        
        private void ViewPerspective_Click(object sender, RoutedEventArgs e)
        {
            Viewport3D.Orthographic = false;
            Viewport3D.Camera.LookDirection = new Vector3D(-100, 100, -100);
            Viewport3D.Camera.UpDirection = new Vector3D(0, 0, 1);
            Viewport3D.ZoomExtents();
        }

        private void ViewTop_Click(object sender, RoutedEventArgs e)
        {
            Viewport3D.Orthographic = true;
            Viewport3D.Camera.LookDirection = new Vector3D(0, 0, -100);
            Viewport3D.Camera.UpDirection = new Vector3D(0, 1, 0); // Y is up when looking down Z
            Viewport3D.ZoomExtents();
        }

        private void ViewFront_Click(object sender, RoutedEventArgs e)
        {
            Viewport3D.Orthographic = true;
            Viewport3D.Camera.LookDirection = new Vector3D(0, 100, 0); // Looking along +Y
            Viewport3D.Camera.UpDirection = new Vector3D(0, 0, 1);
            Viewport3D.ZoomExtents();
        }

        private void ViewRight_Click(object sender, RoutedEventArgs e)
        {
            Viewport3D.Orthographic = true;
            Viewport3D.Camera.LookDirection = new Vector3D(-100, 0, 0); // Looking along -X from right
            Viewport3D.Camera.UpDirection = new Vector3D(0, 0, 1);
            Viewport3D.ZoomExtents();
        }

        // --- MEASURING TOOL ---

        private void MeasureToggle_Click(object sender, RoutedEventArgs e)
        {
            _isMeasuring = MeasureToggle.IsChecked == true;
            if (_isMeasuring)
            {
                MeasureToggle.Content = "Measure (Active)";
                Cursor = Cursors.Cross;
            }
            else
            {
                MeasureToggle.Content = "Measure (Off)";
                Cursor = Cursors.Arrow;
                ClearMeasurement();
            }
        }

        private void ClearMeasure_Click(object sender, RoutedEventArgs e)
        {
            ClearMeasurement();
        }

        private void ClearMeasurement()
        {
            if (_measureLine != null) Viewport3D.Children.Remove(_measureLine);
            if (_measureText != null) Viewport3D.Children.Remove(_measureText);
            if (_startMarker != null) Viewport3D.Children.Remove(_startMarker);
            if (_endMarker != null) Viewport3D.Children.Remove(_endMarker);
            
            _measureStartPoint = null;
            _measureLine = null;
            _measureText = null;
            _startMarker = null;
            _endMarker = null;

            ClearMeasureButton.Visibility = Visibility.Collapsed;
        }

        private void Viewport_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (!_isMeasuring || e.ChangedButton != MouseButton.Left) return;

            var hits = Viewport3D.Viewport.FindHits(e.GetPosition(Viewport3D));
            if (hits.Count > 0)
            {
                e.Handled = true; // Prevent camera rotation when measuring
                var point = hits[0].Position;

                if (_measureStartPoint == null)
                {
                    // First Click
                    ClearMeasurement();
                    _measureStartPoint = point;
                    _startMarker = new SphereVisual3D { Center = point, Radius = 0.5, Fill = System.Windows.Media.Brushes.Red };
                    Viewport3D.Children.Add(_startMarker);
                }
                else
                {
                    // Second Click
                    var start = _measureStartPoint.Value;
                    
                    _endMarker = new SphereVisual3D { Center = point, Radius = 0.5, Fill = System.Windows.Media.Brushes.Red };
                    Viewport3D.Children.Add(_endMarker);

                    _measureLine = new LinesVisual3D { Color = System.Windows.Media.Colors.Red, Thickness = 3 };
                    _measureLine.Points.Add(start);
                    _measureLine.Points.Add(point);
                    Viewport3D.Children.Add(_measureLine);

                    double distance = start.DistanceTo(point);
                    var midPoint = new Point3D((start.X + point.X) / 2, (start.Y + point.Y) / 2, (start.Z + point.Z) / 2);

                    _measureText = new TextVisual3D
                    {
                        Position = midPoint,
                        Text = distance.ToString("F2") + " mm",
                        Foreground = System.Windows.Media.Brushes.White,
                        Background = System.Windows.Media.Brushes.Black,
                        Height = 2
                    };
                    Viewport3D.Children.Add(_measureText);

                    ClearMeasureButton.Visibility = Visibility.Visible;
                    _measureStartPoint = null; // Reset for next measurement
                }
            }
        }
    }
}
