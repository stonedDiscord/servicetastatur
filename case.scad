include <jl_scad/box.scad>
include <jl_scad/parts.scad>
include <jl_scad/utils.scad>

$slop = 0.04;
$fn = $preview?16:48;
$attachable_dump_tree = true;


sz = [183+4,57+4,10];

box_make(print=true,halves=BOTTOM,explode=5,hide_box=false)
{
    // rim snap
    box_shell_base_lid(sz,wall_sides=2,wall_top=1.2,rbot_inside=1,rtop_inside=1,rsides=15,rim_height=3,k=0.2,rim_snap=true) {
        // display
        box_part([TOP]) fwd(24) box_cutout(rect([150,15]),anchor=FRONT);
        // halter
        X(87.5) Y(23) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        X(-87.5) Y(23) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        X(87.5) Y(-26.5) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        X(-87.5) Y(-26.5) box_part([BOT]) standoff(h=3,od=3,id=0.2);
        // kabel
        X(70) box_part([BACK]) box_hole(5.5,chamfer=1);
    }
}