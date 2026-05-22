`timescale 1ns / 1ps

module main (
    output logic [3:0] DIG,
    output logic [7:0] LED_out
);

    // Mimas AU-Plus mapping:
    //
    // DIG is active-low:
    //   0 = digit enabled
    //   1 = digit disabled
    //
    // LED_out pattern for displaying 0:
    //   8'b00000001
    //
    // Numato's docs label:
    //   LED_out[7] = DP
    //   LED_out[6] = A
    //   LED_out[5] = B
    //   LED_out[4] = C
    //   LED_out[3] = D
    //   LED_out[2] = E
    //   LED_out[1] = F
    //   LED_out[0] = G

    always_comb begin
        DIG     = 4'b0000;      // Enable all 4 digits
        LED_out = 8'b00000001;  // Display 0, decimal point off
    end

endmodule