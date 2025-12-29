#include "renderer.h"
#include "gpu.h"

void Renderer::blit(const GPU& gpu, const u32 x, const u32 y, const u16 pixel)
{
    // Wrap around
    u16* output = &vram[(y % vram_height) * vram_width + (x % vram_width)];
    write_with_masking(output, pixel, gpu);
}

void Renderer::push(const GPU& gpu, const Triangle& polygon)
{
    Vertex v[3];
    for (int i = 0; i < 3; i++)
    {
        unpack_vertex(polygon.vertices[i], v[i].x, v[i].y);
        unpack_colour(polygon.colours[i], v[i].r, v[i].g, v[i].b);
        unpack_uv(polygon.uvs[i], v[i].u, v[i].v);
        v[i].x += (i32)gpu.drawing_offset_x;
        v[i].y += (i32)gpu.drawing_offset_y;
    }

    // Parse texpage (other fields are taken from GP0 0xE1)
    const u32 texture_base_page_x = (polygon.texture_page & 0b1111) * 64;
    const u32 texture_base_page_y = ((polygon.texture_page >> 4) & 0b1) * 256;

    // Parse CLUT
    const u32 clut_x = (polygon.clut & 0b111111) * 16;
    const u32 clut_y = (polygon.clut >> 6) & 0b111111111;

    ensure_clockwise_vertex_ordering(v[0], v[1], v[2]);

    // Compute bounding box
    i32 min_x = std::min(v[0].x, std::min(v[1].x, v[2].x));
    i32 min_y = std::min(v[0].y, std::min(v[1].y, v[2].y));
    i32 max_x = std::max(v[0].x, std::max(v[1].x, v[2].x));
    i32 max_y = std::max(v[0].y, std::max(v[1].y, v[2].y));

    // Confine to drawing area and VRAM bounds - TODO: should wrap instead?
    min_x = std::max(min_x, std::max((i32)gpu.drawing_area_x1, 0));
    max_x = std::min(max_x, std::min((i32)gpu.drawing_area_x2, (i32)vram_width - 1));
    min_y = std::max(min_y, std::max((i32)gpu.drawing_area_y1, 0));
    max_y = std::min(max_y, std::min((i32)gpu.drawing_area_y2, (i32)vram_height - 1));

    if (min_x > max_x || min_y > max_y)
        return;

    // Precompute edge functions (incremental form)
    const i32 A01 = v[0].y - v[1].y, B01 = v[1].x - v[0].x;
    const i32 A12 = v[1].y - v[2].y, B12 = v[2].x - v[1].x;
    const i32 A20 = v[2].y - v[0].y, B20 = v[0].x - v[2].x;

    // Compute edge function values at top-left corner
    Vertex p = { min_x, min_y };
    i32 w0_row = get_cross_product_z(v[1], v[2], p);
    i32 w1_row = get_cross_product_z(v[2], v[0], p);
    i32 w2_row = get_cross_product_z(v[0], v[1], p);

    // Precompute for tie-breaking
    const auto is_top_left = [](const Vertex& v0, const Vertex& v1)
    {
        const int dx = v1.x - v0.x;
        const int dy = v1.y - v0.y;
        return (dy < 0) || (dy == 0 && dx > 0);
    };
    const bool is_top_left01 = is_top_left(v[0], v[1]);
    const bool is_top_left12 = is_top_left(v[1], v[2]);
    const bool is_top_left20 = is_top_left(v[2], v[0]);

    // Precompute denominator for barycentric coords
    const i32 denom = get_cross_product_z(v[0], v[1], v[2]);
    const double inv_denom = 1.0 / (double)denom;

    // Color deltas for interpolation
    const double dr0 = v[0].r * inv_denom;
    const double dr1 = v[1].r * inv_denom;
    const double dr2 = v[2].r * inv_denom;
    const double dg0 = v[0].g * inv_denom;
    const double dg1 = v[1].g * inv_denom;
    const double dg2 = v[2].g * inv_denom;
    const double db0 = v[0].b * inv_denom;
    const double db1 = v[1].b * inv_denom;
    const double db2 = v[2].b * inv_denom;

    // UV deltas
    const double du0 = v[0].u * inv_denom;
    const double du1 = v[1].u * inv_denom;
    const double du2 = v[2].u * inv_denom;
    const double dv0 = v[0].v * inv_denom;
    const double dv1 = v[1].v * inv_denom;
    const double dv2 = v[2].v * inv_denom;

    u16* vram_ptr = &vram[min_y * vram_width + min_x];
    const i32 vram_stride = vram_width;

    for (i32 y = min_y; y <= max_y; y++)
    {
        i32 w0 = w0_row;
        i32 w1 = w1_row;
        i32 w2 = w2_row;

        u16* pixel_ptr = vram_ptr;

        for (i32 x = min_x; x <= max_x; x++)
        {
            // Check if inside with proper tie-breaking
            bool inside = (w0 > 0 || (w0 == 0 && is_top_left12)) &&
                         (w1 > 0 || (w1 == 0 && is_top_left20)) &&
                         (w2 > 0 || (w2 == 0 && is_top_left01));

            if (inside)
            {
                if (polygon.is_textured)
                {
                    // Max texture size is 256 so u8 will wrap for us
                    // TODO: texture flip x and y
                    u8 u = (u8)std::round(w0 * du0 + w1 * du1 + w2 * du2);
                    u8 v = (u8)std::round(w0 * dv0 + w1 * dv1 + w2 * dv2);

                    u8 texel_r, texel_g, texel_b;
                    bool texel_mask;
                    bool is_transparent;
                    bool skip = false;

                    // Sample texel
                    if (gpu.texture_page_colours == GPU::TexturePageColours::FourBit)
                    {
                        // 4-bit: 4 pixels per 16-bit word
                        const u32 tex_x = (texture_base_page_x + (u / 4)) % vram_width;
                        const u32 tex_y = (texture_base_page_y + v) % vram_height;
                        const u16 texel_word = vram[tex_y * vram_width + tex_x];

                        // Extract 4-bit index (0-15)
                        const u32 pixel_in_word = u % 4;
                        const u8 clut_index = (u8)(texel_word >> (pixel_in_word * 4)) & 0xF;

                        // Look up color in CLUT
                        const u16 texel = vram[clut_y * vram_width + clut_x + clut_index];
                        texel_r = ((texel >> 0) & 0b11111) * 8;
                        texel_g = ((texel >> 5) & 0b11111) * 8;
                        texel_b = ((texel >> 10) & 0b11111) * 8;
                        texel_mask = (texel >> 15) != 0;
                        is_transparent = (texel >> 15) != 0;

                        // Colour 0 is fully transparrent
                        if (texel == 0)
                            skip = true;
                    }
                    else if (gpu.texture_page_colours == GPU::TexturePageColours::EightBit)
                    {
                        // 8-bit: 2 pixels per 16-bit word
                        const u32 tex_x = (texture_base_page_x + (u / 2)) % vram_width;
                        const u32 tex_y = (texture_base_page_y + v) % vram_height;
                        const u16 texel_word = vram[tex_y * vram_width + tex_x];

                        // Extract 8-bit index (0-255)
                        const u32 pixel_in_word = u % 2;
                        const u8 clut_index = (u8)(texel_word >> (pixel_in_word * 8)) & 0xFF;

                        // Look up color in CLUT
                        const u16 texel = vram[clut_y * vram_width + clut_x + clut_index];
                        texel_r = ((texel >> 0) & 0b11111) * 8;
                        texel_g = ((texel >> 5) & 0b11111) * 8;
                        texel_b = ((texel >> 10) & 0b11111) * 8;
                        texel_mask = (texel >> 15) != 0;
                        is_transparent = (texel >> 15) != 0;

                        // Colour 0 is fully transparrent
                        if (texel == 0)
                            skip = true;
                    }
                    else
                    {
                        // 15-bit
                        const u32 tex_x = (texture_base_page_x + u) % vram_width;
                        const u32 tex_y = (texture_base_page_y + v) % vram_height;
                        const u16 texel = vram[tex_y * vram_width + tex_x];
                        unpack_colour(texel, texel_r, texel_g, texel_b);
                    }

                    if (!skip)
                    {
                        if (polygon.use_raw_texture)
                        {
                            write_with_masking(pixel_ptr, texel_r, texel_g, texel_b, texel_mask, gpu);
                        }
                        else
                        {
                            // Colour blending (modulation)
                            const u8 r = (u8)(((u32)(w0 * dr0 + w1 * dr1 + w2 * dr2) * texel_r) >> 7);
                            const u8 g = (u8)(((u32)(w0 * dg0 + w1 * dg1 + w2 * dg2) * texel_g) >> 7);
                            const u8 b = (u8)(((u32)(w0 * db0 + w1 * db1 + w2 * db2) * texel_b) >> 7);

                            if (gpu.texture_page_colours != GPU::TexturePageColours::FifteenBit && is_transparent)
                                write_with_transparency_and_masking(pixel_ptr, r, g, b, texel_mask, gpu);
                            else
                                write_with_masking(pixel_ptr, r, g, b, texel_mask, gpu);
                        }
                    }
                }
                else
                {
                    const u8 r = (u8)(w0 * dr0 + w1 * dr1 + w2 * dr2);
                    const u8 g = (u8)(w0 * dg0 + w1 * dg1 + w2 * dg2);
                    const u8 b = (u8)(w0 * db0 + w1 * db1 + w2 * db2);
                    if (polygon.is_transparent)
                        write_with_transparency_and_masking(pixel_ptr, r, g, b, false, gpu);
                    else
                        write_with_masking(pixel_ptr, r, g, b, false, gpu);
                }
            }

            // Increment edge functions
            w0 += A12;
            w1 += A20;
            w2 += A01;
            pixel_ptr++;
        }

        // Move to next row
        w0_row += B12;
        w1_row += B20;
        w2_row += B01;
        vram_ptr += vram_stride;
    }
}

void Renderer::push(const QuickRectangle& rectangle)
{
    // Drop 24-bit colour to 16-bit, and set the mask bit to 0
    u8 r, g, b;
    unpack_colour(rectangle.colour, r, g, b);
    const u16 colour = pack_colour(r, g, b, false);

    // Clip to within VRAM
    if (rectangle.x >= vram_width) return;
    if (rectangle.y >= vram_height) return;
    u16 width = rectangle.width;
    u16 height = rectangle.height;
    if (rectangle.x + width >= vram_width) width = vram_width - rectangle.x;
    if (rectangle.y + height >= vram_height) height = vram_height - rectangle.y;

    // Unaffected by mask setting
    for (u16 y = 0; y < height; ++y)
        for (u16 x = 0; x < width; ++x)
            vram[(y + rectangle.y) * vram_width + x + rectangle.x] = colour;
}

void Renderer::push(const GPU& gpu, const Rectangle& rectangle)
{
    const u16 x0 = (u16)(rectangle.vertex & 0xffff);
    const u16 y0 = (u16)(rectangle.vertex >> 16);

    if (rectangle.is_textured)
    {
        // Parse UV
        const u32 colour_lookup_table = rectangle.uv >> 16;
        const u32 u = rectangle.uv & 0xff;
        const u32 v = (rectangle.uv >> 8) & 0xff;

        // Get texture settings from GPU
        const u32 texture_base_page_x = gpu.texture_page_x_base * 64;
        const u32 texture_base_page_y = gpu.texture_page_y_base_1 * 256;
        if (gpu.texture_page_colours != GPU::TexturePageColours::FifteenBit)
        {
            dbg("TODO: non-15-bit colour for rectangle");
            (void)colour_lookup_table;
            exit(1);
        }

        for (u32 y = 0; y < rectangle.height; y++)
        {
            for (u32 x = 0; x < rectangle.width; x++)
            {
                u32 offset_x = x;
                u32 offset_y = y;

                // Texture flipping
                if (gpu.textured_rectangle_x_flip) offset_x = rectangle.width - 1 - x;
                if (gpu.textured_rectangle_y_flip) offset_y = rectangle.height - 1 - y;

                // Max texture window is 256x256
                offset_x %= 256;
                offset_y %= 256;

                const u32 tex_x = (texture_base_page_x + u + offset_x) % vram_width;
                const u32 tex_y = (texture_base_page_y + v + offset_y) % vram_height;

                // Sample texture from VRAM
                const u16 texel = vram[tex_y * vram_width + tex_x];

                // Write to destination
                const u32 dst_x = x0 + x;
                const u32 dst_y = y0 + y;
                if (dst_x < vram_width && dst_y < vram_height)
                {
                    u16* pixel = &vram[dst_y * vram_width + dst_x];

                    if (rectangle.is_transparent && gpu.semi_transparency_mode)
                    {
                        const u8 r = ((u8)(texel >> 0) & 0b11111) << 3;
                        const u8 g = ((u8)(texel >> 5) & 0b11111) << 3;
                        const u8 b = ((u8)(texel >> 10) & 0b11111) << 3;
                        const bool mask = (texel >> 15) != 0;
                        write_with_transparency_and_masking(pixel, r, g, b, mask, gpu);
                    }
                    else
                        write_with_masking(pixel, texel, gpu);
                }
            }
        }
    }
    else
    {
        u8 r, g, b;
        unpack_colour(rectangle.colour, r, g, b);
        const u16 colour = pack_colour(r, g, b, false);

        for (u16 y = 0; y < rectangle.height; y++)
        {
            for (u16 x = 0; x < rectangle.width; x++)
            {
                u16* pixel = &vram[(y0 + y) * vram_width + x0 + x];

                if (rectangle.is_transparent && gpu.semi_transparency_mode)
                    write_with_transparency_and_masking(pixel, r, g, b, false, gpu);
                else
                    write_with_masking(pixel, colour, gpu);
            }
        }
    }
}

i32 Renderer::get_cross_product_z(const Vertex& v0, const Vertex& v1, const Vertex& v2) const
{
    return (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
}

void Renderer::ensure_clockwise_vertex_ordering(Vertex& v0, Vertex& v1, Vertex& v2) const
{
    if (get_cross_product_z(v0, v1, v2) < 0)
        std::swap(v0, v1);
}

bool Renderer::is_inside_triangle(const i32 x, const i32 y, const Vertex& v0, const Vertex& v1, const Vertex& v2) const
{
    // Every possible pair
    const Vertex edges[3][2] =
    {
        { v0, v1 },
        { v1, v2 },
        { v2, v0 }
    };

    for (int i = 0; i < 3; ++i)
    {
        const i32 cross_product = get_cross_product_z(edges[i][0], edges[i][1], Vertex { x, y });

        // Point outside triangle
        if (cross_product < 0) return false;

        // Point lies on an edge
        if (cross_product == 0)
        {
            // Right-oriented edge
            if (edges[i][1].y > edges[i][0].y)
                return false;

            // Horizontal bottom edge
            if (edges[i][1].y == edges[i][0].y && edges[i][1].x < edges[i][0].x)
                return false;

        }
    }

    return true;
}

void Renderer::write_with_masking(u16* original, const u16 pixel, const GPU& gpu) const
{
    if (gpu.check_mask_before_draw && is_mask_bit_set(*original))
        return;

    if (gpu.set_mask_while_drawing)
        *original = pixel | (1 << 15);
    else
        *original = pixel;
}

void Renderer::write_with_masking(u16* original, const u8 r, const u8 g, const u8 b, const bool mask, const GPU& gpu) const
{
    if (gpu.check_mask_before_draw && is_mask_bit_set(*original))
        return;

    if (gpu.set_mask_while_drawing)
        *original = pack_colour(r, g, b, true);
    else
        *original = pack_colour(r, g, b, mask);
}

void Renderer::write_with_transparency_and_masking(u16* original, const u8 r, const u8 g, const u8 b, const bool mask, const GPU& gpu) const
{
    // Read old 5-bit channels, convert to 8-bit
    u8 old_r, old_g, old_b, old_mask;
    old_r = (u8)(((*original >> 0) & 0b11111) << 3);
    old_g = (u8)(((*original >> 5) & 0b11111) << 3);
    old_b = (u8)(((*original >> 10) & 0b11111) << 3);
    old_mask = (*original >> 15) != 0;

    if (gpu.check_mask_before_draw && old_mask)
        return;

    u8 new_r, new_g, new_b;
    switch (gpu.semi_transparency_mode)
    {
        case 0:
            new_r = (u8)std::min((u16)(old_r >> 1) + (u16)(r >> 1), 255);
            new_g = (u8)std::min((u16)(old_g >> 1) + (u16)(g >> 1), 255);
            new_b = (u8)std::min((u16)(old_b >> 1) + (u16)(b >> 1), 255);
        break;

        case 1:
            new_r = (u8)std::min((u16)old_r + (u16)r, 255);
            new_g = (u8)std::min((u16)old_g + (u16)g, 255);
            new_b = (u8)std::min((u16)old_b + (u16)b, 255);
        break;

        case 2:
            new_r = (u8)std::max((u16)old_r - (u16)r, 0);
            new_g = (u8)std::max((u16)old_g - (u16)g, 0);
            new_b = (u8)std::max((u16)old_b - (u16)b, 0);
        break;

        default:
            new_r = (u8)std::min((u16)old_r + (u16)(r >> 2), 255);
            new_g = (u8)std::min((u16)old_g + (u16)(g >> 2), 255);
            new_b = (u8)std::min((u16)old_b + (u16)(b >> 2), 255);
        break;
    }

    if (gpu.set_mask_while_drawing)
        *original = pack_colour(new_r, new_g, new_b, true);
    else
        *original = pack_colour(new_r, new_g, new_b, mask);
}
