#include "gpu.h"

#define HACK

GPU::GPU()
{
    dbg("TODO: undo bit 31 hack (and add back assertion)");
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
    value |= u32(texture_page_x_base & 0b1111) << 0;
    value |= u32(texture_page_y_base_1 & 0b1) << 4;
    value |= u32(semi_transparency_mode & 0b11) << 5;
    value |= u32((u32)texture_page_colours & 0b11) << 7;
    value |= u32(dither_24_bit_to_15_bit & 0b1) << 9;
    value |= u32(texture_page_y_base_2 & 0b1) << 15;

    // GP0 0xE6 settings
    value |= u32(set_mask_while_drawing & 0b1) << 11;
    value |= u32(check_mask_before_draw & 0b1) << 12;

    // Misc.
    value |= u32(interlace & 0b1) << 13;

    // GP1
    value |= u32(horizontal_resolution_1 & 0b11) << 17;
#ifndef HACK
    value |= u32(vertical_resolution & 0b1) << 19;
#endif
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
    value |= 1 << 27;

    // Ready to receive DMA block
    value |= 1 << 28;

    return value;
}

void GPU::send_gp0_command(const u32 opcode)
{
    // Continuation of 0xA0 - transfer from CPU to VRAM
    if (gp0_input_state.get_mode() == GP0Mode::CPUToVRAM)
    {
        if (gp0_input_state.header_count == 0)
        {
            gp0_cpu_to_vram.destination_x = (u16)opcode;
            gp0_cpu_to_vram.destination_y = (u16)(opcode >> 16);
        }
        else if (gp0_input_state.header_count == 1)
        {
            const u16 width = (u16)opcode;
            const u16 height = (u16)(opcode >> 16);
            gp0_cpu_to_vram.required_blits = (u32)width * (u32)height;
            gp0_cpu_to_vram.blit_count = 0;
            gp0_cpu_to_vram.width = width;

            // Round up image size as must send full 32 bits; not just last 16-bit pixel
            // Divide by 2 as two pixels per 32-bit packet
            gp0_input_state.remaining_words = ((gp0_cpu_to_vram.required_blits + 1U) & ~1U) / 2U;
        }
        else
        {
            for (int i = 0; i < 2; i++)
            {
                // Copy pixel
                const u16 x = (u16)(gp0_cpu_to_vram.blit_count % (u32)gp0_cpu_to_vram.width);
                const u16 y = (u16)(gp0_cpu_to_vram.blit_count / (u32)gp0_cpu_to_vram.width);
                renderer.blit(
                    *this,
                    gp0_cpu_to_vram.destination_x + x,
                    gp0_cpu_to_vram.destination_y + y,
                    (u16)(i == 0 ? (opcode & 0xFFFF) : (opcode >> 16))
                );
                gp0_cpu_to_vram.blit_count++;

                // Copy second pixel if there is one
                if (gp0_cpu_to_vram.blit_count >= gp0_cpu_to_vram.required_blits)
                    break;
            }

            if (--gp0_input_state.remaining_words == 0)
                gp0_input_state.reset();
            return;
        }

        gp0_input_state.header_count++;
        return;
    }

    // Continuation of 0xC0 - transfer from CPU to VRAM
    if (gp0_input_state.get_mode() == GP0Mode::CPUToVRAM)
    {
        if (gp0_input_state.header_count == 1)
            gp0_input_state.reset();

        gp0_input_state.header_count++;
        return;
    }

    // Continuation of polygon rendering
    if (gp0_input_state.get_mode() == GP0Mode::RenderPolygon)
    {
        // Pretend first colour was already sent to simplify logic
        if (gp0_render_polygon.opcodes_sent == 0 && gp0_render_polygon.gouraud_shading)
            gp0_render_polygon.opcodes_sent = 1;

        if (gp0_render_polygon.gouraud_shading && gp0_render_polygon.is_textured)
        {
            if ((gp0_render_polygon.opcodes_sent % 3) == 0)
                gp0_render_polygon.colours[gp0_render_polygon.current_vertex] = opcode;
            else if ((gp0_render_polygon.opcodes_sent % 3) == 1)
                gp0_render_polygon.vertices[gp0_render_polygon.current_vertex] = opcode;
            else
                gp0_render_polygon.uvs[gp0_render_polygon.current_vertex++] = opcode;
        }
        else if (gp0_render_polygon.gouraud_shading)
        {
            if ((gp0_render_polygon.opcodes_sent % 2) == 0)
                gp0_render_polygon.colours[gp0_render_polygon.current_vertex] = opcode;
            else if ((gp0_render_polygon.opcodes_sent % 2) == 1)
                gp0_render_polygon.vertices[gp0_render_polygon.current_vertex++] = opcode;
        }
        else if (gp0_render_polygon.is_textured)
        {
            if ((gp0_render_polygon.opcodes_sent % 2) == 0)
                gp0_render_polygon.vertices[gp0_render_polygon.current_vertex] = opcode;
            else if ((gp0_render_polygon.opcodes_sent % 2) == 1)
                gp0_render_polygon.uvs[gp0_render_polygon.current_vertex++] = opcode;
        }
        else
            gp0_render_polygon.vertices[gp0_render_polygon.current_vertex++] = opcode;

        gp0_render_polygon.opcodes_sent++;

        if (--gp0_input_state.remaining_words == 0)
        {
            // If we are using flat shading, lie about colours (fill in rest)
            if (!gp0_render_polygon.gouraud_shading)
                for (int i = 1; i < 4; i++)
                    gp0_render_polygon.colours[i] = gp0_render_polygon.colours[0];

            // Full polygon received
            if (gp0_render_polygon.is_quad) push_quad();
            else push_triangle();
            gp0_input_state.reset();
        }

        return;
    }

    // Continuation of quick rectangle fill
    if (gp0_input_state.get_mode() == GP0Mode::QuickRectangleFill)
    {
        if (gp0_input_state.header_count == 0)
        {
            gp0_quick_rectangle_fill.x = (u16)opcode;
            gp0_quick_rectangle_fill.y = (u16)(opcode >> 16);
        }
        else if (gp0_input_state.header_count == 1)
        {
            gp0_quick_rectangle_fill.width = (u16)opcode;
            gp0_quick_rectangle_fill.height = (u16)(opcode >> 16);
        }

        if (++gp0_input_state.header_count == 2)
        {
            gp0_input_state.reset();
            renderer.push(Renderer::QuickRectangle {
                gp0_quick_rectangle_fill.colour,
                gp0_quick_rectangle_fill.x,
                gp0_quick_rectangle_fill.y,
                gp0_quick_rectangle_fill.width,
                gp0_quick_rectangle_fill.height
            });
        }
        return;
    }

    // Continuation of rectangle
    if (gp0_input_state.get_mode() == GP0Mode::Rectangle)
    {
        bool done = false;

        if (gp0_input_state.header_count == 0)
        {
            gp0_rectangle.vertex = opcode;

            if (!gp0_rectangle.is_textured && gp0_rectangle.size != Rectangle::Size::Variable)
                done = true;
        }
        else if (gp0_input_state.header_count == 1)
        {
            if (gp0_rectangle.is_textured)
            {
                gp0_rectangle.uv = opcode;
                if (gp0_rectangle.size != Rectangle::Size::Variable)
                    done = true;
            }
            else if (gp0_rectangle.size == Rectangle::Size::Variable)
            {
                gp0_rectangle.width = (u16)opcode;
                gp0_rectangle.height = (u16)(opcode >> 16);
                done = true;
            }
        }
        else if (gp0_input_state.header_count == 2)
        {
            gp0_rectangle.width = (u16)opcode;
            gp0_rectangle.height = (u16)(opcode >> 16);
            done = true;
        }

        gp0_input_state.header_count++;

        if (done)
        {
            gp0_input_state.reset();

            u16 width, height;
            switch (gp0_rectangle.size)
            {
                case Rectangle::Size::Variable:
                    width = gp0_rectangle.width;
                    height = gp0_rectangle.height;
                    break;

                case Rectangle::Size::SinglePixel:
                    width = height = 1;
                    break;

                case Rectangle::Size::Sprite8x8:
                    width = height = 8;
                    break;

                default:
                    width = height = 16;
            }

            renderer.push(*this, Renderer::Rectangle {
                gp0_rectangle.is_textured,
                gp0_rectangle.is_transparent,
                gp0_rectangle.use_raw_texture,
                gp0_rectangle.colour,
                gp0_rectangle.vertex,
                gp0_rectangle.uv,
                width,
                height,
            });
        }
        return;
    }

    const u32 command = ((opcode >> 24) & 0xFF);
    const u32 mode = opcode >> 29;

    // Draw mode settings
    if (command == 0xE1)
    {
        texture_page_x_base = ((opcode >> 0) & 0b1111);
        texture_page_y_base_1 = ((opcode >> 4) & 0b1);
        semi_transparency_mode = ((opcode >> 5) & 0b11);
        texture_page_colours = (TexturePageColours)((opcode >> 7) & 0b11);
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
        drawing_area_x1 = opcode & 0b1111111111;
        drawing_area_y1 = (opcode >> 10) & 0b111111111;
        return;
    }
    if (command == 0xE4)
    {
        drawing_area_x2 = opcode & 0b1111111111;
        drawing_area_y2 = (opcode >> 10) & 0b111111111;
        return;
    }

    // Drawing offset
    if (command == 0xE5)
    {
        const u32 x = opcode & 0b11111111111;
        const u32 y = (opcode >> 11) & 0b11111111111;

        // 11-bit signed two's complement
        drawing_offset_x = (i16)(x << 5) >> 5;
        drawing_offset_y = (i16)(y << 5) >> 5;

        return;
    }

    // Mask bit setting
    if (command == 0xE6)
    {
        set_mask_while_drawing = (command & 0b1) != 0;
        check_mask_before_draw = ((command >> 1) & 0b1) != 0;
        return;
    }

    // NOPs
    if (command == 0x00)
        return;

    // Clears the CLUT cache
    if (command == 0x01)
        return;

    // CPU to VRAM blit
    if (command == 0xA0)
    {
        gp0_input_state.set_mode(GP0Mode::CPUToVRAM);
        return;
    }

    // VRAM to CPU blit
    if (command == 0xC0)
    {
        gp0_input_state.set_mode(GP0Mode::VRAMToCPU);
        dbg("TODO: VRAM to CPU blit");
        return;
    }

    // Polygon render
    if (mode == 0b001)
    {
        gp0_input_state.set_mode(GP0Mode::RenderPolygon);
        gp0_render_polygon = {
            .gouraud_shading = ((opcode >> 28) & 0b1) == 1,
            .is_quad = ((opcode >> 27) & 0b1) == 1,
            .is_textured = ((opcode >> 26) & 0b1) == 1,
            .is_transparent = ((opcode >> 25) & 0b1) == 1,
            .use_raw_texture = ((opcode >> 24) & 0b1) == 1,
            .colours = {},
            .vertices = {},
            .uvs = {},
            .current_vertex = 0,
            .opcodes_sent = 0
        };
        gp0_render_polygon.colours[0] = opcode & 0xFFFFFF;

        // First colour is already included, so subtract 1
        const u32 vertices = gp0_render_polygon.is_quad ? 4 : 3;
        gp0_input_state.remaining_words = vertices +
            (gp0_render_polygon.gouraud_shading ? (vertices - 1) : 0) +
            (gp0_render_polygon.is_textured ? vertices : 0);

        return;
    }

    // Quick rectangle fill
    if (command == 0x02)
    {
        gp0_input_state.set_mode(GP0Mode::QuickRectangleFill);
        gp0_quick_rectangle_fill.colour = opcode & 0xFFFFFF;
        return;
    }

    // Rectangle
    if (mode == 0b011)
    {
        gp0_input_state.set_mode(GP0Mode::Rectangle);
        gp0_rectangle.size = (Rectangle::Size)((opcode >> 27) & 0b11);
        gp0_rectangle.is_textured = ((opcode >> 26) & 0b1) != 0;
        gp0_rectangle.is_transparent = ((opcode >> 25) & 0b1) != 0;
        gp0_rectangle.use_raw_texture = ((opcode >> 24) & 0b1) != 0;
        gp0_rectangle.colour = opcode & 0xFFFFFF;
        return;
    }

    dbg("TODO: unknown GP0 command", dbg::hex(opcode), dbg::hex(command), dbg::bin((u8)mode));
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
        semi_transparency_mode = 0;
        texture_page_colours = TexturePageColours::FourBit;
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

        // Reset command buffer
        gp0_input_state.reset();

#ifndef HACK
        assert(get_gpu_stat() == 0x14802000);
#endif
        return;
    }

    // Reset command buffer
    if (command == 0x01)
    {
        gp0_input_state.reset();
        return;
    }

    // Display enable
    if (command == 0x03)
    {
        display_off = opcode & 1;
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
        display_area_x = opcode & 0b1111111110;
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

    // Acknowledge GPU interrupt
    if (command == 0x02)
    {
        irq_request = 0;
        return;
    }

    // Read GPU internal register
    if (command == 0x10)
    {
        dbg("TODO: GP1 0x10");
        return;
    }

    dbg("TODO: GP1 command", dbg::hex(command));
    exit(1);
}

void GPU::dma_send(const u32 data)
{
    send_gp0_command(data);
}

void GPU::push_quad()
{
    const u16 clut = (u16)(gp0_render_polygon.uvs[0] >> 16);
    const u16 texture_page = (u16)(gp0_render_polygon.uvs[1] >> 16);

    // First triangle
    renderer.push(*this, {
        gp0_render_polygon.is_textured,
        gp0_render_polygon.is_transparent,
        gp0_render_polygon.use_raw_texture,
        gp0_render_polygon.colours,
        gp0_render_polygon.vertices,
        gp0_render_polygon.uvs,
        clut,
        texture_page
    });

    // Second triangle
    renderer.push(*this, {
        gp0_render_polygon.is_textured,
        gp0_render_polygon.is_transparent,
        gp0_render_polygon.use_raw_texture,
        gp0_render_polygon.colours + 1,
        gp0_render_polygon.vertices + 1,
        gp0_render_polygon.uvs + 1,
        clut,
        texture_page
    });
}

void GPU::push_triangle()
{
    const u16 clut = (u16)(gp0_render_polygon.uvs[0] >> 16);
    const u16 texture_page = (u16)(gp0_render_polygon.uvs[1] >> 16);

    renderer.push(*this, {
        gp0_render_polygon.is_textured,
        gp0_render_polygon.is_transparent,
        gp0_render_polygon.use_raw_texture,
        gp0_render_polygon.colours,
        gp0_render_polygon.vertices,
        gp0_render_polygon.uvs,
        clut,
        texture_page
    });
}