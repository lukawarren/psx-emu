#pragma once

class GPU;

class Renderer
{
public:
    struct Triangle
    {
        bool gouraud_shading;
        bool is_textured;
        bool is_transparent;
        bool use_raw_texture;
        u32 colours[3];
        u32 vertices[3];
        u32 uvs[3];

        Triangle(
            const bool gouraud_shading,
            const bool is_textured,
            const bool is_transparent,
            const bool use_raw_texture,
            const u32* colours,
            const u32* vertices,
            const u32* uvs
        ) : gouraud_shading(gouraud_shading), is_textured(is_textured),
            is_transparent(is_transparent), use_raw_texture(use_raw_texture)
        {
            memcpy(this->colours, colours, sizeof(u32) * 3);
            memcpy(this->vertices, vertices, sizeof(u32) * 3);
            memcpy(this->uvs, uvs, sizeof(u32) * 3);
        }
    };

    void push(const GPU& gpu, const Triangle& polygon);
    void render();

private:
    struct Vertex
    {
        i32 x, y;
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
    };

    static inline void unpack_vertex(const u32 vertex, i32& x, i32& y)
    {
        const auto sign_extend_11_bits = [](u32 val)
        {
            u32 res = val & 0x7FF;
            if ((res & (1 << 10)) != 0)
                res |= 0xFFFFF800;
            return (u16)res;
        };

        x = sign_extend_11_bits(vertex & 0x7FF);
        y = sign_extend_11_bits((vertex >> 16) & 0x7FF);
    }

    static inline void unpack_colour(const u32 colour, u8& r, u8& g, u8& b)
    {
        r = ((colour >> 0) & 0xFF);
        g = ((colour >> 8) & 0xFF);
        b = ((colour >> 16) & 0xFF);
    }

    static inline u16 pack_colour(const u8 r, const u8 g, const u8 b, const bool mask_bit)
    {
        // Drop from 8-bit per channel (24-bit colour) to 5-bit (plus mask) for 16-bit
        return (mask_bit ? (1 << 15) : 0) |
               ((u16)(b >> 3) << 10) |
               ((u16)(g >> 3) << 5)  |
                (u16)(r >> 3);
    }

    i32 get_cross_product_z(const Vertex& v0, const Vertex& v1, const Vertex& v2) const;
    void ensure_clockwise_vertex_ordering(Vertex& v0, Vertex& v1, Vertex& v2) const;
    bool is_inside_triangle(const i32 x, const i32 y, const Vertex& v0, const Vertex& v1, const Vertex& v2) const;
    std::array<double, 3> get_barycentric_coordinates(const i32 x, const i32 y, Vertex v0, Vertex v1, Vertex v2) const;

    // Laid out as 512 lines of 2048 bytes each
    // i.e 1024x512 16-bit pixels
    constexpr static i32 vram_width = 1024;
    constexpr static i32 vram_height = 512;
    u16 vram[1024 * 512];
};