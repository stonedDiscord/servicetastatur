$fn = $preview?16:48;

difference()
{
    union() {cube([6.7, 6.7, 10],center=true);
    translate([0,0,6]) cube([5.4,5.4,2],center=true);}
    translate([0,0,-4.5]) cylinder(h=1,r=2,center=true);
} 