#include "imgui.h"
#include "implot.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <iostream>
#include <SDL.h>
#include <SDL2/SDL_image.h>
#include <ctime>
#include <string>
#include <fstream>
#include <filesystem>
#include <algorithm>

// Image functions
void img_black_and_white(uint32_t* pixels, int w, int h);
void img_steganography(uint32_t* pixels, int w, int h, const char* filename);
void img_negative(uint32_t* pixels, int w, int h);
void img_gamma(uint32_t* pixels, int w, int h, float c, float gamma);
void img_log(uint32_t* pixels, int w, int h, float c);

// Setup functions
bool create_window_and_renderer(SDL_Window** window, SDL_Renderer** renderer, SDL_DisplayMode display);
bool remake_texture(SDL_Renderer* renderer, SDL_Texture** texture, int iw, int ih);
bool init_SDL(SDL_DisplayMode* displayMode);


// Auxiliary functions
void get_pixel_array_from_image(SDL_Renderer* renderer, SDL_Texture** texture, const char* img, uint32_t** pixels, int* w, int* h, SDL_Rect* rect, int iaw, int iah);
void convert_hex_to_RGBA(uint32_t color_hex, int* r, int* g, int* b, int* a);
void file_dialog(SDL_Renderer* renderer, SDL_Texture** texture, uint32_t** pixels, int* w, int* h, SDL_Rect* destRect, int iaw, int iah);
void IMG_SaveBMP(uint32_t* pixels, const char* filename, int width, int height);
void space_out(int rep1 = 1, int rep2 = 1);
void img_save(uint32_t* pixels, int width, int height);

namespace fs = std::filesystem;

// Ensure SDL version so the software works correctly
#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

// Main code
int main(int, char**)
{
    srand(time(NULL));

    uint32_t* pixels;
    SDL_DisplayMode display;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* mainTexture;

    SDL_Rect destRect;

    init_SDL(&display);

    // Setup const variables
    int imgAreaWidth = display.w - 340 /*340 é o tamanho da largura da UI na esquerda*/;
    int imgAreaHeight = display.h/*- 69*/ /*69 é o tamanho da barra superior, temos que compensar na imagem*/;

    // Get image dimensions
    int imgWidth = imgAreaWidth;
    int imgHeight = imgAreaHeight;

    destRect = { 0, 0, imgWidth, imgHeight};

    // Create pixel array to hold pixel data
    pixels = (uint32_t*)malloc(imgWidth * imgHeight * sizeof(uint32_t));
    if (pixels == NULL) {
        printf("Unable to allocate memory for pixel array!\n");
        return 1;
    }

    memset(pixels, 0x21, imgWidth * imgHeight * sizeof(uint32_t));
    
    if (!create_window_and_renderer(&window, &renderer, display))
        return -1;
    if (!remake_texture(renderer, &mainTexture, imgWidth, imgHeight))
        return -1;

    // Create window icon surface
    SDL_SetWindowIcon(window, SDL_ConvertSurfaceFormat(IMG_Load("../res/icon/sun-2081062_640.png"), SDL_PIXELFORMAT_RGBA8888, 0));

    SDL_SetWindowFullscreen(window, /*SDL_WINDOW_FULLSCREEN*/0);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Our state 
    bool show_demo_window = false;

    static std::vector<float> graphData(256, 0);

    for (size_t i = 0; i < graphData.size(); ++i)
    {
        int a=100, b=150, r = i, v = 0.5*a, w = 0.5*b, L = 256;
        float l=.5, m=2, n=.5; 

        if ((0 <= r) & (r < a))
            graphData[i] = r * l;

        else if ((a <= r) & (r < b))
            graphData[i] = m * (r - a) + v;

        else if ((b <= r) & (r <= L-1))
            graphData[i] = n * (r - b) + v + 2 * (b - a);
    }

    // Main loop
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            // static float f = 0.0f;
            // static int counter = 0;
            ImGui::SetNextWindowPos(ImVec2(imgAreaWidth, 0));
            ImGui::SetNextWindowSize(ImVec2(340, imgAreaHeight));
            ImGui::Begin("Image Settings", NULL, ImGuiWindowFlags_NoCollapse);                          // Create a window called "Hello, world!" and append into it.

            space_out(0, 2);

            file_dialog(renderer, &mainTexture, &pixels, &imgWidth, &imgHeight, &destRect, imgAreaWidth, imgAreaHeight);
            
            space_out(2, 2);

            img_save(pixels, imgWidth, imgHeight);

            space_out(2);

            // ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            // ImGui::Checkbox("Another Window", &show_another_window);

            // ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            // ImGui::ColorEdit4("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            ImGui::SameLine();
            if (ImGui::Button("B&W"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                img_black_and_white(pixels, imgWidth, imgHeight);
            ImGui::SameLine();
            if (ImGui::Button("Negative"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                img_negative(pixels, imgWidth, imgHeight);

            space_out();

            static float c_log = 0;
            ImGui::Text("c_log");
            ImGui::DragFloat("##c_log", &c_log, 0.1f, 0.0f, 100.0f);
            ImGui::SameLine();
            if (ImGui::Button("Log"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                img_log(pixels, imgWidth, imgHeight, c_log);
 
            space_out();

            static float c_gamma = 0;
            static float gamma = 0;
            ImGui::Text("c_gamma");
            ImGui::DragFloat("##c_gamma", &c_gamma, 0.1f, 0.0f, 100.0f);
            ImGui::Text("gamma");
            ImGui::DragFloat("##gamma", &gamma, 0.1f, 0.0f, 100.0f);
            ImGui::SameLine();
            if (ImGui::Button("Correct gamma"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                img_gamma(pixels, imgWidth, imgHeight, c_gamma, gamma);

            space_out();

            static char steg_text[256] = ""; // Nome do arquivo a ser salvo
            // Campo de texto para o nome do arquivo
            ImGui::InputTextWithHint("##Steganography text", "hidden text (max 256 chars)", steg_text, IM_ARRAYSIZE(steg_text));

            ImGui::SameLine();
            if (ImGui::Button("Hide"))
                img_steganography(pixels, imgWidth, imgHeight, steg_text);

            ImGui::SameLine();
            if (ImGui::Button("Reveal"))
                img_steganography(pixels, imgWidth, imgHeight, steg_text);


            space_out();

            // static int p1[2] = { 0, 0 };

            // ImGui::DragInt2("mimimi", p1, 1, 0, 9);
            // ImGui::DragFloat("graph", &graphData[p1[0] + 10 * p1[1]], 1.0f, 0.0f, 255.0f);
            // ImGui::PlotLines("Graph", graphData.data(), graphData.size(), 0, NULL, FLT_MAX, 255, ImVec2(0, 180));

            // static float data[256];
            // if (ImPlot::BeginPlot("My Plot", "X", "Y")) {
            //     for (int i = 0; i < 256; ++i) {
            //         data[i] = i;
            //     }
            //     ImPlot::PlotLine("Sine Wave", data, 256);
            //     ImPlot::EndPlot();
            // }

            space_out(100, 1);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }
        if (show_demo_window) {
            ImGui::ShowDemoWindow();
        }        

        // Rendering
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 0x21, 0x21, 0x21, 0x21);
        SDL_RenderClear(renderer);
        SDL_UpdateTexture(mainTexture, NULL, pixels, imgWidth * sizeof(uint32_t));

        // Render main texture
        
        // Present renderer
        SDL_RenderCopy(renderer, mainTexture, NULL, &destRect);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void convert_hex_to_RGBA(uint32_t color_hex, int* r, int* g, int* b, int* a)
{
    *r = color_hex >> 24 & 0xff;
    *g = color_hex >> 16 & 0xff;
    *b = color_hex >>  8 & 0xff;
    *a = color_hex >>  0 & 0xff;
}

void img_log(uint32_t* pixels, int w, int h, float c)
{
    int r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        r = c * std::log10(r + 1);
        g = c * std::log10(g + 1);
        b = c * std::log10(b + 1);
        
        pixels[i] = r << 24 | g << 16 | b << 8 | a;
    }
}

void img_negative(uint32_t* pixels, int w, int h)
{
    int r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        r = 0xFF - r;
        g = 0xFF - g;
        b = 0xFF - b;

        pixels[i] = r << 24 | g << 16 | b << 8 | a;
    }
}

void img_black_and_white(uint32_t* pixels, int w, int h)
{
    int r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        int media = (r+g+b)/3;
        
        r = media;
        g = media;
        b = media;
        
        pixels[i] = r << 24 | g << 16 | b << 8 | a;
    }
}

void file_dialog(SDL_Renderer* renderer, SDL_Texture** texture, uint32_t** pixels, int* w, int* h, SDL_Rect* destRect, int iaw, int iah)
{
    if (ImGui::Button("Open File")) {
        // Abre o diálogo de arquivo
        ImGui::OpenPopup("Select a file");
    }

    const char* selectedFilePath;

    // Diálogo de arquivo
    if (ImGui::BeginPopupModal("Select a file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Itera pelos arquivos e diretórios do diretório atual e subdiretórios
        if (ImGui::Selectable(".")) goto exit_popup;
        for (const auto& entry : fs::/*recursive_*/directory_iterator("./../res")) {
            if (!fs::is_directory(entry.path()) && ImGui::Selectable(entry.path().string().c_str()+9)) {
                // Se um arquivo ou diretório é selecionado, armazena o caminho absoluto
                selectedFilePath = entry.path().c_str();

                get_pixel_array_from_image(renderer, texture, selectedFilePath, pixels, w, h, destRect, iaw, iah);

                ImGui::CloseCurrentPopup(); // Fecha o diálogo
                goto exit_popup;
            }
        }
    exit_popup:
        ImGui::EndPopup();
    }
}

void IMG_SaveBMP(uint32_t* pixels, const char* filename, int width, int height)
{
    std::ofstream outFile(filename, std::ios::binary);

    // BMP header
    const uint32_t fileSize = 54 + width * height * 4; // 54 is the size of BMP header

    uint8_t header[54] = {
        'B','M',                           // Signature
        static_cast<uint8_t>(fileSize),    // File size in bytes
        static_cast<uint8_t>(fileSize >> 8),
        static_cast<uint8_t>(fileSize >> 16),
        static_cast<uint8_t>(fileSize >> 24),
        0,0,0,0,                           // Reserved
        54,0,0,0,                          // Offset to pixel data
        40,0,0,0,                          // Header size
        static_cast<uint8_t>(width),       // Image width
        static_cast<uint8_t>(width >> 8),
        static_cast<uint8_t>(width >> 16),
        static_cast<uint8_t>(width >> 24),
        static_cast<uint8_t>(height),      // Image height
        static_cast<uint8_t>(height >> 8),
        static_cast<uint8_t>(height >> 16),
        static_cast<uint8_t>(height >> 24),
        1,0,                              // Planes
        24,0,                             // Bits per pixel (24 = RGB)
        0,0,0,0,                          // Compression
        0,0,0,0,                          // Image size (unspecified)
        0,0,0,0,                          // X pixels per meter (unspecified)
        0,0,0,0,                          // Y pixels per meter (unspecified)
        0,0,0,0,                          // Colors used (unspecified)
        0,0,0,0                           // Important colors (unspecified)
    };

    outFile.write(reinterpret_cast<char*>(header), 54); // Write header

    // Write pixel data
    for (int i = height - 1; i >= 0; --i) {
        for (int j = 0; j < width; ++j) {
            uint32_t pixel = pixels[i * width + j];
            uint8_t r = (pixel >> 24) & 0xFF;
            uint8_t g = (pixel >> 16) & 0xFF;
            uint8_t b = (pixel >>  8) & 0xFF;

            outFile.write(reinterpret_cast<const char*>(&b), 1);
            outFile.write(reinterpret_cast<const char*>(&g), 1);
            outFile.write(reinterpret_cast<const char*>(&r), 1);
        }
    }

    outFile.close();
}

bool remake_texture(SDL_Renderer* renderer, SDL_Texture** texture, int iw, int ih)
{
    *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, iw, ih);
    if (*texture == NULL) {
        printf("Unable to create main texture! SDL Error: %s\n", SDL_GetError());
        return 0;
    }

    return 1;
}

bool create_window_and_renderer(SDL_Window** window, SDL_Renderer** renderer, SDL_DisplayMode display)
{
    *window = SDL_CreateWindow("Image Processor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, display.w, display.h, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (*window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 0;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (*renderer == nullptr)
    {
        SDL_Log("Error creating SDL_Renderer!");
        return 0;
    }

    return 1;
}

void img_save(uint32_t* pixels, int width, int height)
{
    static char filename[128] = ""; // Nome do arquivo a ser salvo

    // Campo de texto para o nome do arquivo
    ImGui::InputText("##File Name", filename, IM_ARRAYSIZE(filename));

    // Botão "Save"
    ImGui::SameLine();
    if (ImGui::Button("Save File")) {
        std::string file = filename;
        std::string ext = file.substr(file.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Diretório onde a imagem será salva
        std::string directory = "../res/";

        // Verifica se o diretório existe, se não, cria-o
        if (!fs::exists(directory))
            fs::create_directory(directory);

        // Caminho completo do arquivo
        std::string filepath = directory + file;

        // Salva a imagem com base na extensão
        if (ext == "bmp") {
            IMG_SaveBMP(pixels, filepath.c_str(), width, height);
            std::cout << "Imagem salva com sucesso em: " << filepath << std::endl;
        } else {
            SDL_Surface* surface = SDL_CreateRGBSurfaceFrom((void*)pixels, width, height, 32, width * 4, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);

            if (surface != NULL) {
                if (ext == "png") {
                    IMG_SavePNG(surface, filepath.c_str());
                } else if (ext == "jpg") {
                    IMG_SaveJPG(surface, filepath.c_str(), 100);
                }

                std::cout << "Imagem salva com sucesso em: " << filepath << std::endl;

                // Libera a superfície
                SDL_FreeSurface(surface);
            } else {
                std::cout << "Erro ao criar a superfície SDL!" << std::endl;
            }
        }
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Extensão de arquivo inválida! Use .bmp, .png ou .jpg.");
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

bool init_SDL(SDL_DisplayMode* displayMode)
{
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
    
        return 0;
    }

    // Getting useful info about the display (imgWidth and imgHeight)
    if (SDL_GetDesktopDisplayMode(0, displayMode) != 0) {
        printf("Unable to get desktop display mode! SDL_Error: %s\n", SDL_GetError());
    
        return 0;
    }

    return 1;
}

void get_pixel_array_from_image(SDL_Renderer* renderer, SDL_Texture** texture, const char* img, uint32_t** pixels, int* w, int* h, SDL_Rect* rect, int iaw, int iah)
{
    SDL_Surface* imageSurface = SDL_ConvertSurfaceFormat(IMG_Load(img), SDL_PIXELFORMAT_RGBA8888, 0);
    if (imageSurface == NULL) {
        printf("Unable to load image! SDL_image Error: %s\n", IMG_GetError());

        return;
    }

    // Get image dimensions
    *w = imageSurface->w;
    *h = imageSurface->h;

    *rect = { iaw/2 - *w/2, iah/2 - *h/2, *w, *h };

    *pixels = (uint32_t*)malloc((*w) * (*h) * sizeof(uint32_t));

    /*if (*w > *h && (float)iaw/(*w) * (*h) <= iah) {
        *rect = { 0, 0,  iaw, (int)((float)iaw/(*w) * (*h)) };
    
    } else {
        *rect = { 0, 0, (int)((float)iah/(*h) * (*w)), iah };

    }*/

    // Extract pixel data from image surface
    SDL_LockSurface(imageSurface);
    memcpy(*pixels, imageSurface->pixels, (*w) * (*h) * sizeof(uint32_t));
    SDL_UnlockSurface(imageSurface);

    remake_texture(renderer, texture, *w, *h);
    
    SDL_FreeSurface(imageSurface);
}

void img_gamma(uint32_t* pixels, int w, int h, float c, float gamma)
{
    int r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        r = 255 * (c * std::pow((float)r / 255.0f, gamma));
        g = 255 * (c * std::pow((float)g / 255.0f, gamma));
        b = 255 * (c * std::pow((float)b / 255.0f, gamma));
        
        pixels[i] = r << 24 | g << 16 | b << 8 | a;
    }
}

void img_steganography(uint32_t* pixels, int w, int h, const char* filename)
{
    int r, g, b, a;

    int chars = 0;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        r &= 0xFE;
        g &= 0xFE;
        b &= 0xFE;
        a &= 0xFE;
        
        pixels[i] = r << 24 | g << 16 | b << 8 | a;
    
        chars += 4;
    }

    chars /= 8;
}

void space_out(int rep1, int rep2)
{
    for (int i = 0; i < rep1; ++i)
    {
        ImGui::Spacing();
    }
    
    ImGui::Separator();
    
    for (int i = 0; i < rep2; ++i)
    {
        ImGui::Spacing();
    }
}