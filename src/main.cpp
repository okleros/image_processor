/*
KIMS - Kleros Image Manipularion Software. A poor clone of GIMP or any other Image manipulation
software out there. Made entirely for a uni project, and definitely not intended to copy any other
software at all (or be copied, this code sucks!)

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
#include <stack>
#include <SDL.h>


#define MAX_STEG_TEXT 524288
#define SIDEBAR_SIZE 360

struct Kernel {
    int size;        // The size of the Kernel, e.g. 3, for a 3x3 kernel.
    float* elements; // Pointer to the Kernel elements
};

// Image functions
void img_generate_equalized_histogram(uint32_t* pixels, int w, int h, uint8_t c);
void img_black_and_white_lum(uint32_t* pixels, int w, int h);
void img_black_and_white(uint32_t* pixels, int w, int h);
void apply_convolution(uint32_t* pixels, int w, int h, Kernel k);
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
void generate_normalized_histogram(uint32_t* pixels, int w, int h, uint8_t c);
void check_if_image_is_grayscale(uint32_t* pixels, int w, int h);
bool get_pixel_array_from_image(SDL_Renderer* renderer, SDL_Texture** texture, const char* img, uint32_t** pixels, int* w, int* h, SDL_Rect* rect, int iaw, int iah);
uint32_t convert_RGBA_to_hex(float r, float g, float b, float a);
void convert_hex_to_RGBA(uint32_t color_hex, float* r, float* g, float* b, float* a);
void img_threshold(uint32_t* pixels, int w, int h, uint8_t c);
void file_dialog(SDL_Renderer* renderer, SDL_Texture** texture, uint32_t** pixels, int* w, int* h, SDL_Rect* destRect, int iaw, int iah);
void IMG_SaveBMP(uint32_t* pixels, const char* filename, int width, int height);
void clear_stack(std::stack<uint32_t*>& stack);
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

bool is_img_grayscale = true;

// histogramas em RGB 
float hr[256], hg[256], hb[256];
float* histograms[3] = {hr, hg, hb};

// CDFs de cada canal
float CDFr[256], CDFg[256], CDFb[256];
float* CDF[3] = {CDFr, CDFg, CDFb};

float maxmax;

std::stack<uint32_t*> undo_stack;
std::stack<uint32_t*> redo_stack;
std::stack<std::string> undo_log_stack;
std::stack<std::string> redo_log_stack;

// Main code
int main(int, char**)
{

    setbuf(stdout, NULL);

    uint32_t* pixels = nullptr;
    SDL_DisplayMode display;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* mainTexture = nullptr;

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

    std::string text;
    
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

        {
            // static float f = 0.0f;
            // static int counter = 0;
            ImGui::SetNextWindowPos(ImVec2(imgAreaWidth, 0));
            ImGui::SetNextWindowSize(ImVec2(SIDEBAR_SIZE, imgAreaHeight-20));
            ImGui::Begin("Image Settings", NULL, ImGuiWindowFlags_NoCollapse);                          // Create a window called "Hello, world!" and append into it.
            
            space_out();

            // load/save images
            file_dialog(renderer, &mainTexture, &pixels, &imgWidth, &imgHeight, &destRect, imgAreaWidth, imgAreaHeight);
            img_save(pixels, imgWidth, imgHeight);

            space_out();

            if (ImGui::Button("Undo"))
            {
                if (!undo_stack.empty())
                {
                    redo_stack.push(pixels);
                    pixels = undo_stack.top();
                    undo_stack.pop();

                    text = "[UNDO] " + undo_log_stack.top();
                    redo_log_stack.push(undo_log_stack.top());
                    undo_log_stack.pop();

                    check_if_image_is_grayscale(pixels, imgWidth, imgHeight);

                    generate_normalized_histogram(pixels, imgWidth, imgHeight, 0);
                    generate_normalized_histogram(pixels, imgWidth, imgHeight, 1);
                    generate_normalized_histogram(pixels, imgWidth, imgHeight, 2);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Redo"))
            {
                if (!redo_stack.empty())
                {
                    undo_stack.push(pixels);
                    pixels = redo_stack.top();
                    redo_stack.pop();

                    text = "[REDO] " + redo_log_stack.top();
                    undo_log_stack.push(redo_log_stack.top());
                    redo_log_stack.pop();

                    check_if_image_is_grayscale(pixels, imgWidth, imgHeight);

                    generate_normalized_histogram(pixels, imgWidth, imgHeight, 0);
                    generate_normalized_histogram(pixels, imgWidth, imgHeight, 1);
                    generate_normalized_histogram(pixels, imgWidth, imgHeight, 2);
                }
            }
            ImGui::SameLine();
            ImGui::Text(text.c_str(), 1000.0f / io.Framerate, io.Framerate);

            space_out();

            // ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            // ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            // ImGui::Checkbox("Another Window", &show_another_window);

            // ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            // ImGui::ColorEdit4("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::CollapsingHeader("Intensity Transformations")) {
                // ImGui::Indent();
                ImGui::BeginGroup();

                space_out();
                if (ImGui::Button("B&W")){                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_black_and_white(pixels, imgWidth, imgHeight);
                }
                ImGui::SameLine();
                if (ImGui::Button("B&W (Luminance)"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_black_and_white_lum(pixels, imgWidth, imgHeight);

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
                
                static int threshold = 0;
                ImGui::Text("threshold");
                ImGui::DragInt("##threshold", &threshold, 1.0f, 0.0f, 255.0f);

                ImGui::SameLine();
                if (ImGui::Button("Threshold"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_threshold(pixels, imgWidth, imgHeight, threshold);
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

                float gx[9] = {
    -1,   0,  1,
    -2,   0,  2,
    -1,   0,  1
};
                static Kernel k = {3, gx};

                if (ImGui::Button("Convolve"))
                {
                    apply_convolution(pixels, imgWidth, imgHeight, k);
                }

                ImGui::EndGroup();
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
                if (ImGui::Button("Equalize Full Image"))
                {
                    img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 0);
                    img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 1);
                    img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 2);
                }

                space_out(0, 0);

                ImGui::Indent();
                ImGui::BeginGroup();
                if (!is_img_grayscale) {
                    
                    if (ImGui::Button("Equalize RED"))
                    {
                        img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 0);
                    }
                    ImGui::PlotHistogram("HistogramR", hr, IM_ARRAYSIZE(hr), 0, NULL, 0.0f, maxmax, ImVec2(0,160));

                    if (ImGui::Button("Equalize GREEN"))
                    {
                        img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 1);
                    }
                    ImGui::PlotHistogram("HistogramG", hg, IM_ARRAYSIZE(hg), 0, NULL, 0.0f, maxmax, ImVec2(0,160));

                    if (ImGui::Button("Equalize BLUE"))
                    {
                        img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 2);
                    }
                    ImGui::PlotHistogram("HistogramB", hb, IM_ARRAYSIZE(hb), 0, NULL, 0.0f, maxmax, ImVec2(0,160));
                } else {
                    ImGui::PlotHistogram("##HistogramR", hr, IM_ARRAYSIZE(hr), 0, NULL, 0.0f, maxmax, ImVec2(0,160));
                }
                
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

void clear_stack(std::stack<uint32_t*>& stack)
{
    std::stack<uint32_t*> empty;
    std::swap(stack, empty);
}

void convert_hex_to_RGBA(uint32_t color_hex, float* r, float* g, float* b, float* a)
{
    *r = (color_hex >> 24 & 0xff) / 255.0f;
    *g = (color_hex >> 16 & 0xff) / 255.0f;
    *b = (color_hex >>  8 & 0xff) / 255.0f;
    *a = (color_hex >>  0 & 0xff) / 255.0f;
}

uint32_t convert_RGBA_to_hex(float r, float g, float b, float a)
{
    return ((int)(r*255) << 24) | ((int)(g*255) << 16) | ((int)(b*255) << 8) | (int)a*255;
}

void img_log(uint32_t* pixels, int w, int h, float c)
{
    float r, g, b, a;

    uint32_t* old_pixels = new uint32_t[w*h];//(uint32_t*)malloc(w * h * sizeof(uint32_t));

    for (int i = 0; i < w*h; ++i)
    {
        old_pixels[i] = pixels[i];

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

    undo_stack.push(old_pixels);
    undo_log_stack.push("log transformation");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
}

void img_threshold(uint32_t* pixels, int w, int h, uint8_t threshold)
{
    if (0)
    {
        printf("Image is not in grayscale!\n");
    
    } else {
        float intensity, lixo;

        uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));

        for (int i = 0; i < w*h; ++i)
        {
            old_pixels[i] = pixels[i];

            convert_hex_to_RGBA(pixels[i], &intensity, &lixo, &lixo, &lixo);
            
            if ((int)(intensity * 255.0f) < threshold)
                intensity = 0;
            else
                intensity = 255;

            // Atribui os valores de volta ao pixel
            pixels[i] = (uint8_t)intensity << 24 | (uint8_t)intensity << 16 | (uint8_t)intensity << 8 | (uint8_t)intensity;
        }

        undo_stack.push(old_pixels);

        std::string log_text = "thresholding by " + std::to_string(threshold);

        undo_log_stack.push(log_text);
    }

    generate_normalized_histogram(pixels, w, h, 0);
}

void img_negative(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;

    uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));

    for (int i = 0; i < w*h; ++i)
    {
        old_pixels[i] = pixels[i];

        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        r = 255 * (1 - r);
        g = 255 * (1 - g);
        b = 255 * (1 - b);
        a = 255 * (a);

        pixels[i] = (uint8_t)r << 24 | (uint8_t)g << 16 | (uint8_t)b << 8 | (uint8_t)a;
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("negative");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
}

void img_black_and_white(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;
    int media;

    uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    
    for (int i = 0; i < w*h; ++i)
    {
        old_pixels[i] = pixels[i];
    
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        media = 255 * ((r + g + b) / 3);

        a = 255 * a;
        
        pixels[i] = media << 24 | media << 16 | media << 8 | (uint8_t)a;
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("grayscale (no luminance)");

    is_img_grayscale = true;

    generate_normalized_histogram(pixels, w, h, 0);
}

void img_black_and_white_lum(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;

    uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));

    for (int i = 0; i < w*h; ++i)
    {
        old_pixels[i] = pixels[i];
    
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        int media = 255 * ((r * 0.3 + g * 0.59 + b * 0.11));
        
        a = 255 * a;
        
        pixels[i] = media << 24 | media << 16 | media << 8 | (int)a;
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("grayscale (with luminance)");
    
    is_img_grayscale = true;

    generate_normalized_histogram(pixels, w, h, 0);
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

                free(*pixels);
                get_pixel_array_from_image(renderer, texture, selectedFilePath, pixels, w, h, destRect, iaw, iah);
                clear_stack(undo_stack);
                clear_stack(redo_stack);

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
    
    generate_normalized_histogram(*pixels, *w, *h, 0);
    generate_normalized_histogram(*pixels, *w, *h, 1);
    generate_normalized_histogram(*pixels, *w, *h, 2);

    check_if_image_is_grayscale(*pixels, *w, *h);

    SDL_FreeSurface(imageSurface);

    return 1;
}

void check_if_image_is_grayscale(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;

    is_img_grayscale = true; // If any pixel has equal R, G, B values, the image not BW
    for (int i = 0; i < w * h; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        if (r != g || g != b || r != b) {
            is_img_grayscale = false; // If any pixel has different R, G, B values, the image is not BW
        }
    }
}

void img_gamma(uint32_t* pixels, int w, int h, float c, float gamma)
{
    float r, g, b, a;

    uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));

    for (int i = 0; i < w*h; ++i) {
        old_pixels[i] = pixels[i];

        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        
        r = 255 * (c * std::pow(r, gamma));
        g = 255 * (c * std::pow(g, gamma));
        b = 255 * (c * std::pow(b, gamma));
        a = 255 * (a);
        
        pixels[i] = (uint8_t)r << 24 | (uint8_t)g << 16 | (uint8_t)b << 8 | (uint8_t)a;
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("gamma transformation");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
}

void img_hide(uint32_t* pixels, int w, int h, const char* text)
{
    float r, g, b, a;
    uint8_t ir, ig, ib, ia, c;

    size_t char_amt_supported = (float)w * h / 2;
    size_t char_amt_given = strlen(text);

    if (strlen(text) > char_amt_supported) {
        printf("The image is not big enough for %ld characters as it only supports %ld. Consider changing your image to at least a %dx%d size image and try again.\n", char_amt_given, char_amt_supported, (int)std::round(std::sqrt(char_amt_given*2)), (int)std::round(std::sqrt(char_amt_given*2)));

        return;
    }

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

void generate_normalized_histogram(uint32_t* pixels, int w, int h, uint8_t c)
{
    int tot_pixels = w*h;
    float r, g, b, a;
    uint8_t ind;

    float* current_histogram = histograms[c];

    // zerar os histogramas atuais para gerar um novo
    memset(current_histogram, 0, sizeof(hr));
    // hr é o histograma em R (red), estou usando ele no sizeof pois se usar current_histogram
    // o compilador reclama, já que current_histogram não tem tamanho definido em tempo de compilação
    // já hr sim (current_histogram é o ponteiro que aponta para o início do vetor hr, e somente o vetor
    // hr foi inicializado com a notação [], já que todos os histogramas tem o mesmo tamanho, não há
    // mal em colocar o hr aqui)

    for (int i = 0; i < tot_pixels; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        float colors[3] = {r, g, b};

        ind = 255 * colors[c];

        current_histogram[ind]++;
    }

    for (int i = 0; i < 256; ++i)
    {
        current_histogram[i] /= tot_pixels;
    }

    // Encontra o maior valor entre as 3 arrays a fim de escalar os histogramas com base no maior valor.
    // como a dimensão dos valores é muito discrepante, o histograma na sua forma pura fica pouco
    // representativo, então faço essa escala para refletir melhor os dados. A escala ocorre apenas na
    // visualização, não nos dados
    maxmax = std::fmax(array_max(hr), std::fmax(array_max(hg), array_max(hb)));;
}

void generate_CDF(uint8_t c)
{
    float* current_histogram = histograms[c];
    float* current_cdf = CDF[c];

    current_cdf[0] = current_histogram[0];

    for (int i = 1; i < 256; ++i)
    {
        current_cdf[i] = current_cdf[i-1] + current_histogram[i];
    }
}

void img_generate_equalized_histogram(uint32_t* pixels, int w, int h, uint8_t c)
{
    uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));

    // primeira vez para fazer os cálculos
    generate_normalized_histogram(pixels, w, h, c);
    generate_CDF(c);

    int tot_pixels = w * h;
    float r, g, b, a;
    float* current_cdf = CDF[c];

    for (int i = 0; i < tot_pixels; ++i)
    {
        old_pixels[i] = pixels[i];

        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        float colors[3] = { r, g, b };

        uint8_t old_intensity = (uint8_t)(255 * colors[c]);
        uint8_t new_intensity = (uint8_t)(current_cdf[old_intensity] * 255);

        colors[c] = new_intensity / 255.0f;

        // Reconstituir o pixel com os novos valores equalizados
        pixels[i] = convert_RGBA_to_hex(colors[0], colors[1], colors[2], a);
    }

    undo_stack.push(old_pixels);

    if (c == 0)
        undo_log_stack.push("Equalize RED Histogram");

    if (c == 1)
        undo_log_stack.push("Equalize GREEN Histogram");

    if (c == 2)
        undo_log_stack.push("Equalize BLUE Histogram");

    // gerando o histograma normalizado de novo para mostrar na GUI
    generate_normalized_histogram(pixels, w, h, c);
    generate_normalized_histogram(pixels, w, h, c);
}

// Função para aplicar convolução em uma imagem RGBA
void apply_convolution(uint32_t* pixels, int w, int h, Kernel k) {
    int halfSize = k.size / 2;
    uint32_t* old_pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));

    for (int i = 0; i < k.size*k.size; ++i)
    {
        printf("%f ", k.elements[i]);
    }

    // Iterar sobre cada pixel da imagem
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            old_pixels[y * w + x] = pixels[y * w + x];

            float r = 0, g = 0, b = 0, a = 0;

            // Aplicar o kernel
            for (int ky = -halfSize; ky <= halfSize; ++ky) {
                for (int kx = -halfSize; kx <= halfSize; ++kx) {
                    int ix = x + kx;
                    int iy = y + ky;

                    // Verificar se estamos dentro dos limites da imagem
                    if (ix >= 0 && ix < w && iy >= 0 && iy < h) {
                        float kr = k.elements[(ky + halfSize) * k.size + (kx + halfSize)];
                        
                        float pr, pg, pb, pa;
                        convert_hex_to_RGBA(old_pixels[iy * w + ix], &pr, &pg, &pb, &pa);

                        r += pr * kr;
                        g += pg * kr;
                        b += pb * kr;
                        a += pa * kr;
                    }
                }
            }

            // Clamping
            r = std::min(std::max(r, 0.0f), 1.0f);
            g = std::min(std::max(g, 0.0f), 1.0f);
            b = std::min(std::max(b, 0.0f), 1.0f);
            a = std::min(std::max(a, 0.0f), 1.0f);

            // Converter de volta para hexadecimal e armazenar no array de saída
            pixels[y * w + x] = convert_RGBA_to_hex(r, g, b, a);
        }
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("convolution");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);

    // Copiar o resultado de volta para os pixels originais
    // std::copy(output.begin(), output.end(), pixels);
}