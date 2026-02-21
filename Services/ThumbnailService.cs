using System;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Media.Imaging;
using Rhino.FileIO;

namespace RhinoThumbs.NET.Services
{
    public class ThumbnailService
    {
        public async Task<BitmapSource?> GetThumbnailAsync(string filePath)
        {
            return await Task.Run(() =>
            {
                try
                {
                    if (!File.Exists(filePath)) return null;
                    return ExtractBitmapManually(filePath);
                }
                catch (Exception)
                {
                    return null;
                }
            });
        }

        private BitmapSource? ExtractBitmapManually(string filePath)
        {
            // Try to use the external CLI tool first (most robust for compressed files)
            try
            {
                string appDir = AppDomain.CurrentDomain.BaseDirectory;
                string cliPath = Path.Combine(appDir, "ThumbnailExtractorCLI.exe");
                
                if (File.Exists(cliPath))
                {
                    string tempBmp = Path.GetTempFileName();
                    
                    var startInfo = new System.Diagnostics.ProcessStartInfo
                    {
                        FileName = cliPath,
                        Arguments = $"\"{filePath}\" \"{tempBmp}\"",
                        UseShellExecute = false,
                        CreateNoWindow = true,
                        RedirectStandardError = true
                    };

                    using (var process = System.Diagnostics.Process.Start(startInfo))
                    {
                        if (process != null)
                        {
                            process.WaitForExit(5000); // 5 sec timeout
                            if (process.ExitCode == 0 && File.Exists(tempBmp) && new FileInfo(tempBmp).Length > 0)
                            {
                                try
                                {
                                    var bitmap = new BitmapImage();
                                    bitmap.BeginInit();
                                    bitmap.CacheOption = BitmapCacheOption.OnLoad;
                                    bitmap.UriSource = new Uri(tempBmp);
                                    bitmap.EndInit();
                                    bitmap.Freeze();
                                    
                                    // Cleanup temp file
                                    try { File.Delete(tempBmp); } catch { }
                                    
                                    return bitmap;
                                }
                                catch { }
                            }
                        }
                    }
                    try { if (File.Exists(tempBmp)) File.Delete(tempBmp); } catch { }
                }
            }
            catch { }

            // Fallback to manual scan (only works for uncompressed files)
            try
            {
                using (var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (var reader = new BinaryReader(fs))
                {
                    // Scan first 200KB for BMP signature "BM" (0x42, 0x4D).
                    var buffer = new byte[1024 * 200]; 
                    int bytesRead = fs.Read(buffer, 0, buffer.Length);
                    
                    for (int i = 0; i < bytesRead - 54; i++)
                    {
                        if (buffer[i] == 0x42 && buffer[i + 1] == 0x4D)
                        {
                            int bmpSize = BitConverter.ToInt32(buffer, i + 2);
                            
                            // Basic validation
                            if (bmpSize > 0 && bmpSize < 1024 * 1024 * 5 && (i + bmpSize) <= fs.Length)
                            {
                                try
                                {
                                    var ms = new MemoryStream();
                                    if (i + bmpSize <= bytesRead)
                                    {
                                        ms.Write(buffer, i, bmpSize);
                                    }
                                    else
                                    {
                                        fs.Seek(i, SeekOrigin.Begin);
                                        var fullBmp = new byte[bmpSize];
                                        fs.Read(fullBmp, 0, bmpSize);
                                        ms.Write(fullBmp, 0, bmpSize);
                                    }
                                    
                                    ms.Position = 0;
                                    var decoder = new BmpBitmapDecoder(ms, BitmapCreateOptions.PreservePixelFormat, BitmapCacheOption.OnLoad);
                                    
                                    if (decoder.Frames.Count > 0)
                                    {
                                        var frame = decoder.Frames[0];
                                        frame.Freeze(); 
                                        return frame;
                                    }
                                }
                                catch
                                {
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
            catch { }
            return null;
        }
    }
}
