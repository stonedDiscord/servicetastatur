include <jl_scad/box.scad>
include <jl_scad/parts.scad>
include <jl_scad/utils.scad>

$slop = 0.04;
$fn = $preview?16:48;
$attachable_dump_tree = true;


sz = [183+4,57+4,12];

box_make(print=true,halves=BOT,explode=5,hide_box=false)
{
    // rim snap
    box_shell_base_lid(sz,wall_sides=2,wall_top=1.2,rbot_inside=1,rtop_inside=1,rsides=15,rim_height=3,k=0.2,rim_snap=true) {
        // display
        box_part([TOP]) fwd(20) box_cutout(rect([150,15]),anchor=FRONT);

        // taster
        box_part([TOP]) X(-85) Y(14) box_cutout(rect([6,6]),anchor=FRONT); //Left
        box_part([TOP]) X(-75) Y(7) box_cutout(rect([6,6]),anchor=FRONT); //Up
        box_part([TOP]) X(-75) Y(21) box_cutout(rect([6,6]),anchor=FRONT); //Down
        box_part([TOP]) X(-65) Y(14) box_cutout(rect([6,6]),anchor=FRONT); //Right
        
        
        box_part([TOP]) X(-34) Y(20) box_cutout(rect([6,6]),anchor=FRONT); //F1
        box_part([TOP]) X(-8) Y(20) box_cutout(rect([6,6]),anchor=FRONT); //F2
        box_part([TOP]) X(17) Y(20) box_cutout(rect([6,6]),anchor=FRONT);  //F3
        box_part([TOP]) X(45) Y(20) box_cutout(rect([6,6]),anchor=FRONT);  //F4

        box_part([TOP]) X(77) Y(17) box_cutout(rect([6,6]),anchor=FRONT);  //OK
 
        // halter
        X(87.5) Y(23) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        X(-87.5) Y(23) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        X(87.5) Y(-26.5) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        X(-87.5) Y(-26.5) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        // kabel
        X(70) box_part([BACK]) box_hole(5.5,chamfer=1);
    }
}
