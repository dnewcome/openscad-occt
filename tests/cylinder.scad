// cone (r1!=r2) minus a thinner bore -> tapered tube
difference() {
  cylinder(h=20, r1=10, r2=4);
  translate([0,0,-1]) cylinder(h=22, r=2);
}
