module lcd_timing (
    input wire clk_33m,          // 33.3 MHz Pixel Clock from rPLL
    input wire rst_n,            // Active low reset
    output reg hsync,            // Horizontal Sync Pulse
    output reg vsync,            // Vertical Sync Pulse
    output reg video_de,         // Data Enable (High when in visible area)
    output reg [9:0] pixel_x,    // Current X coordinate (0-799)
    output reg [9:0] pixel_y,    // Current Y coordinate (0-479)
    output reg frame_done        // Triggers high for 1 cycle at start of V-Blank
);

    // Horizontal Timing Constants
    localparam H_ACTIVE = 800;
    localparam H_FP     = 40;
    localparam H_SYNC   = 48;
    localparam H_BP     = 88;
    localparam H_TOTAL  = H_ACTIVE + H_FP + H_SYNC + H_BP; // 976

    // Vertical Timing Constants
    localparam V_ACTIVE = 480;
    localparam V_FP     = 13;
    localparam V_SYNC   = 3;
    localparam V_BP     = 32;
    localparam V_TOTAL  = V_ACTIVE + V_FP + V_SYNC + V_BP; // 528

    // Internal Counters
    reg [9:0] h_cnt;
    reg [9:0] v_cnt;

    // 1. Horizontal and Vertical Counters
    always @(posedge clk_33m or negedge rst_n) begin
        if (!rst_n) begin
            h_cnt <= 0;
            v_cnt <= 0;
        end else begin
            if (h_cnt == H_TOTAL - 1) begin
                h_cnt <= 0;
                if (v_cnt == V_TOTAL - 1)
                    v_cnt <= 0;
                else
                    v_cnt <= v_cnt + 1'd1;
            end else begin
                h_cnt <= h_cnt + 1'd1;
            end
        end
    end

    // 2. Sync Pulse Generation (Most 800x480 screens prefer negative/low-active syncs)
    always @(posedge clk_33m or negedge rst_n) begin
        if (!rst_n) begin
            hsync <= 1'b1;
            vsync <= 1'b1;
        end else begin
            hsync <= (h_cnt >= (H_ACTIVE + H_FP) && h_cnt < (H_ACTIVE + H_FP + H_SYNC)) ? 1'b0 : 1'b1;
            vsync <= (v_cnt >= (V_ACTIVE + V_FP) && v_cnt < (V_ACTIVE + V_FP + V_SYNC)) ? 1'b0 : 1'b1;
        end
    end

    // 3. Data Enable & Pixel Coordinate Management
    always @(posedge clk_33m or negedge rst_n) begin
        if (!rst_n) begin
            video_de <= 1'b0;
            pixel_x  <= 0;
            pixel_y  <= 0;
            frame_done <= 1'b0;
        end else begin
            // Data is valid only in the active display window
            if ((h_cnt < H_ACTIVE) && (v_cnt < V_ACTIVE)) begin
                video_de <= 1'b1;
                pixel_x  <= h_cnt;
                pixel_y  <= v_cnt;
            end else begin
                video_de <= 1'b0;
                pixel_x  <= 0;
                pixel_y  <= 0;
            end

            // Assert frame_done exactly when entering the Vertical Front Porch (V-Blank start)
            if ((v_cnt == V_ACTIVE) && (h_cnt == 0))
                frame_done <= 1'b1;
            else
                frame_done <= 1'b0;
        end
    end

endmodule