// step_check -- read a STEP file and report exact B-Rep validity.
//
// The STL watertightness check tests the *tessellation*, which can show T-junctions
// at fillet/blend boundaries (OCCT meshes each face independently) even when the
// underlying solid is exact and valid. This checks the real thing: BRepCheck_Analyzer
// on the B-Rep read back from STEP, plus the solid/shell count. A step toward the
// "stronger verification invariants" on the roadmap (validity, not just bbox).

#include <STEPControl_Reader.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <Standard_Failure.hxx>

#include <cstdio>

int main(int argc, char** argv) {
  if (argc < 2) { printf("usage: step_check file.step\n"); return 2; }
  try {
    STEPControl_Reader reader;
    if (reader.ReadFile(argv[1]) != IFSelect_RetDone) {
      printf("%s  READ FAILED\n", argv[1]);
      return 2;
    }
    reader.TransferRoots();
    TopoDS_Shape s = reader.OneShape();

    int solids = 0, shells = 0;
    for (TopExp_Explorer e(s, TopAbs_SOLID); e.More(); e.Next()) ++solids;
    for (TopExp_Explorer e(s, TopAbs_SHELL); e.More(); e.Next()) ++shells;

    bool ok = BRepCheck_Analyzer(s).IsValid();
    printf("%-28s solids=%d shells=%d  brep_valid=%s\n",
           argv[1], solids, shells, ok ? "YES" : "NO");
    return ok ? 0 : 1;
  } catch (const Standard_Failure& e) {
    printf("%s  OCCT error: %s\n", argv[1], e.GetMessageString());
    return 2;
  }
}
