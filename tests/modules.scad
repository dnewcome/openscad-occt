// M3: user module + function + for + if + children()
function spacing() = size + gap;
size = 10;
gap  = 4;

module tile(n) {
  difference() {
    cube([size, size, 3]);
    translate([size/2, size/2, -1]) cylinder(h = 5, r = 2 + n*0.5);
  }
}

for (i = [0:3])
  if (i != 1)
    translate([i * spacing(), 0, 0]) tile(i);
