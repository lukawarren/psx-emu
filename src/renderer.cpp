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
    }

    // Add drawing offset
    for (int i = 0;  i < 3; i++)
    {
        v[i].x += (i32)gpu.drawing_offset_x;
        v[i].y += (i32)gpu.drawing_offset_y;
    }

    ensure_clockwise_vertex_ordering(v[0], v[1], v[2]);

    // Compute bounding box of triangle
    i32 min_x = std::min(v[0].x, std::min(v[1].x, v[2].x));
    i32 min_y = std::min(v[0].y, std::min(v[1].y, v[2].y));
    i32 max_x = std::max(v[0].x, std::max(v[1].x, v[2].x));
    i32 max_y = std::max(v[0].y, std::max(v[1].y, v[2].y));

    // Confine box to within the drawing area
    min_x = std::max(min_x, (i32)gpu.drawing_area_x1);
    max_x = std::min(max_x, (i32)gpu.drawing_area_x2);
    min_y = std::max(min_y, (i32)gpu.drawing_area_y1);
    max_y = std::min(max_y, (i32)gpu.drawing_area_y2);

    // Skip triangle if outside the drawing area
    if (min_x > max_x || min_y > max_y)
        return;

    // Rasterise (note that min and max are inclusive)
    for (i32 y = min_y; y <= max_y; y++)
    {
        for (i32 x = min_x; x <= max_x; x++)
        {
            if (is_inside_triangle(x, y, v[0], v[1], v[2]))
            {
                // TODO: what happens when co-ord outside of VRAM?
                if (y < 0 || y >= vram_height) continue;
                if (x < 0 || x >= vram_width) continue;

                const std::array<double, 3> lambda = get_barycentric_coordinates(
                    x, y, v[0], v[1], v[2]
                );

                // Interpolate colours
                const double rs[3] = { (double)v[0].r, (double)v[1].r, (double)v[2].r };
                const double gs[3] = { (double)v[0].g, (double)v[1].g, (double)v[2].g };
                const double bs[3] = { (double)v[0].b, (double)v[1].b, (double)v[2].b };
                u8 r = (u8)std::round(lambda[0] * rs[0] + lambda[1] * rs[1] + lambda[2] * rs[2]);
                u8 g = (u8)std::round(lambda[0] * gs[0] + lambda[1] * gs[1] + lambda[2] * gs[2]);
                u8 b = (u8)std::round(lambda[0] * bs[0] + lambda[1] * bs[1] + lambda[2] * bs[2]);

                // TODO: deal with masking bit, etc.
                vram[y * vram_width + x] = pack_colour(r, g, b, false);
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

std::array<double, 3> Renderer::get_barycentric_coordinates(const i32 x, const i32 y, Vertex v0, Vertex v1, Vertex v2) const
{
    const i32 denominator = get_cross_product_z(v0, v1, v2);
    if (denominator == 0)
    {
        // TODO: think actually you just don't draw anything (i.e. skip earlier)?
        //       vertices are collinear
        return { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 };
    }

    const double lambda0 = (double)(get_cross_product_z(v1, v2, Vertex { x, y })) / (double)denominator;
    const double lambda1 = (double)(get_cross_product_z(v2, v0, Vertex { x, y })) / (double)denominator;
    const double lambda2 = 1.0 - lambda0 - lambda1;

    return { lambda0, lambda1, lambda2 };
}