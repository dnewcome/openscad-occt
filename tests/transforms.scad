// translate / rotate / scale + union
union() {
  cube([10, 10, 2], center=true);
  translate([0, 0, 5]) scale([1, 1, 2]) sphere(3);
  rotate([0, 90, 0]) translate([0, 0, 8]) cylinder(h=6, r=1.5, center=true);
}
