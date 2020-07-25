# repair_design max_slew hi fanout register array
source "helpers.tcl"
source "hi_fanout.tcl"

read_liberty Nangate45/Nangate45_typ.lib
read_lef Nangate45/Nangate45.lef
set def_file [file join $result_dir "repair_slew1.def"]
write_hi_fanout_def $def_file 30
read_def $def_file
create_clock -period 1 clk1

set_wire_rc -layer metal3
estimate_parasitics -placement

# set max slew low enough there are no max cap violations
set_max_transition .05 [current_design]
# print order is not stable across ports so just count violations
with_output_to_variable violations {
  report_check_types -max_slew -max_cap -max_fanout -violators
}
puts "Found [regexp -all VIOLATED $violations] violations"

repair_design -buffer_cell BUF_X2

with_output_to_variable violations {
  report_check_types -max_slew -max_cap -max_fanout -violators
}
puts "Found [regexp -all VIOLATED $violations] violations"
