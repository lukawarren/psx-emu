#include "renderer.h"
#include "gpu.h"

void Renderer::blit(const u32 x, const u32 y, const u16 pixel)
{
    // Wrap around
    // TODO: respect mask setting
    vram[(y % vram_height) * vram_width + (x % vram_width)] = pixel;
}

void Renderer::push(const GPU& gpu, const Triangle& polygon)
{
    Vertex v[3];
    for (int i = 0; i < 3; i++)
    {
        unpack_vertex(polygon.vertices[i], v[i].x, v[i].y);
        unpack_colour(polygon.colours[i], v[i].r, v[i].g, v[i].b);
        v[i].x += (i32)gpu.drawing_offset_x;
        v[i].y += (i32)gpu.drawing_offset_y;
    }

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
    const bool is_top_left01 = (A01 < 0) || (A01 == 0 && B01 > 0);
    const bool is_top_left12 = (A12 < 0) || (A12 == 0 && B12 > 0);
    const bool is_top_left20 = (A20 < 0) || (A20 == 0 && B20 > 0);

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
                // Interpolate using precomputed values
                u8 r = (u8)std::round(w0 * dr0 + w1 * dr1 + w2 * dr2);
                u8 g = (u8)std::round(w0 * dg0 + w1 * dg1 + w2 * dg2);
                u8 b = (u8)std::round(w0 * db0 + w1 * db1 + w2 * db2);

                *pixel_ptr = pack_colour(r, g, b, false);
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

void Renderer::push(const Rectangle& rectangle)
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
