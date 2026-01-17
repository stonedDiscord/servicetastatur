include <jl_scad/box.scad>
include <jl_scad/parts.scad>
include <jl_scad/utils.scad>

$slop = 0.04;
$fn = $preview?16:48;
$attachable_dump_tree = true;

module button_guide(
    size = 7,
    wall = 1.2,
    h = 10,
    clearance = 0.25
){
    difference() {
        // OUTER sleeve (extends DOWN into box)
        translate([0,-1.5,0])
        cuboid(
            [size + wall*2, size + wall*2, h],
            anchor=FRONT+BOTTOM
        );

        // INNER hole
        translate([0,-0.5,-0.01])
            cuboid(
                [size + clearance, size + clearance, h + 0.02],
                anchor=FRONT+BOTTOM
            );
    }
}

sz = [183+4,57+4,24];

box_make(print=true,halves=TOP,explode=5,hide_box=false)
{
    // rim snap
    box_shell_base_lid(sz,wall_sides=2,wall_top=1.2,rbot_inside=1,rtop_inside=1,rsides=15,rim_height=3,k=0.2,rim_snap=true) {
        // display
        box_part([TOP]) fwd(18) box_cutout(rect([150,15]),anchor=FRONT);

        // taster
        box_part([TOP]) X(-85) Y(14) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //Left
        box_part([TOP]) X(-75) Y(7)  { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //Up
        box_part([TOP]) X(-75) Y(21) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //Down
        box_part([TOP]) X(-65) Y(14) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //Right
        
        
        box_part([TOP]) X(-34) Y(20) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //F1
        box_part([TOP]) X(-8)  Y(20) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //F2
        box_part([TOP]) X(17)  Y(20) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //F3
        box_part([TOP]) X(45)  Y(20) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //F4

        box_part([TOP]) X(77)  Y(17) { box_cutout(rect([6,6]),anchor=FRONT); button_guide(); } //OK
 
        // halter
        // lcd oben
        X(87.5)  Y(23.5) box_standoff_clamp(h=6,od=3,id=1.5,gap=2,pin_h=2);
        X(-87.5) Y(23.5) box_standoff_clamp(h=6,od=3,id=1.5,gap=2,pin_h=2);
       
        // unterschied 26.5
       
        // lcd unten
        X(87.5)  Y(-3)   box_standoff_clamp(h=16,od=3,id=1.5,gap=2,pin_h=2);
        X(-87.5) Y(-3)   box_standoff_clamp(h=16,od=3,id=1.5,gap=2,pin_h=2);
      
        // unterschied 23
      
        // board unten
        X(87.5)  Y(-26)  box_standoff_clamp(h=6,od=3,id=1.5,gap=2,pin_h=2);
        X(-87.5) Y(-26)  box_standoff_clamp(h=6,od=3,id=1.5,gap=2,pin_h=2);
        // kabel
        X(70)    Z(-6)   box_part([BACK]) box_hole(5.5,chamfer=1);
        
        // button wall
        X(50) Y(18) box_part([TOP],FRONT) box_wall(RIGHT,height=15,length=70,width=1,fillet=1.5,gap=0.5);
        
        // screen wall
        Y(25) box_part([TOP],FRONT) box_wall(RIGHT,height=8,length=140,width=1,fillet=1.5,gap=0.5);
        
        // screen
        box_part([TOP])
        box_preview() X(15) Y(-13) position(LEFT) cuboid([162,29,9],anchor=LEFT,edges="X");
    }
}
