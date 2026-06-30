// Toolchain smoke test: prove OCCT can build a CSG result, tessellate it,
// and export both STL and STEP. Mirrors the Milestone-1 pipeline in miniature.
//   cube(10) centered  -  cylinder(r=3, h=20)
#include <iostream>

#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <TopoDS_Shape.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <StlAPI_Writer.hxx>
#include <STEPControl_Writer.hxx>

int main()
{
  // A 10x10x10 box centered at the origin.
  const double s = 10.0;
  TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(-s / 2, -s / 2, -s / 2), s, s, s).Shape();

  // A cylinder r=3, h=20 along +Z, pushed down so it pierces the box fully.
  gp_Ax2 axis(gp_Pnt(0, 0, -10), gp_Dir(0, 0, 1));
  TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(axis, 3.0, 20.0).Shape();

  // Boolean difference.
  TopoDS_Shape result = BRepAlgoAPI_Cut(box, cyl).Shape();

  // Tessellate (linear deflection 0.1, angular 0.5 rad) in place.
  BRepMesh_IncrementalMesh mesher(result, 0.1, Standard_False, 0.5, Standard_True);
  mesher.Perform();
  std::cout << "meshed: done=" << (mesher.IsDone() ? "yes" : "no") << "\n";

  // STL (binary).
  StlAPI_Writer stl;
  stl.ASCIIMode() = Standard_False;
  if (!stl.Write(result, "smoke.stl")) {
    std::cerr << "STL write failed\n";
    return 1;
  }
  std::cout << "wrote smoke.stl\n";

  // STEP.
  STEPControl_Writer step;
  if (step.Transfer(result, STEPControl_AsIs) != IFSelect_RetDone) {
    std::cerr << "STEP transfer failed\n";
    return 1;
  }
  if (step.Write("smoke.step") != IFSelect_RetDone) {
    std::cerr << "STEP write failed\n";
    return 1;
  }
  std::cout << "wrote smoke.step\n";
  return 0;
}
