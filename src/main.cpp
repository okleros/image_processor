/*
KIPS - Kleros Image Processing Software. A poor clone of GIMP or any other Image manipulation
software out there. Made entirely for a uni project, and definitely not intended to copy any other
software at all 

Author: Gutemberg Andrade 
Co-author: Unknown 
*/

#include "imgui.h"
#include "implot.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL2/SDL_image.h>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <SDL.h>


#define MAX_STEG_TEXT 524288
#define SIDEBAR_SIZE 360

// Image functions
void img_black_and_white_lum(uint32_t* pixels, int w, int h);
void img_black_and_white(uint32_t* pixels, int w, int h);
void img_negative(uint32_t* pixels, int w, int h);
char* img_reveal(uint32_t* pixels, int w, int h);
void img_gamma(uint32_t* pixels, int w, int h, float c, float gamma);
void img_hide(uint32_t* pixels, int w, int h, const char* text);
void img_log(uint32_t* pixels, int w, int h, float c);

// Setup functions
bool create_window_and_renderer(SDL_Window** window, SDL_Renderer** renderer, SDL_DisplayMode display);
bool remake_texture(SDL_Renderer* renderer, SDL_Texture** texture, int iw, int ih);
bool init_SDL(SDL_DisplayMode* displayMode);

// Auxiliary functions
void generate_normalized_histogram(uint32_t* pixels, int w, int h);
bool get_pixel_array_from_image(SDL_Renderer* renderer, SDL_Texture** texture, const char* img, uint32_t** pixels, int* w, int* h, SDL_Rect* rect, int iaw, int iah);
void convert_hex_to_RGBA(uint32_t color_hex, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
void file_dialog(SDL_Renderer* renderer, SDL_Texture** texture, uint32_t** pixels, int* w, int* h, SDL_Rect* destRect, int iaw, int iah);
void IMG_SaveBMP(uint32_t* pixels, const char* filename, int width, int height);
void space_out(int rep1 = 1, int rep2 = 1);
void img_save(uint32_t* pixels, int width, int height);

namespace fs = std::filesystem;

// Ensure SDL version so the software works correctly
#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

template<typename T>
T clamp(T n, T lower, T upper)
{
    return (T)std::min(std::max(n, lower), upper);
}

template <typename T, size_t N>
T array_max(const T (&array)[N]) {
    // Find the maximum element in the array
    auto max_element_iter = std::max_element(std::begin(array), std::end(array));
    
    // Return the maximum element
    return *max_element_iter;
}

// histogramas em RGB 
float hr[256], hg[256], hb[256];
float maxmax;

// Main code
int main(int, char**)
{

    setbuf(stdout, NULL);

    uint32_t* pixels;
    SDL_DisplayMode display;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* mainTexture;

    SDL_Rect destRect;

    init_SDL(&display);

    // Setup const variables
    int imgAreaWidth = display.w - SIDEBAR_SIZE /*SIDEBAR_SIZE é o tamanho da largura da UI na esquerda*/;
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

    // SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

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
        int a=100, b=150, r = i, v = 0.5*a, L = 256;
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
            ImGui::SetNextWindowSize(ImVec2(SIDEBAR_SIZE, imgAreaHeight-20));
            ImGui::Begin("Image Settings", NULL, ImGuiWindowFlags_NoCollapse);                          // Create a window called "Hello, world!" and append into it.

            if (ImGui::CollapsingHeader("Load/Save Images")) {
                file_dialog(renderer, &mainTexture, &pixels, &imgWidth, &imgHeight, &destRect, imgAreaWidth, imgAreaHeight);
                img_save(pixels, imgWidth, imgHeight);
            }

            // ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            // ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            // ImGui::Checkbox("Another Window", &show_another_window);

            // ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            // ImGui::ColorEdit4("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::CollapsingHeader("Intensity Transformations")) {
                // ImGui::Indent();
                ImGui::BeginGroup();

                if (ImGui::Button("B&W"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_black_and_white(pixels, imgWidth, imgHeight);

                ImGui::SameLine();
                if (ImGui::Button("B&W (Luminance)"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_black_and_white_lum(pixels, imgWidth, imgHeight);

                ImGui::SameLine();
                if (ImGui::Button("Negative"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_negative(pixels, imgWidth, imgHeight);
                
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

                if (ImGui::Button("Correct gamma"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_gamma(pixels, imgWidth, imgHeight, c_gamma, gamma);

                ImGui::EndGroup();
                // ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Steganography")) {
                static char steg_text[MAX_STEG_TEXT]; // Nome do arquivo a ser salvo
                // Campo de texto para o nome do arquivo
                ImGui::InputTextMultiline/*WithHint*/("##Steganography text", steg_text, IM_ARRAYSIZE(steg_text));

                ImGui::SameLine();
                if (ImGui::Button("Hide"))
                    img_hide(pixels, imgWidth, imgHeight, steg_text);

                ImGui::SameLine();
                if (ImGui::Button("Reveal"))
                    printf("%s\n", img_reveal(pixels, imgWidth, imgHeight));
            }

            if (ImGui::CollapsingHeader("Histogram Equalization")) {
                ImGui::Indent();
                ImGui::BeginGroup();

                if (ImGui::Button("Regenerate Histograms"))
                {
                    generate_normalized_histogram(pixels, imgWidth, imgHeight);
                }

                ImGui::PlotHistogram("HistogramR", hr, IM_ARRAYSIZE(hr), 0, NULL, 0.0f, maxmax, ImVec2(0,160));
                ImGui::PlotHistogram("HistogramG", hg, IM_ARRAYSIZE(hg), 0, NULL, 0.0f, maxmax, ImVec2(0,160));
                ImGui::PlotHistogram("HistogramB", hb, IM_ARRAYSIZE(hb), 0, NULL, 0.0f, maxmax, ImVec2(0,160));

                ImGui::EndGroup();
                ImGui::Unindent();
            }

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

            space_out(1, 1);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }
        if (show_demo_window) {
            ImGui::ShowDemoWindow();
        }        

        // Rendering
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 0x21, 0x21, 0x21, 0XFF);
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

void convert_hex_to_RGBA(uint32_t color_hex, float* r, float* g, float* b, float* a)
{
    *r = (color_hex >> 24 & 0xff) / 255.0f;
    *g = (color_hex >> 16 & 0xff) / 255.0f;
    *b = (color_hex >>  8 & 0xff) / 255.0f;
    *a = (color_hex >>  0 & 0xff) / 255.0f;
}

void img_log(uint32_t* pixels, int w, int h, float c)
{
    float r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        // Aplica a transformação logarítmica
        r = 255 * (c * std::log(1.0f + r));
        g = 255 * (c * std::log(1.0f + g));
        b = 255 * (c * std::log(1.0f + b));
        a = 255 * (a);
        
        // Clipping dos valores resultantes
        r = clamp(r, 0.0f, 255.0f);
        g = clamp(g, 0.0f, 255.0f);
        b = clamp(b, 0.0f, 255.0f);
        
        // Atribui os valores de volta ao pixel
        pixels[i] = (uint8_t)r << 24 | (uint8_t)g << 16 | (uint8_t)b << 8 | (uint8_t)a;
    }
}

void img_negative(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        r = 255 * (1 - r);
        g = 255 * (1 - g);
        b = 255 * (1 - b);
        a = 255 * (a);

        pixels[i] = (uint8_t)r << 24 | (uint8_t)g << 16 | (uint8_t)b << 8 | (uint8_t)a;
    }
}

void img_black_and_white(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;
    int media;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        media = 255 * ((r + g + b) / 3);

        a = 255 * a;
        
        pixels[i] = media << 24 | media << 16 | media << 8 | (uint8_t)a;
    }
}

void img_black_and_white_lum(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;

    for (int i = 0; i < w*h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        int media = 255 * ((r * 0.3 + g * 0.59 + b * 0.11));
        
        a = 255 * a;
        
        pixels[i] = media << 24 | media << 16 | media << 8 | (int)a;
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


    // Botão "Save"
    ImGui::SameLine();
    if (ImGui::Button("Save file as")) {
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

    ImGui::SameLine();
    // Campo de texto para o nome do arquivo
    ImGui::InputText("##File Name", filename, IM_ARRAYSIZE(filename));
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

bool get_pixel_array_from_image(SDL_Renderer* renderer, SDL_Texture** texture, const char* img, uint32_t** pixels, int* w, int* h, SDL_Rect* rect, int iaw, int iah)
{
    SDL_Surface* imageSurface = SDL_ConvertSurfaceFormat(IMG_Load(img), SDL_PIXELFORMAT_RGBA8888, 0);
    if (imageSurface == NULL) {
        printf("Unable to load image! SDL_image Error: %s\n", IMG_GetError());

        return 0;
    }

    // Get image dimensions
    *w = imageSurface->w;
    *h = imageSurface->h;

    *rect = { iaw/2 - *w/2, iah/2 - *h/2, *w, *h };

    *pixels = (uint32_t*)malloc((*w) * (*h) * sizeof(uint32_t));

    // Extract pixel data from image surface
    SDL_LockSurface(imageSurface);
    memcpy(*pixels, imageSurface->pixels, (*w) * (*h) * sizeof(uint32_t));
    SDL_UnlockSurface(imageSurface);

    remake_texture(renderer, texture, *w, *h);
    
    generate_normalized_histogram(*pixels, *w, *h);

    SDL_FreeSurface(imageSurface);

    return 1;
}

void img_gamma(uint32_t* pixels, int w, int h, float c, float gamma)
{
    float r, g, b, a;

    for (int i = 0; i < w*h; ++i) {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        r = 255 * (c * std::pow(r, gamma));
        g = 255 * (c * std::pow(g, gamma));
        b = 255 * (c * std::pow(b, gamma));
        a = 255 * (a);
        
        pixels[i] = (uint8_t)r << 24 | (uint8_t)g << 16 | (uint8_t)b << 8 | (uint8_t)a;
    }
}

void img_hide(uint32_t* pixels, int w, int h, const char* text)
{
    float r, g, b, a;
    uint8_t ir, ig, ib, ia, c;

    size_t char_amt_supported = (float)w * h / 2;
    size_t char_amt_given = strlen(text);

    printf("%ld\n", char_amt_given);

    if (strlen(text) > char_amt_supported) {
        printf("The image is not big enough for %ld characters as it only supports %ld. Consider changing your image to at least a %dx%d size image and try again.\n", char_amt_given, char_amt_supported, (int)std::round(std::sqrt(char_amt_given*2)), (int)std::round(std::sqrt(char_amt_given*2)));

        return;
    }

    printf("%ld\n", char_amt_given);
    
    for (int i = 0; i < w*h; i += 2) {
        c = *text;
        for (int j = 0; j < 2; ++j) {
            convert_hex_to_RGBA(pixels[i+j], &r, &g, &b, &a);

            ir = 255 * r;
            ig = 255 * g;
            ib = 255 * b;
            ia = 255 * a;

            ir = (ir & 0xFE) | ((c >> 0x7) & 0x1);
            c <<= 0x1;
            
            ig = (ig & 0xFE) | ((c >> 0x7) & 0x1);
            c <<= 0x1;
            
            ib = (ib & 0xFE) | ((c >> 0x7) & 0x1);
            c <<= 0x1;
            
            ia = (ia & 0xFE) | ((c >> 0x7) & 0x1);
            c <<= 0x1;
            
            pixels[i+j] = ir << 0x18 | ig << 0x10 | ib << 0x8 | ia;
        }

        if (*text == '\0')
            break;

        text++;
    }
}

char* img_reveal(uint32_t* pixels, int w, int h)
{
    char* revealed_text = (char*)malloc(MAX_STEG_TEXT * sizeof(char));

    float r, g, b, a;
    uint8_t ir, ig, ib, ia, current_char;

    for (int i = 0; i < MAX_STEG_TEXT; ++i) {
        current_char = 0;
        for (int j = 0; j < 2; ++j) {
            convert_hex_to_RGBA(pixels[(i*2)+j], &r, &g, &b, &a);

            ir = 255 * r;
            ig = 255 * g;
            ib = 255 * b;
            ia = 255 * a;
            
            current_char = current_char << 0x1 | (ir & 0x1);
            current_char = current_char << 0x1 | (ig & 0x1);
            current_char = current_char << 0x1 | (ib & 0x1);
            current_char = current_char << 0x1 | (ia & 0x1);
        }

        revealed_text[i] = (char)current_char;
   
        if (current_char == '\0')
            break;
    }

    return revealed_text;
}

void rgb2hsv(uint32_t hex_color, uint* h, uint* s, uint* v)
{
    float r, g, b, a;

    convert_hex_to_RGBA(hex_color, &r, &g, &b, &a);

    float cmax = std::fmax(r, std::fmax(g, b));
    float cmin = std::fmin(r, std::fmin(g, b));

    float delta = cmax - cmin;

    if (delta == 0)
        *h = delta;
    
    else if (cmax == r)
        *h = 60 * (fmod(((g - b) / delta), 6.0f));
    
    else if (cmax == g)
        *h = 60 * (((b - r) / delta) + 2);
    
    else if (cmax == b)
        *h = 60 * (((r - g) / delta) + 4);

    else
        *h = 0;

    if (cmax == 0)
        *s = cmax;

    else
        *s = delta/cmax;

    *v = cmax;
}

uint32_t hsv2rgb(uint h, uint s, uint v)
{
    return 0;
}

void space_out(int rep1, int rep2)
{
    for (int i = 0; i < rep1; ++i)
        ImGui::Spacing();
    
    ImGui::Separator();
    
    for (int i = 0; i < rep2; ++i)
        ImGui::Spacing();
}

void generate_normalized_histogram(uint32_t* pixels, int w, int h)
{
    int tot_pixels = w*h;
    float r, g, b, a;
    uint8_t ir, ig, ib;

    // zerar os histogramas atuais para gerar um novo
    memset(hr, 0, sizeof(hr));
    memset(hg, 0, sizeof(hg));
    memset(hb, 0, sizeof(hb));

    for (int i = 0; i < tot_pixels; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        ir = 255 * r;
        ig = 255 * g;
        ib = 255 * b;

        hr[ir]++;
        hg[ig]++;
        hb[ib]++;
    }

    for (int i = 0; i < 256; ++i)
    {
        hr[i] /= tot_pixels;
        hg[i] /= tot_pixels;
        hb[i] /= tot_pixels;
    }

    // Encontra o maior valor entre as 3 arrays a fim de escalar os histogramas com base no maior valor.
    // como a dimensão dos valores é muito discrepante, o histograma na sua forma pura fica pouco
    // representativo, então faço essa escala para refletir melhor os dados. A escala ocorre apenas na
    // visualização, não nos dados
    maxmax = std::fmax(array_max(hr), std::fmax(array_max(hg), array_max(hb)));;
}