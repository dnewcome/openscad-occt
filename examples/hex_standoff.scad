// M3 hex standoff. Showcases: circle($fn=6) as an EXACT hexagon (6 planar faces,
// not a faceted curve) + a cylindrical bore -> a clean, exact STEP spacer.
difference() {
  linear_extrude(20) circle(r = 5, $fn = 6);       // hex body, 10mm across corners
  translate([0, 0, -1]) cylinder(h = 22, r = 1.6); // M3 clearance bore
}
