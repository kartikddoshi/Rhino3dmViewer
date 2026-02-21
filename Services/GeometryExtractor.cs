using System;
using System.Collections.Generic;
using System.Windows.Media.Media3D;
using Rhino.FileIO;
using Rhino.Geometry;

namespace RhinoThumbs.NET.Services
{
    public class GeometryExtractor
    {
        public static Model3DGroup ExtractModel(string filePath)
        {
            var modelGroup = new Model3DGroup();
            
            try
            {
                using (var file3dm = File3dm.Read(filePath))
                {
                    if (file3dm == null) return modelGroup;

                    var brush = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(200, 200, 200));
                    brush.Freeze();
                    var material = new DiffuseMaterial(brush);
                    material.Freeze();

                    foreach (var obj in file3dm.Objects)
                    {
                        var geometry = obj.Geometry;
                        Mesh? displayMesh = null;

                        if (geometry is Mesh rhinoMesh)
                        {
                            displayMesh = rhinoMesh;
                        }
                        else if (geometry is Extrusion extrusion)
                        {
                            displayMesh = extrusion.GetMesh(MeshType.Render);
                        }
                        else if (geometry is Brep brep)
                        {
                            // Try to get existing render meshes to avoid expensive meshing
                            var meshes = new List<Mesh>();
                            foreach (var face in brep.Faces)
                            {
                                var faceMesh = face.GetMesh(MeshType.Render);
                                if (faceMesh != null)
                                {
                                    meshes.Add(faceMesh);
                                }
                            }
                            
                            if (meshes.Count > 0)
                            {
                                displayMesh = new Mesh();
                                foreach(var m in meshes)
                                {
                                    displayMesh.Append(m);
                                }
                            }
                        }

                        if (displayMesh != null && displayMesh.Vertices.Count > 0)
                        {
                            var wpfMesh = ConvertRhinoMeshToWPF(displayMesh);
                            wpfMesh.Freeze();
                            var geomModel = new GeometryModel3D(wpfMesh, material);
                            geomModel.Freeze();
                            modelGroup.Children.Add(geomModel);
                        }
                    }
                }
            }
            catch (Exception)
            {
                // Return an empty group or whatever was extracted so far on error
            }

            modelGroup.Freeze();
            return modelGroup;
        }

        private static MeshGeometry3D ConvertRhinoMeshToWPF(Mesh rhinoMesh)
        {
            var wpfMesh = new MeshGeometry3D();

            foreach (var vertex in rhinoMesh.Vertices)
            {
                wpfMesh.Positions.Add(new Point3D(vertex.X, vertex.Y, vertex.Z));
            }

            foreach (var normal in rhinoMesh.Normals)
            {
                wpfMesh.Normals.Add(new Vector3D(normal.X, normal.Y, normal.Z));
            }

            foreach (var face in rhinoMesh.Faces)
            {
                if (face.IsTriangle)
                {
                    wpfMesh.TriangleIndices.Add(face.A);
                    wpfMesh.TriangleIndices.Add(face.B);
                    wpfMesh.TriangleIndices.Add(face.C);
                }
                else if (face.IsQuad)
                {
                    // Split quad into two triangles
                    wpfMesh.TriangleIndices.Add(face.A);
                    wpfMesh.TriangleIndices.Add(face.B);
                    wpfMesh.TriangleIndices.Add(face.C);

                    wpfMesh.TriangleIndices.Add(face.C);
                    wpfMesh.TriangleIndices.Add(face.D);
                    wpfMesh.TriangleIndices.Add(face.A);
                }
            }

            return wpfMesh;
        }
    }
}
