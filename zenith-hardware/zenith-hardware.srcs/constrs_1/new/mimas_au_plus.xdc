## Mimas AU-Plus 7-segment display constraints
## Source: Numato Mimas AU-Plus seven-segment guide

set_property -dict { PACKAGE_PIN L6   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[7] }]; # 7SEG_7 / DP
set_property -dict { PACKAGE_PIN L5   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[6] }]; # 7SEG_6 / A
set_property -dict { PACKAGE_PIN K5   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[5] }]; # 7SEG_5 / B
set_property -dict { PACKAGE_PIN K4   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[4] }]; # 7SEG_4 / C
set_property -dict { PACKAGE_PIN K6   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[3] }]; # 7SEG_3 / D
set_property -dict { PACKAGE_PIN J6   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[2] }]; # 7SEG_2 / E
set_property -dict { PACKAGE_PIN H6   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[1] }]; # 7SEG_1 / F
set_property -dict { PACKAGE_PIN G5   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { LED_out[0] }]; # 7SEG_0 / G

set_property -dict { PACKAGE_PIN H5   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { DIG[0] }]; # 7_SEG1_EN
set_property -dict { PACKAGE_PIN H4   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { DIG[1] }]; # 7_SEG2_EN
set_property -dict { PACKAGE_PIN J4   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { DIG[2] }]; # 7_SEG3_EN
set_property -dict { PACKAGE_PIN J3   IOSTANDARD LVCMOS33   SLEW FAST} [get_ports { DIG[3] }]; # 7_SEG4_EN