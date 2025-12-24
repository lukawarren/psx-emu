#include "gpu.h"

GPU::GPU()
{
}

u32 GPU::get_gpu_read()
{
    dbg("TODO: gpu_read");
    return 0;
}

u32 GPU::get_gpu_stat()
{
    u32 value = 0;

    // GP0 0xE1 settings
    value |= u32(texture_page_x_base & 0b111) << 0;
    value |= u32(texture_page_y_base_1 & 0b1) << 4;
    value |= u32(semi_transparency & 0b11) << 5;
    value |= u32(texture_page_colours & 0b11) << 7;
    value |= u32(dither_24_bit_to_15_bit & 0b1) << 9;
    value |= u32(texture_page_y_base_2 & 0b1) << 15;

    // GP0 0xE6 settings
    value |= u32(set_mask_while_drawing & 0b1) << 11;
    value |= u32(check_mask_before_draw & 0b1) << 12;

    // Misc.
    value |= u32(interlace & 0b1) << 13;

    // GP1
    value |= u32(horizontal_resolution_1 & 0b11) << 17;
    value |= u32(vertical_resolution & 0b1) << 19;
    value |= u32(video_mode & 0b1) << 20;
    value |= u32(display_area_colour_depth & 0b1) << 21;
    value |= u32(vertical_interlace & 0b1) << 22;
    value |= u32(display_off & 0b1) << 23;
    value |= u32(irq_request & 0b1) << 24;
    value |= u32(horizontal_resolution_2 & 0b1) << 14;
    value |= u32(flip_screen_horizontally & 0b1) << 16;

    // Ready to receive command word
    value |= 1 << 26;

    // Ready to send VRAM to CPU
    value |= 0 << 27;

    // Ready to receive DMA block
    value |= 1 << 28;

    return value;
}

void GPU::send_gp0_command(const u32 opcode)
{
    const u32 command = ((opcode >> 24) & 0xFF);

    // Draw mode settings
    if (command == 0xE1)
    {
        texture_page_x_base = ((opcode >> 0) & 0b111);
        texture_page_y_base_1 = ((opcode >> 4) & 0b1);
        semi_transparency = ((opcode >> 5) & 0b11);
        texture_page_colours = ((opcode >> 7) & 0b11);
        dither_24_bit_to_15_bit = ((opcode >> 9) & 0b1);
        draw_to_display_area = ((opcode >> 10) & 0b1);
        texture_page_y_base_2 = ((opcode >> 11) & 0b1);
        textured_rectangle_x_flip = ((opcode >> 12) & 0b1);
        textured_rectangle_y_flip = ((opcode >> 13) & 0b1);
        return;
    }

    // Texture window settings
    if (command == 0xE2)
    {
        texture_window_mask_x = command & 0b11111;
        texture_window_mask_y = (command >> 5) & 0b11111;
        texture_window_offset_x = (command >> 10) & 0b11111;
        texture_window_offset_y = (command >> 15) & 0b11111;
        return;
    }

    // Drawing area settings
    if (command == 0xE3)
    {
        drawing_area_x1 = opcode & 0b111111111;
        drawing_area_y1 = (opcode >> 10) & 0b111111111;
        return;
    }
    if (command == 0xE4)
    {
        drawing_area_x2 = opcode & 0b111111111;
        drawing_area_y2 = (opcode >> 10) & 0b111111111;
        return;
    }

    // Drawing offset
    if (command == 0xE5)
    {
        drawing_offset_x = opcode & 0b11111111111;
        drawing_offset_y = (opcode >> 11) & 0b11111111111;
        return;
    }

    // Mask bit setting
    if (command == 0xE6)
    {
        set_mask_while_drawing = (command & 0b1) != 0;
        check_mask_before_draw = ((command >> 1) & 0b1) != 0;
        return;
    }

    // NOP?
    if (opcode == 0x0007920c)
        return;

    dbg("TODO: unknown GP0 command", dbg::hex(opcode), dbg::bin((u8)(opcode >> 29)));
    exit(1);
}

void GPU::send_gp1_command(const u32 opcode)
{
    const u8 command = (u8)(opcode >> 24);

    // Reset GPU
    if (command == 0)
    {
        // Reset GP0 settings
        texture_page_x_base = 0;
        texture_page_y_base_1 = 0;
        texture_page_y_base_2 = 0;
        semi_transparency = 0;
        texture_page_colours = 0;
        dither_24_bit_to_15_bit = 0;
        draw_to_display_area = 1;
        textured_rectangle_x_flip = 0;
        textured_rectangle_y_flip = 0;
        texture_window_mask_x = 0;
        texture_window_mask_y = 0;
        texture_window_offset_x = 0;
        texture_window_offset_y = 0;
        drawing_area_x1 = 0;
        drawing_area_y1 = 0;
        drawing_area_x2 = 0;
        drawing_area_y2 = 0;
        drawing_offset_x = 0;
        drawing_offset_y = 0;
        set_mask_while_drawing = 0;
        check_mask_before_draw = 0;

        interlace = 1;

        // Reset GP1
        dbg("TODO: GPU reset; clear FIFO (0x01) and check display_x*, display_y* values");
        irq_request = 0;
        display_off = 1;
        dma_direction = 0;

        // GP1 0x05
        display_area_x = 0;
        display_area_y = 0;

        // GP1 0x06
        display_range_x1 = 0x200;
        display_range_x2 = 256 * 10;

        // GP1 0x07
        display_range_y1 = 0x10;
        display_range_y2 = 240;

        // GP1 0x08
        horizontal_resolution_1 = 0;
        vertical_resolution = 0;
        video_mode = 0;
        display_area_colour_depth = 0;
        vertical_interlace = 0;
        horizontal_resolution_2 = 0;
        flip_screen_horizontally = 0;

        assert(get_gpu_stat() == 0x14802000);
        return;
    }

    // Reset command buffer
    if (command == 0x01)
    {
        dbg("TODO: reset GP1 command buffer");
        return;
    }

    // DMA direction
    if (command == 0x04)
    {
        dma_direction = opcode & 0b11;
        return;
    }

    // Start of display area (in VRAM)
    if (command == 0x05)
    {
        display_area_x = opcode & 0b1111111111;
        display_area_y = (opcode >> 10) & 0b111111111;
        return;
    }

    // Horizontal display range (on screen)
    if (command == 0x06)
    {
        display_range_x1 = opcode & 0b111111111111;
        display_range_x2 = (opcode >> 10) & 0b111111111111;
        return;
    }

    // Vertical display range (on screen)
    if (command == 0x07)
    {
        display_range_y1 = opcode & 0b1111111111;
        display_range_y2 = (opcode >> 10) & 0b1111111111;
        return;
    }

    // Display mode
    if (command == 0x08)
    {
        horizontal_resolution_1 = (opcode >> 1) & 0b11;
        vertical_resolution = (opcode >> 2) & 0b1;
        video_mode = (opcode >> 3) & 0b1;
        display_area_colour_depth = (opcode >> 4) & 0b1;
        vertical_interlace = (opcode >> 5) & 0b1;
        horizontal_resolution_2 = (opcode >> 6) & 0b1;
        flip_screen_horizontally = (opcode >> 7) & 0b1;
        return;
    }

    dbg("TODO: GP1 command", dbg::hex(command));
    exit(1);
}

void GPU::dma_send(const u32 data)
{
    send_gp0_command(data);
    printf("%u\n", data);
}
