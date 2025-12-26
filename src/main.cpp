#include "cpu.h"

int main()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("Failed to initialise SDL: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_Window* window = SDL_CreateWindow("psx-emu", Renderer::vram_width, Renderer::vram_height, 0);
    if (window == nullptr)
    {
        printf("Failed to create SDL window: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == nullptr)
    {
        printf("Failed to create SDL renderer: %s\n", SDL_GetError());
        exit(1);
    }

    if (!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE))
        SDL_SetRenderVSync(renderer, 1);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_XBGR1555,
        SDL_TEXTUREACCESS_STREAMING,
        Renderer::vram_width,
        Renderer::vram_height
    );
    if (texture == nullptr)
    {
        printf("Failed to create SDL texture: %s\n", SDL_GetError());
        exit(1);
    }

    CPU* cpu = new CPU();
    bool running = true;
    SDL_Event event;

    const u64 target_frame_time = 1000000000 / 60;
    const u64 frequency = SDL_GetPerformanceFrequency();

    while (running)
    {
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT)
                running = false;

        const u64 frame_start = SDL_GetPerformanceCounter();

        for (u32 i = 0; i < CPU::frequency / 60; i++)
            cpu->execute();

        // Update texture with VRAM
        SDL_UpdateTexture(
            texture,
            nullptr,
            cpu->bus.gpu.renderer.get_vram(),
            Renderer::vram_width * sizeof(uint16_t)
        );

        // Render
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        const u64 frame_time = SDL_GetPerformanceCounter() - frame_start;
        const u64 nanoseconds = (u64)((double)frame_time * 1e9 / (double)frequency);
        if (nanoseconds < target_frame_time)
            SDL_DelayNS(target_frame_time - nanoseconds);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}