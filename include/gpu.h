#pragma once

class GPU
{
public:
    GPU();
    void send_gp0_command(const u32 opcode);
    void send_gp1_command(const u32 opcode);

    // Bus registers - read for receiving responses and stat as a status register
    u32 get_gpu_read();
    u32 get_gpu_stat();

private:
    // GP0 0xE0 settings
    u8 texture_page_x_base;
    u8 texture_page_y_base_1;
    u8 texture_page_y_base_2;
    u8 semi_transparency;
    u8 texture_page_colours;
    bool dither_24_bit_to_15_bit;
    bool draw_to_display_area;
    bool textured_rectangle_x_flip;
    bool textured_rectangle_y_flip;

    bool interlace;

    // GP1 0x00 settings
    bool display_off;
    bool irq_request;
    u8 dma_direction;
    u32 display_address;
    u16 display_x1;
    u16 display_x2;
    u16 display_y1;
    u16 display_y2;
    u8 display_mode;

    // GP1 0x05 settings
    u16 display_area_x;
    u16 display_area_y;

    // GP1 0x08 settings
    u8 horizontal_resolution_1;
    bool vertical_resolution;
    bool video_mode;
    bool display_area_colour_depth;
    bool vertical_interlace;
    bool horizontal_resolution_2;
    bool flip_screen_horizontally;
};