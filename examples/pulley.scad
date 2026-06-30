// V-groove pulley, defined entirely by one revolved profile.
// Showcases: rotate_extrude -> exact CONICAL + CYLINDRICAL surfaces of revolution
// (the V-groove walls are true cones, not facets) and the bore comes free from
// the profile's inner radius. A lathe/CAM tool reads this STEP as real geometry.
rotate_extrude()
  polygon([[3, 0], [20, 0], [20, 4], [14, 9], [20, 14], [20, 18], [3, 18]]);
