#pragma once
#include "renderer.h"

class GPU
{
friend class Renderer;
public:
    GPU();

    // Bus registers - read for receiving responses and stat as a status register
    u32 get_gpu_read();
    u32 get_gpu_stat();

    // Sent via I/O from CPU
    void send_gp0_command(const u32 opcode);
    void send_gp1_command(const u32 opcode);

    // DMA
    void dma_send(const u32 data);

    Renderer renderer;

private:
    enum class GP0Mode
    {
        Normal,
        VRAMToCPU,
        CPUToVRAM,
        RenderPolygon,
        QuickRectangleFill,
        Rectangle
    };
    struct
    {
        u32 remaining_words;
        u8 header_count;

        void set_mode(GP0Mode new_mode)
        {
            mode = new_mode;
            header_count = 0;
        }

        GP0Mode get_mode() const { return mode; }
        void reset() { mode = GP0Mode::Normal; }

        private:
            GP0Mode mode = GP0Mode::Normal;
    } gp0_input_state;

    struct CPUToVRAM
    {
        u16 destination_x;
        u16 destination_y;
        u32 required_blits;
        u32 blit_count;
        u16 width;
    } gp0_cpu_to_vram;

    struct RenderPolygon
    {
        bool gouraud_shading;
        bool is_quad;
        bool is_textured;
        bool is_transparent;
        bool use_raw_texture;
        u32 colours[4];
        u32 vertices[4];
        u32 uvs[4];
        u8 current_vertex;
        u8 opcodes_sent;
    } gp0_render_polygon;

    struct QuickRectangleFill
    {
        u32 colour;
        u16 x;
        u16 y;
        u16 width;
        u16 height;
    } gp0_quick_rectangle_fill;

    struct Rectangle
    {
        enum class Size
        {
            Variable = 0b00,
            SinglePixel = 0b01,
            Sprite8x8 = 0b10,
            Sprite16x16 = 0b11
        };
        Size size;
        bool is_textured;
        bool is_transparent;
        bool use_raw_texture;
        u32 colour;
        u32 vertex;
        u32 uv;
        u16 width;
        u16 height;
    } gp0_rectangle;

    // GP0 0xE1 settings ("texpage")
    u8 texture_page_x_base;
    u8 texture_page_y_base_1;
    u8 texture_page_y_base_2;
    u8 semi_transparency_mode;
    Renderer::ColourDepth colour_depth;
    bool dither_24_bit_to_15_bit;
    bool draw_to_display_area;
    bool textured_rectangle_x_flip;
    bool textured_rectangle_y_flip;

    // GP0 0xE2 settings - NOTE: in 8 pixel steps
    u8 texture_window_mask_x;
    u8 texture_window_mask_y;
    u8 texture_window_offset_x;
    u8 texture_window_offset_y;

    // GP0 0xE3 and 0xE4
    u32 drawing_area_x1;
    u32 drawing_area_y1;
    u32 drawing_area_x2;
    u32 drawing_area_y2;

    // GP0 0xE5 settings
    i16 drawing_offset_x;
    i16 drawing_offset_y;

    // GP0 0xE6 settings
    bool set_mask_while_drawing;
    bool check_mask_before_draw;

    bool interlace;

    // GP1 0x00 settings
    bool display_off;
    bool irq_request;
    u8 dma_direction;
    u32 display_address;
    u16 display_range_x1;
    u16 display_range_x2;
    u16 display_range_y1;
    u16 display_range_y2;
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

    void push_triangle();
    void push_quad();
};