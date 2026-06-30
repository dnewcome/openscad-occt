// Declarative query selection: fillet only the convex (outer) edges of a step,
// leaving the reentrant concave edge sharp. Swap "convex" -> "concave" to fillet
// only the inner edge, or "all" for both.
fillet(2, edges="convex") union() {
  cube([30, 20, 8]);
  cube([12, 20, 24]);
}
