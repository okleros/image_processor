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
void img_apply_median_filter(uint32_t* pixels, int w, int h, int kernel_size = 3);
void img_apply_sobel_filter(uint32_t* pixels, int w, int h);
void img_apply_convolution(uint32_t* pixels, int w, int h, Kernel k);
void img_apply_chroma_key(uint32_t* foreground, uint32_t* background, int w, int h, uint32_t key_color, float tolerance);
void img_black_and_white(uint32_t* pixels, int w, int h);
void adjust_brightness(float* i, float brightness_factor);
void adjust_saturation(float* s, float saturation_factor);
void adjust_channels(uint32_t* pixels, int w, int h, float cr_factor, float mg_factor, float yb_factor);
void apply_rotation(uint32_t* pixels, int w, int h, float angle, bool bilinear);
void img_threshold(uint32_t* pixels, int w, int h, uint8_t c);
void img_negative(uint32_t* pixels, int w, int h);
void adjust_image(uint32_t* pixels, int w, int h, float hue_shift, float saturation_factor, float brightness_factor);
char* img_reveal(uint32_t* pixels, int w, int h);
void apply_scale(uint32_t* pixels, int w, int h, float sx, float sy, bool bilinear);
void apply_sepia(uint32_t* image, int w, int h);
void adjust_hue(float* h, float hue_shift);
void img_gamma(uint32_t* pixels, int w, int h, float c, float gamma);
void img_hide(uint32_t* pixels, int w, int h, const char* text);
void img_log(uint32_t* pixels, int w, int h, float c);

// Setup functions
bool create_window_and_renderer(SDL_Window** window, SDL_Renderer** renderer, SDL_DisplayMode display);
bool remake_texture(SDL_Renderer* renderer, SDL_Texture** texture, int iw, int ih);
bool init_SDL(SDL_DisplayMode* displayMode);

// Auxiliary functions
void generate_normalized_histogram(uint32_t* pixels, int w, int h, uint8_t c);
float pixel_RGBA_to_grayscale_lum(uint32_t pixel);
void check_if_image_is_grayscale(uint32_t* pixels, int w, int h);
bool get_pixel_array_from_image(SDL_Renderer* renderer, SDL_Texture** texture, const char* img, uint32_t** pixels, int* w, int* h, SDL_Rect* rect, int iaw, int iah);
void show_color_conversion_tool();
Kernel generate_gaussian_kernel(int size, float sigma);
Kernel generate_average_kernel(int size);
float pixel_RGBA_to_grayscale(uint32_t pixel);
uint32_t convert_RGBA_to_hex(float r, float g, float b, float a);
void create_or_resize_kernel(Kernel& kernel, int newSize);
uint32_t* get_pixel_array(const char* img, int* w, int* h);
void convert_hex_to_RGBA(uint32_t color_hex, float* r, float* g, float* b, float* a);
void show_kernel_table(Kernel& kernel, const char* label, float clamp);
void print_color_prob(uint32_t* pixels, int w, int h);
ImU32 value_to_color(float value, float clamp);
void file_dialog_2(char* buf);
uint32_t getPixel(const uint32_t* image, int width, int x, int y);
float my_distance(float* arr1, float* arr2, size_t dimensions);
void file_dialog(SDL_Renderer* renderer, SDL_Texture** texture, uint32_t** pixels, int* w, int* h, SDL_Rect* destRect, int iaw, int iah);
void IMG_SaveBMP(uint32_t* pixels, const char* filename, int width, int height);
void clear_stack(std::stack<uint32_t*>& stack);
uint32_t hsi2rgb(float h, float s, float i);
void rgb_to_lab(float* rgb, float* l, float* a, float* b);
void space_out(int rep1 = 1, int rep2 = 1);
void img_save(uint32_t* pixels, int width, int height);
void rgb2hsi(uint32_t rgb, float* h, float* s, float* i);

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
float hr[256], hg[256], hb[256], hi[256];
float* histograms[4] = {hr, hg, hb, hi};

// CDFs de cada canal
float CDFr[256], CDFg[256], CDFb[256], CDFi[256];
float* CDF[4] = {CDFr, CDFg, CDFb, CDFi};

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
    pixels = new uint32_t[imgWidth*imgHeight];
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
    ImGui::StyleColorsClassic();

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
    
    Kernel generic_kernel = {3, new float[9]{-1, -1, -1, 0, 0, 0, 1, 1, 1}};

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
            
            space_out(); //------------------------------------------------------------

            // load/save images
            file_dialog(renderer, &mainTexture, &pixels, &imgWidth, &imgHeight, &destRect, imgAreaWidth, imgAreaHeight);
            img_save(pixels, imgWidth, imgHeight);

            space_out(); //------------------------------------------------------------

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

            space_out(); //------------------------------------------------------------

            // ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            // ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            // ImGui::Checkbox("Another Window", &show_another_window);

            // ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            // ImGui::ColorEdit4("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::CollapsingHeader("Intensity Transformations")) {
                // ImGui::Indent();
                ImGui::BeginGroup();

                space_out(); //------------------------------------------------------------
                if (ImGui::Button("B&W")){                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_black_and_white(pixels, imgWidth, imgHeight);
                }
                ImGui::SameLine();
                if (ImGui::Button("B&W (Luminance)"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_black_and_white_lum(pixels, imgWidth, imgHeight);

                ImGui::SameLine();
                if (ImGui::Button("Negative"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_negative(pixels, imgWidth, imgHeight);
                space_out(); //------------------------------------------------------------
                
                static float c_log = 0;
                ImGui::Text("c_log");
                ImGui::DragFloat("##c_log", &c_log, 0.1f, 0.0f, 100.0f);

                ImGui::SameLine();
                if (ImGui::Button("Log"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_log(pixels, imgWidth, imgHeight, c_log);
                space_out(); //------------------------------------------------------------
                
                static int threshold = 0;
                ImGui::Text("threshold");
                ImGui::DragInt("##threshold", &threshold, 1.0f, 0.0f, 255.0f);

                ImGui::SameLine();
                if (ImGui::Button("Threshold"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_threshold(pixels, imgWidth, imgHeight, threshold);
                space_out(); //------------------------------------------------------------

                static float c_gamma = 0;
                ImGui::Text("c_gamma");
                ImGui::DragFloat("##c_gamma", &c_gamma, 0.1f, 0.0f, 100.0f);

                static float gamma = 0;
                ImGui::Text("gamma");
                ImGui::DragFloat("##gamma", &gamma, 0.1f, 0.0f, 100.0f);

                ImGui::SameLine();
                if (ImGui::Button("Correct gamma"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                    img_gamma(pixels, imgWidth, imgHeight, c_gamma, gamma);
                space_out(); //------------------------------------------------------------

                static float hue_shift = 0, saturation_factor = 1, brightness_factor = 1;

                ImGui::SliderFloat("Hue", &hue_shift, 0.0f, 1.0f, "Hue: %.3f");
                ImGui::SliderFloat("Saturation", &saturation_factor, 0.0f, 2.0f, "Saturation: %.3f");
                ImGui::SliderFloat("Brightness", &brightness_factor, 0.0f, 2.0f, "Brightness: %.3f");

                if (ImGui::Button("Adjust image"))
                {
                    adjust_image(pixels, imgWidth, imgHeight, hue_shift, saturation_factor, brightness_factor);
                }
                space_out(); //------------------------------------------------------------
                static float cr_factor = 1, mg_factor = 1, yb_factor = 1;
                
                ImGui::SliderFloat("C/R", &cr_factor, 0.0f, 2.0f, "cr_factor: %.3f");
                ImGui::SliderFloat("M/G", &mg_factor, 0.0f, 2.0f, "mg_factor: %.3f");
                ImGui::SliderFloat("Y/B", &yb_factor, 0.0f, 2.0f, "yb_factor: %.3f");

                if (ImGui::Button("Adjust channels"))
                {
                    adjust_channels(pixels, imgWidth, imgHeight, cr_factor, mg_factor, yb_factor);
                }
                space_out(); //------------------------------------------------------------
                if (ImGui::Button("Apply sepia"))
                {
                    apply_sepia(pixels, imgWidth, imgHeight);
                }
                space_out(); //------------------------------------------------------------
                show_color_conversion_tool();
                space_out(); //------------------------------------------------------------
                ImGui::EndGroup();
            }

            if (ImGui::CollapsingHeader("Convolution"))
            {
                ImGui::Indent(5.0f);
                if (ImGui::CollapsingHeader("Generic convolution"))
                {
                    space_out(); //------------------------------------------------------------
                    static int newSize = generic_kernel.size;
                    static float data_magnitude = 1;
                    // Input field to adjust the kernel size
                    ImGui::InputInt("Kernel Size", &newSize, 2);
                    ImGui::InputFloat("Data magnitude", &data_magnitude, 0.1f, 1.0f);

                    // Resize the kernel if the size has changed
                    if (newSize != generic_kernel.size) {
                        create_or_resize_kernel(generic_kernel, newSize);
                    }

                    show_kernel_table(generic_kernel, "generic_kernel", data_magnitude);

                    if (ImGui::Button("Apply Generic Convolution"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, generic_kernel);
                    }
                    space_out(); //------------------------------------------------------------
                }

                if (ImGui::CollapsingHeader("Gaussian blur"))
                {
                    space_out(); //------------------------------------------------------------
                    static int kernel_size = 3;
                    ImGui::InputInt("Kernel size", &kernel_size, 2);
                    
                    static float std_dev = 1.0f;
                    ImGui::InputFloat("std dev", &std_dev, 0.5f, 2.0f);
                    Kernel gk = generate_gaussian_kernel(kernel_size, std_dev);

                    static bool show_gaussian_kernel = false;
                    static float g_data_magnitude = 0.2f;
                    if (show_gaussian_kernel) ImGui::InputFloat("GData mag", &g_data_magnitude, 0.05f, 1.0f);
                    if (ImGui::Button("Apply Gaussian Blur"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, gk);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("View Kernel", &show_gaussian_kernel);
                    if (show_gaussian_kernel)
                    {
                        show_kernel_table(gk, "gaussian_kernel", g_data_magnitude);
                    }
                    space_out(); //------------------------------------------------------------
                }

                if (ImGui::CollapsingHeader("Average blur"))
                {
                    space_out(); //------------------------------------------------------------
                    static int avg_kernel_size = 3;
                    ImGui::InputInt("Kernel size", &avg_kernel_size, 2);
                    Kernel ak = generate_average_kernel(avg_kernel_size);

                    static bool show_avg_kernel = false;
                    static float avg_data_magnitude = 1/(avg_kernel_size*avg_kernel_size);
                    if (ImGui::Button("Apply Average Blur"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, ak);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("View Kernel", &show_avg_kernel);
                    if (show_avg_kernel)
                    {
                        show_kernel_table(ak, "avg_kernel", avg_data_magnitude);
                    }
                    space_out(); //------------------------------------------------------------
                }

                if (ImGui::CollapsingHeader("Sharpness (Laplacian)"))
                {
                    space_out(); //------------------------------------------------------------
                    static bool show_laplace_kernel = false;
                    static float laplace_data_magnitude = 3.0f;

                    float lk_elements[] = { 0, -1,  0,
                                           -1,  5, -1, 
                                            0, -1,  0};
                    Kernel lk = {3, lk_elements};

                    if (ImGui::Button("Apply Laplacian Filter"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, lk);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("View Kernel", &show_laplace_kernel);
                    if (show_laplace_kernel)
                    {
                        show_kernel_table(lk, "laplace_kernel", laplace_data_magnitude);
                    }
                    space_out(); //------------------------------------------------------------
                }
                
                if (ImGui::CollapsingHeader("Sharpness (High Boost)")) {
                    space_out(); //------------------------------------------------------------
                    static bool show_hb_kernel = false;
                    static float hb_data_magnitude = 3.0f;

                    float hbk_elements[] = {-1, -1, -1,
                                            -1,  9, -1, 
                                            -1, -1, -1};
                    Kernel hbk = {3, hbk_elements};
                
                    if (ImGui::Button("Apply High Boost Filter"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, hbk);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("View Kernel", &show_hb_kernel);
                    if (show_hb_kernel)
                    {
                        show_kernel_table(hbk, "hb_kernel", hb_data_magnitude);
                    }
                    space_out(); //------------------------------------------------------------
                }
                
                if (ImGui::CollapsingHeader("Sobel Operator")) {
                    space_out(); //------------------------------------------------------------
                    static bool show_sx_kernel = false;
                    static float sx_data_magnitude = 3.0f;

                    float sxk_elements[] = {-1, 0, 1,
                                            -2, 0, 2, 
                                            -1, 0, 1};
                    Kernel sxk = {3, sxk_elements};
                    
                    if (ImGui::Button("Apply SobelX Filter"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, sxk);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("View X Kernel", &show_sx_kernel);
                    if (show_sx_kernel)
                    {
                        show_kernel_table(sxk, "sx_kernel", sx_data_magnitude);
                    }

                    space_out(); //------------------------------------------------------------
                    static bool show_sy_kernel = false;
                    static float sy_data_magnitude = 3.0f;
                    
                    float syk_elements[] = {-1, -2, -1,
                                             0,  0,  0, 
                                             1,  2,  1};
                    Kernel syk = {3, syk_elements};

                    if (ImGui::Button("Apply SobelY Filter"))
                    {
                        img_apply_convolution(pixels, imgWidth, imgHeight, syk);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("View Y Kernel", &show_sy_kernel);
                    if (show_sy_kernel)
                    {
                        show_kernel_table(syk, "sy_kernel", sy_data_magnitude);
                    }

                    space_out(); //------------------------------------------------------------
                    if (ImGui::Button("Apply Sobel Operator"))
                    {
                        img_apply_sobel_filter(pixels, imgWidth, imgHeight);
                    }
                    space_out(); //------------------------------------------------------------
                }
                ImGui::Unindent(5.0f);
            }

            if (ImGui::CollapsingHeader("Spacial transformations"))
            {
                static bool bilinear = false;

                static float angle=45, s[2] = {2, 2};

                ImGui::Checkbox("Bilinear", &bilinear);

                ImGui::InputFloat2("Scale X and Y", s);
                if (ImGui::Button("Apply scale"))
                {
                    apply_scale(pixels, imgWidth, imgHeight, s[0], s[1], bilinear);
                }
                ImGui::InputFloat("Angle of rotation", &angle);
                if (ImGui::Button("Apply rotation"))
                {
                    apply_rotation(pixels, imgWidth, imgHeight, angle, bilinear);
                }
            }

            if (ImGui::CollapsingHeader("Filter by Median")) {
                space_out(); //------------------------------------------------------------
                static int median_kernel_size = 3;
                ImGui::InputInt("Kernel size", &median_kernel_size, 2);
                
                if (ImGui::Button("Apply Median Filter"))
                {
                    img_apply_median_filter(pixels, imgWidth, imgHeight, median_kernel_size);
                }
                space_out(); //------------------------------------------------------------
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

            if (ImGui::CollapsingHeader("Chroma key"))
            {
                static float tolerance = 30;

                int sub_img_w, sub_img_h;

                static char x[256] = "./../res/cloudy_sky.jpg";
                file_dialog_2(x);
                
                ImGui::SameLine();
                ImGui::Text("%s\n", x);
                
                static float color[3]{0.0f, 1.0f, 0.0f};
                ImGui::ColorEdit3("##MyColor", color);
                
                ImGui::SliderFloat("##tolerance", &tolerance, 0.0f, 100.0f);

                ImGui::SameLine();
                if (ImGui::Button("Apply"))
                {
                    uint32_t* sub_img = get_pixel_array(x, &sub_img_w, &sub_img_h);
                    
                    if (imgWidth == sub_img_w && imgHeight == sub_img_h)
                    {
                        img_apply_chroma_key(pixels, sub_img,
                            imgWidth, imgHeight, convert_RGBA_to_hex(color[0], color[1], color[2], 1), tolerance);
                    } else {
                        std::cout << "[ERROR]: dimension mismatch ("<< imgWidth << "x" << imgHeight << ") vs (" << sub_img_w << "x" << sub_img_h << ")." << std::endl;
                    }
                
                    delete[] sub_img;
                }

            }

            if (ImGui::CollapsingHeader("Histogram Equalization")) {
                if (ImGui::Button("Equalize Full Image"))
                {
                    img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 3);
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

                    if (ImGui::Button("Equalize I"))
                    {
                        img_generate_equalized_histogram(pixels, imgWidth, imgHeight, 3);
                    }
                    ImGui::PlotHistogram("##HistogramI", hi, IM_ARRAYSIZE(hi), 0, NULL, 0.0f, maxmax, ImVec2(0,160));
                } else {
                    ImGui::PlotHistogram("##HistogramI", hi, IM_ARRAYSIZE(hi), 0, NULL, 0.0f, maxmax, ImVec2(0,160));
                }
                
                ImGui::EndGroup();
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Data compression tests"))
            {
                space_out();
                // RGB color space
                static float color1[3]{0.0f, 1.0f, 0.0f};
                static float color2[3]{0.0f, 1.0f, 0.0f};

                // LAB color space
                static float l1, a1, b1, l2, a2, b2;

                rgb_to_lab(color1, &l1, &a1, &b1);
                rgb_to_lab(color2, &l2, &a2, &b2);

                float lab1[3] = {l1, a1, b1};
                float lab2[3] = {l2, a2, b2};

                ImGui::ColorEdit3("##MyColor1", color1);
                ImGui::Text("LAB color1: [%.3f, %.3f, %.3f]", l1, a1, b1);
                ImGui::ColorEdit3("##MyColor2", color2);
                ImGui::Text("LAB color2: [%.3f, %.3f, %.3f]", l2, a2, b2);

                ImGui::Text("\nEuclidean difference RGB: %.3f", my_distance(color1, color2, 3));
                ImGui::Text("Euclidean difference LAB: %.3f", my_distance(lab1, lab2, 3));
                space_out();

                if (ImGui::Button("Show color amount"))
                {
                    print_color_prob(pixels, imgWidth, imgHeight);
                }

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
            ImGui::Text("Image dimensions: [%dx%d]", imgWidth, imgHeight);
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

uint32_t getPixel(const uint32_t* image, int width, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= width) return 0; // Fora dos limites
    return image[y * width + x];
}

void clear_stack(std::stack<uint32_t*>& stack)
{
    while (!stack.empty()) {
        free(stack.top());
        stack.pop();
    }
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

    uint32_t* old_pixels = new uint32_t[w*h];

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
    generate_normalized_histogram(pixels, w, h, 2);
    generate_normalized_histogram(pixels, w, h, 3);
}

void img_threshold(uint32_t* pixels, int w, int h, uint8_t threshold)
{
    if (0)
    {
        printf("Image is not in grayscale!\n");
    
    } else {
        float intensity, lixo;

        uint32_t* old_pixels = new uint32_t[w*h];

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

    uint32_t* old_pixels = new uint32_t[w*h];

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
    generate_normalized_histogram(pixels, w, h, 3);
}

void img_black_and_white(uint32_t* pixels, int w, int h)
{
    float r, g, b, a;
    int media;

    uint32_t* old_pixels = new uint32_t[w*h];
    
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

    uint32_t* old_pixels = new uint32_t[w*h];

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

void file_dialog_2(char* buf)
{
    if (ImGui::Button("Open File##1")) {
        // Abre o diálogo de arquivo
        ImGui::OpenPopup("Select a file##1");
    }

    // Diálogo de arquivo
    if (ImGui::BeginPopupModal("Select a file##1", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Itera pelos arquivos e diretórios do diretório atual e subdiretórios
        if (ImGui::Selectable(".")) goto exit_popup;
        for (const auto& entry : fs::/*recursive_*/directory_iterator("./../res")) {
            if (!fs::is_directory(entry.path()) && ImGui::Selectable(entry.path().string().c_str()+9)) {
                // Se um arquivo ou diretório é selecionado, armazena o caminho absoluto

                strcpy(buf, entry.path().c_str());

                ImGui::CloseCurrentPopup(); // Fecha o diálogo
                goto exit_popup;
            }
        }
    exit_popup:
        ImGui::EndPopup();
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
    SDL_DestroyTexture(*texture);

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

uint32_t* get_pixel_array(const char* img, int* w, int* h)
{
    SDL_Surface* imageSurface = SDL_ConvertSurfaceFormat(IMG_Load(img), SDL_PIXELFORMAT_RGBA8888, 0);
    if (imageSurface == NULL) {
        printf("Unable to load image! SDL_image Error: %s\n", IMG_GetError());

        return 0;
    }

    // Get image dimensions
    *w = imageSurface->w;
    *h = imageSurface->h;

    uint32_t* pixels = new uint32_t[(*w) * (*h)];

    // Extract pixel data from image surface
    SDL_LockSurface(imageSurface);
    memcpy(pixels, imageSurface->pixels, (*w) * (*h) * sizeof(uint32_t));
    SDL_UnlockSurface(imageSurface);

    SDL_FreeSurface(imageSurface);

    return pixels;
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

    *pixels = new uint32_t[(*w) * (*h)];

    // Extract pixel data from image surface
    SDL_LockSurface(imageSurface);
    memcpy(*pixels, imageSurface->pixels, (*w) * (*h) * sizeof(uint32_t));
    SDL_UnlockSurface(imageSurface);

    remake_texture(renderer, texture, *w, *h);
    
    generate_normalized_histogram(*pixels, *w, *h, 0);
    generate_normalized_histogram(*pixels, *w, *h, 1);
    generate_normalized_histogram(*pixels, *w, *h, 2);
    generate_normalized_histogram(*pixels, *w, *h, 3);

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

    uint32_t* old_pixels = new uint32_t[w*h];

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
    generate_normalized_histogram(pixels, w, h, 3);
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
    char* revealed_text = new char[MAX_STEG_TEXT];

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

void rgb2hsv(uint32_t hex_color, float* h, float* s, float* v) {
    float r, g, b, a;
    convert_hex_to_RGBA(hex_color, &r, &g, &b, &a);

    float max_val = std::max({r, g, b});
    float min_val = std::min({r, g, b});
    float delta = max_val - min_val;

    // Valor de brilho (V)
    *v = max_val;

    // Saturação (S)
    if (max_val != 0.0f) {
        *s = delta / max_val;
    } else {
        *s = 0.0f;
    }

    // Matiz (H)
    if (*s == 0.0f) {
        *h = 0.0f; // Se a saturação for 0, o matiz não é definido
    } else {
        if (r == max_val) {
            *h = (g - b) / delta;
        } else if (g == max_val) {
            *h = 2.0f + (b - r) / delta;
        } else {
            *h = 4.0f + (r - g) / delta;
        }
        *h *= 60.0f;
        if (*h < 0.0f) {
            *h += 360.0f;
        }
        // *h /= 360.0f; // Normaliza o valor de H para [0.0, 1.0]
    }
}

// Função para converter HSV para RGB
uint32_t hsv2rgb(float h, float s, float v) {
    float r, g, b;

    int i = static_cast<int>(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }

    // Converter de volta para uint32_t
    uint32_t R = static_cast<uint32_t>(std::round(r * 255.0f));
    uint32_t G = static_cast<uint32_t>(std::round(g * 255.0f));
    uint32_t B = static_cast<uint32_t>(std::round(b * 255.0f));
    uint32_t A = 255; // Opacidade total

    return (R << 24) | (G << 16) | (B << 8) | A;
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
    float r, g, b, a, intensity;
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
        intensity = pixel_RGBA_to_grayscale(pixels[i]);

        float colors[4] = {r, g, b, intensity};

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
    maxmax = std::fmax(std::fmax(array_max(hr), std::fmax(array_max(hg), array_max(hb))), array_max(hi));
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

void rgb2hsi(uint32_t rgba, float* h, float* s, float* i) {
    float r, g, b, a;
    convert_hex_to_RGBA(rgba, &r, &g, &b, &a);
    
    // Calculate Intensity
    *i = (r + g + b) / 3.0f;
    
    // Calculate Saturation
    float min_rgb = std::min(r, std::min(g, b));
    *s = 1.0f - (3.0f * min_rgb / (r + g + b + 1e-6f)); // Adding a small value to avoid division by zero
    
    // Calculate Hue
    float numerator = 0.5f * ((r - g) + (r - b));
    float denominator = std::sqrt((r - g) * (r - g) + (r - b) * (g - b));
    float theta = std::acos(numerator / (denominator + 1e-6f)); // Adding a small value to avoid division by zero
    
    if (b <= g) {
        *h = theta;
    } else {
        *h = 2.0f * M_PI - theta;
    }
    
    // Normalize Hue to [0, 1]
    *h /= (2.0f * M_PI);
}


uint32_t hsi2rgb(float h, float s, float i) {
    // Convert Hue from [0, 1] to [0, 2π]
    float hue = h * 2.0f * M_PI;
    float r, g, b;
    
    if (hue < 2.0f * M_PI / 3.0f) {
        b = i * (1.0f - s);
        r = i * (1.0f + s * std::cos(hue) / std::cos(M_PI / 3.0f - hue));
        g = 3.0f * i - (r + b);
    } else if (hue < 4.0f * M_PI / 3.0f) {
        hue -= 2.0f * M_PI / 3.0f;
        r = i * (1.0f - s);
        g = i * (1.0f + s * std::cos(hue) / std::cos(M_PI / 3.0f - hue));
        b = 3.0f * i - (r + g);
    } else {
        hue -= 4.0f * M_PI / 3.0f;
        g = i * (1.0f - s);
        b = i * (1.0f + s * std::cos(hue) / std::cos(M_PI / 3.0f - hue));
        r = 3.0f * i - (g + b);
    }
    
    // Clamp values to [0, 1]
    r = std::fmax(0.0f, std::fmin(1.0f, r));
    g = std::fmax(0.0f, std::fmin(1.0f, g));
    b = std::fmax(0.0f, std::fmin(1.0f, b));
    
    // Convert RGB values to the range [0, 255]
    return convert_RGBA_to_hex(r, g, b, 1.0f); // Alpha set to 1.0f (fully opaque)
}

void img_generate_equalized_histogram(uint32_t* pixels, int w, int h, uint8_t c)
{
    uint32_t* old_pixels = new uint32_t[w*h];
    memcpy(old_pixels, pixels, w*h*sizeof(uint32_t));

    // primeira vez para fazer os cálculos
    generate_normalized_histogram(pixels, w, h, c);
    generate_CDF(c);

    int tot_pixels = w * h;
    float r, g, b, a;
    float hh, s, intensity;
    float* current_cdf = CDF[c];

    for (int i = 0; i < tot_pixels; ++i)
    {
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);
        rgb2hsi(pixels[i], &hh, &s, &intensity);

        float colors[4] = { r, g, b, intensity };

        uint8_t old_intensity = (uint8_t)(255.0f * colors[c]);
        uint8_t new_intensity = (uint8_t)(current_cdf[old_intensity] * 255.0f);

        colors[c] = new_intensity / 255.0f;

        // Reconstituir o pixel com os novos valores equalizados
        if (c < 3)
            pixels[i] = convert_RGBA_to_hex(colors[0], colors[1], colors[2], a);
        else {
            pixels[i] = hsi2rgb(hh, s, colors[c]);
        }

    }

    undo_stack.push(old_pixels);

    if (c == 0)
        undo_log_stack.push("Equalize RED Histogram");

    if (c == 1)
        undo_log_stack.push("Equalize GREEN Histogram");

    if (c == 2)
        undo_log_stack.push("Equalize BLUE Histogram");

    if (c == 3)
        undo_log_stack.push("Equalize INTENSITY Histogram");

    // gerando o histograma normalizado de novo para mostrar na GUI
    generate_normalized_histogram(pixels, w, h, c);
}

Kernel generate_gaussian_kernel(int size, float sigma) {
    Kernel kernel;
    if (size % 2 == 0) {
        printf("Kernel size should be an odd number");
        return kernel;
    }

    kernel.size = size;
    kernel.elements = new float[size * size];
    
    float sum = 0.0f;
    int half_size = size / 2;
    float sigma_squared = sigma * sigma;
    float two_sigma_squared = 2.0f * sigma_squared;
    
    for (int y = -half_size; y <= half_size; ++y) {
        for (int x = -half_size; x <= half_size; ++x) {
            float exponent = -(x * x + y * y) / two_sigma_squared;
            float value = (1.0f / (2.0f * M_PI * sigma_squared)) * exp(exponent);
            int index = (y + half_size) * size + (x + half_size);
            kernel.elements[index] = value;
            sum += value;
        }
    }

    // Normalizar a matriz para que a soma dos valores seja 1
    for (int i = 0; i < size * size; ++i) {
        kernel.elements[i] /= sum;
    }

    return kernel;
}

Kernel generate_average_kernel(int size) {
    Kernel kernel;
    if (size % 2 == 0) {
        printf("Kernel size should be an odd number");
        return kernel;
    }

    kernel.size = size;
    kernel.elements = new float[size * size];
    
    for (int i = 0; i < size*size; ++i)
        kernel.elements[i] = 1.0f/(size*size);

    return kernel;
}

// Function to create or resize the kernel
void create_or_resize_kernel(Kernel& kernel, int newSize) {
    if (newSize % 2 == 0) {
        newSize += 1; // Ensure the size is odd
    }
    if (kernel.size != newSize) {
        kernel.size = newSize;
        delete[] kernel.elements;
        kernel.elements = new float[newSize * newSize];
        std::fill(kernel.elements, kernel.elements + newSize * newSize, 0.0f);
    }
}

// Function to display and edit the kernel in an ImGui table
void show_kernel_table(Kernel& kernel, const char* label, float clamp) {
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    // Push style var to change cell padding
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0.0f)); // Set both x and y padding to 0
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 12.0f)); // Set frame padding to 0 for tighter fit

    // Create a table with the given size
    if (ImGui::BeginTable(label, kernel.size, flags)) {
        for (int row = 0; row < kernel.size; ++row) {
            ImGui::TableNextRow();
            for (int col = 0; col < kernel.size; ++col) {
                ImGui::TableSetColumnIndex(col);

                // Get the index in the 1D kernel array
                int index = row * kernel.size + col;

                ImU32 color = value_to_color(kernel.elements[index], clamp);
                // Aplicar a cor de fundo
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color);
                
                // Center align the text
                ImGui::PushItemWidth(ImGui::GetFontSize() * 2.76f);
                // ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::CalcTextSize("0.000").x) * 0.5f);
                ImGui::DragFloat(("##element" + std::to_string(index)).c_str(), &kernel.elements[index], 0.1f, -FLT_MAX, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopItemWidth();
            }
        }
        ImGui::EndTable();
    }

    // Pop style var to restore previous cell padding
    ImGui::PopStyleVar(2); // Pop both CellPadding and FramePadding
}

// Função para aplicar convolução em uma imagem RGBA
void img_apply_convolution(uint32_t* pixels, int w, int h, Kernel k) {
    int halfSize = k.size / 2;

    uint32_t* old_pixels = new uint32_t[w*h];

    if (old_pixels == nullptr) {
        // Lidar com erro de alocação de memória
        return;
    }

    memcpy(old_pixels, pixels, w * h * sizeof(uint32_t));

    // Iterar sobre cada pixel da imagem
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
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
    generate_normalized_histogram(pixels, w, h, 3);

    // Copiar o resultado de volta para os pixels originais
    // std::copy(output.begin(), output.end(), pixels);
}

void img_apply_sobel_filter(uint32_t* pixels, int w, int h) {
    // Sobel kernels
    float sobel_x[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    float sobel_y[3][3] = {
        {-1, -2, -1},
        {0, 0, 0},
        {1, 2, 1}
    };

    // Create a copy of the original image to read from
    uint32_t* old_pixels = new uint32_t[w*h];
    memcpy(old_pixels, pixels, w * h * sizeof(uint32_t));

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float gx = 0.0f;
            float gy = 0.0f;

            // Apply Sobel kernels
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    int ix = std::clamp(x + kx, 0, w - 1);
                    int iy = std::clamp(y + ky, 0, h - 1);
                    uint32_t pixel = old_pixels[iy * w + ix];
                    float grayscale = pixel_RGBA_to_grayscale_lum(pixel);
                    gx += grayscale * sobel_x[ky + 1][kx + 1];
                    gy += grayscale * sobel_y[ky + 1][kx + 1];
                }
            }

            // Calculate gradient magnitude
            float magnitude = std::sqrt(gx * gx + gy * gy);

            // Normalize to range [0, 1]
            magnitude = std::min(1.0f, magnitude);

            // Convert back to RGBA
            uint32_t rgba = convert_RGBA_to_hex(magnitude, magnitude, magnitude, magnitude);
            pixels[y * w + x] = rgba;
        }
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("sobel operator");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
    generate_normalized_histogram(pixels, w, h, 3);
}

float pixel_RGBA_to_grayscale_lum(uint32_t pixel)
{
    float r, g, b, a;

    convert_hex_to_RGBA(pixel, &r, &g, &b, &a);
        
    float media = r * 0.3 + g * 0.59 + b * 0.11;

    return media;
}

float pixel_RGBA_to_grayscale(uint32_t pixel)
{
    float r, g, b, a;

    convert_hex_to_RGBA(pixel, &r, &g, &b, &a);
        
    float media = (r + g + b) / 3;

    return media;
}

// Quickselect algorithm to find the k-th smallest element
template<typename T>
T quickselect(std::vector<T>& arr, int k) {
    if (arr.empty()) throw std::invalid_argument("Array is empty");

    int left = 0;
    int right = arr.size() - 1;

    while (left < right) {
        int pivotIndex = left + (right - left) / 2;
        T pivotValue = arr[pivotIndex];
        std::swap(arr[pivotIndex], arr[right]);

        int storeIndex = left;
        for (int i = left; i < right; ++i) {
            if (arr[i] < pivotValue) {
                std::swap(arr[i], arr[storeIndex]);
                ++storeIndex;
            }
        }
        std::swap(arr[storeIndex], arr[right]);

        if (k == storeIndex) {
            return arr[k];
        } else if (k < storeIndex) {
            right = storeIndex - 1;
        } else {
            left = storeIndex + 1;
        }
    }

    return arr[left];
}

void img_apply_median_filter(uint32_t* pixels, int w, int h, int kernel_size) {
    uint32_t* old_pixels = new uint32_t[w * h];
    memcpy(old_pixels, pixels, w * h * sizeof(uint32_t));

    int half_k = kernel_size / 2;

    float r, g, b, a;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::vector<float> r_neigh, g_neigh, b_neigh;

            // Collect neighborhood pixels
            for (int ky = -half_k; ky <= half_k; ++ky) {
                for (int kx = -half_k; kx <= half_k; ++kx) {
                    int ix = std::clamp(x + kx, 0, w - 1);
                    int iy = std::clamp(y + ky, 0, h - 1);
                    uint32_t pixel = old_pixels[iy * w + ix];
                    
                    convert_hex_to_RGBA(pixel, &r, &g, &b, &a);

                    r_neigh.push_back(r);
                    if (!is_img_grayscale)
                    {
                        g_neigh.push_back(g);
                        b_neigh.push_back(b);
                    }
                }
            }

            // Sort and find the median for each channel
            auto median = [](std::vector<float>& v) {
                return quickselect(v, v.size() / 2);
            };

            float r_median = median(r_neigh);
            if (!is_img_grayscale)
            {
                float g_median = median(g_neigh);
                float b_median = median(b_neigh);
                
                pixels[y * w + x] = convert_RGBA_to_hex(r_median, g_median, b_median, 1); // Alpha channel set to 255
            } else {
                pixels[y * w + x] = convert_RGBA_to_hex(r_median, r_median, r_median, 1); // Alpha channel set to 255
            }
        }
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("median filter");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
    generate_normalized_histogram(pixels, w, h, 3);
}

ImU32 value_to_color(float value, float clamp) {
    float normalized_value;
    ImVec4 color;

    if (value == 0.0f) {
        // Preto para zero
        color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Preto
    } else if (value > 0) {
        // Valor positivo (verde a preto)
        normalized_value = std::clamp(value / clamp, 0.0f, 1.0f); // Normaliza entre 0 e 1

        // Interpolação entre verde e preto
        color = ImVec4(0.0f, normalized_value, 0.0f, 1.0f); // Verde para preto
    } else {
        // Valor negativo (preto a vermelho)
        normalized_value = std::clamp(-value / clamp, 0.0f, 1.0f); // Normaliza entre 0 e 1

        // Interpolação entre preto e vermelho
        color = ImVec4(normalized_value, 0.0f, 0.0f, 1.0f); // Preto para vermelho
    }

    // Converter a cor de RGBA para hexadecimal
    // if (value < 0)
    // {
    //     std::cout << std::hex << convert_RGBA_to_hex(color.x, color.y, color.z, color.w) << std::endl;
    // }

    ImU32 cell_bg_color = ImGui::GetColorU32(color);
    return cell_bg_color;
}

// Function to apply chroma key effect in HSV color space
void img_apply_chroma_key(uint32_t* foreground, uint32_t* background, int w, int h, uint32_t key_color, float tolerance) {
    uint32_t* old_pixels = new uint32_t[w * h];
    memcpy(old_pixels, foreground, w * h * sizeof(uint32_t));

    float key_h, key_s, key_v;
    rgb2hsv(key_color, &key_h, &key_s, &key_v);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint32_t fg_pixel = foreground[y * w + x];
            uint32_t bg_pixel = background[y * w + x];

            float fg_h, fg_s, fg_v;
            rgb2hsv(fg_pixel, &fg_h, &fg_s, &fg_v);

            float hue_distance = std::fabs(fg_h - key_h);
            if (hue_distance > 180.0f) hue_distance = 360.0f - hue_distance;

            if (hue_distance < tolerance && fg_s > 0.1f && fg_v > 0.1f) {
                foreground[y * w + x] = bg_pixel;
            }
        }
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("chroma key");

    generate_normalized_histogram(foreground, w, h, 0);
    generate_normalized_histogram(foreground, w, h, 1);
    generate_normalized_histogram(foreground, w, h, 2);
    generate_normalized_histogram(foreground, w, h, 3);
}

void apply_sepia(uint32_t* pixels, int w, int h) {
    int num_pixels = w*h;

    uint32_t* old_pixels = new uint32_t[num_pixels];
    memcpy(old_pixels, pixels, num_pixels * sizeof(uint32_t));

    for (int i = 0; i < num_pixels; ++i) {
        float r, g, b, a;
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        // Aplicar a transformação sépia
        float tr = 0.393f * r + 0.769f * g + 0.189f * b;
        float tg = 0.349f * r + 0.686f * g + 0.168f * b;
        float tb = 0.272f * r + 0.534f * g + 0.131f * b;

        // Clamping para manter os valores dentro do intervalo [0, 1]
        r = std::fmin(1.0f, tr);
        g = std::fmin(1.0f, tg);
        b = std::fmin(1.0f, tb);

        // Converter de volta para o formato uint32_t e atualizar o pixel
        pixels[i] = convert_RGBA_to_hex(r, g, b, a);
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("sepia");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
    generate_normalized_histogram(pixels, w, h, 3);
}

void adjust_image(uint32_t* pixels, int w, int h, float hue_shift, float saturation_factor, float brightness_factor) {
    int num_pixels = w*h;

    uint32_t* old_pixels = new uint32_t[num_pixels];
    memcpy(old_pixels, pixels, num_pixels * sizeof(uint32_t));

    for (int i = 0; i < num_pixels; ++i) {
        // Converter RGB para HSI
        float hh, ss, ii;
        rgb2hsi(pixels[i], &hh, &ss, &ii);

        // Ajustar HSI
        adjust_hue(&hh, hue_shift);
        adjust_saturation(&ss, saturation_factor);
        adjust_brightness(&ii, brightness_factor);

        // Converter HSI de volta para RGB e atualizar o pixel
        pixels[i] = hsi2rgb(hh, ss, ii);
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("HSI adjustment");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
    generate_normalized_histogram(pixels, w, h, 3);
}

void adjust_hue(float* h, float hue_shift) {
    *h += hue_shift;
    if (*h > 1.0f) {
        *h -= 1.0f;
    } else if (*h < 0.0f) {
        *h += 1.0f;
    }
}

void adjust_saturation(float* s, float saturation_factor) {
    *s *= saturation_factor;
    if (*s > 1.0f) {
        *s = 1.0f;
    } else if (*s < 0.0f) {
        *s = 0.0f;
    }
}

void adjust_brightness(float* i, float brightness_factor) {
    *i *= brightness_factor;
    if (*i > 1.0f) {
        *i = 1.0f;
    } else if (*i < 0.0f) {
        *i = 0.0f;
    }
}

void adjust_channels(uint32_t* pixels, int w, int h, float cr_factor, float mg_factor, float yb_factor) {
    int num_pixels = w * h;

    uint32_t* old_pixels = new uint32_t[num_pixels];
    memcpy(old_pixels, pixels, num_pixels * sizeof(uint32_t));

    for (int i = 0; i < num_pixels; ++i) {
        float r, g, b, a;
        convert_hex_to_RGBA(pixels[i], &r, &g, &b, &a);

        // Ajustes de canal
        r *= cr_factor; // Ajuste de Vermelho
        g *= mg_factor; // Ajuste de Verde
        b *= yb_factor; // Ajuste de Azul

        // Clamping para manter os valores dentro do intervalo [0, 1]
        r = std::fmax(0.0f, std::fmin(1.0f, r));
        g = std::fmax(0.0f, std::fmin(1.0f, g));
        b = std::fmax(0.0f, std::fmin(1.0f, b));

        // Converter de volta para o formato uint32_t e atualizar o pixel
        pixels[i] = convert_RGBA_to_hex(r, g, b, a);
    }

    undo_stack.push(old_pixels);
    undo_log_stack.push("channel adjustment");

    generate_normalized_histogram(pixels, w, h, 0);
    generate_normalized_histogram(pixels, w, h, 1);
    generate_normalized_histogram(pixels, w, h, 2);
    generate_normalized_histogram(pixels, w, h, 3);
}

void show_color_conversion_tool()
{
    // Valores iniciais para os sliders
    static float h = 0.0f;
    static float s = 1.0f;
    static float v = 1.0f;
    static uint32_t rgbColor = hsv2rgb(h, s, v);

    // Mostrar sliders para HSV
    ImGui::Text("HSV to RGB Conversion");
    ImGui::SliderFloat("Hue1", &h, 0.0f, 360.0f, "%.1f");
    ImGui::SliderFloat("Saturation1", &s, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Value1", &v, 0.0f, 1.0f, "%.2f");

    // Atualizar a cor RGB com base na entrada HSV
    rgbColor = hsv2rgb(h, s, v);
    ImVec4 rgbColorVec;
    convert_hex_to_RGBA(rgbColor, &rgbColorVec.x, &rgbColorVec.y, &rgbColorVec.z, &rgbColorVec.w);  // Mostrar a cor RGB resultante
    ImGui::ColorEdit3("RGB Color", (float*)&rgbColorVec);

    // Mostrar sliders para RGB
    static float r = rgbColorVec.x;
    static float g = rgbColorVec.y;
    static float b = rgbColorVec.z;

    ImGui::Text("RGB to HSV Conversion");
    ImGui::SliderFloat("Red", &r, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Green", &g, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Blue", &b, 0.0f, 1.0f, "%.2f");

    // Atualizar a cor HSV com base na entrada RGB
    uint32_t rgbColorInput = convert_RGBA_to_hex(r, g, b, 1.0f);
    rgb2hsv(rgbColorInput, &h, &s, &v);

    // Mostrar os valores HSV atualizados
    ImGui::Text("HSV Values: H = %.1f, S = %.2f, V = %.2f", h, s, v);
}

void apply_scale(uint32_t* pixels, int w, int h, float sx, float sy, bool bilinear)
{
    // Criação de uma cópia da imagem original para o histórico de desfazer
    uint32_t* old_pixels = new uint32_t[w * h];
    std::memcpy(old_pixels, pixels, w * h * sizeof(uint32_t));

    // Calcular o centro da imagem
    float centerX = w / 2.0f;
    float centerY = h / 2.0f;

    // Aplicar a escala diretamente na imagem de entrada
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Mapear as coordenadas da imagem escalada para a imagem original
            float srcX = (x - centerX) / sx + centerX;
            float srcY = (y - centerY) / sy + centerY;

            int x0 = static_cast<int>(std::floor(srcX));
            int y0 = static_cast<int>(std::floor(srcY));
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float dx = srcX - x0;
            float dy = srcY - y0;

            if (bilinear) {
                // Interpolação bilinear usando a cópia original
                float r00, g00, b00, a00;
                float r01, g01, b01, a01;
                float r10, g10, b10, a10;
                float r11, g11, b11, a11;

                uint32_t color00 = getPixel(old_pixels, w, x0, y0);
                uint32_t color01 = getPixel(old_pixels, w, x1, y0);
                uint32_t color10 = getPixel(old_pixels, w, x0, y1);
                uint32_t color11 = getPixel(old_pixels, w, x1, y1);

                convert_hex_to_RGBA(color00, &r00, &g00, &b00, &a00);
                convert_hex_to_RGBA(color01, &r01, &g01, &b01, &a01);
                convert_hex_to_RGBA(color10, &r10, &g10, &b10, &a10);
                convert_hex_to_RGBA(color11, &r11, &g11, &b11, &a11);

                float r = (1 - dx) * (1 - dy) * r00 + dx * (1 - dy) * r01 + (1 - dx) * dy * r10 + dx * dy * r11;
                float g = (1 - dx) * (1 - dy) * g00 + dx * (1 - dy) * g01 + (1 - dx) * dy * g10 + dx * dy * g11;
                float b = (1 - dx) * (1 - dy) * b00 + dx * (1 - dy) * b01 + (1 - dx) * dy * b10 + dx * dy * b11;
                float a = (1 - dx) * (1 - dy) * a00 + dx * (1 - dy) * a01 + (1 - dx) * dy * a10 + dx * dy * a11;

                pixels[y * w + x] = convert_RGBA_to_hex(r, g, b, a);
            } else {
                // Interpolação nearest neighbor usando a cópia original
                int nearestX = static_cast<int>(std::round(srcX));
                int nearestY = static_cast<int>(std::round(srcY));

                // Garantir que os índices estejam dentro dos limites
                nearestX = nearestX;
                nearestY = nearestY;

                pixels[y * w + x] = getPixel(old_pixels, w, nearestX, nearestY);
            }
        }
    }

    // Adicionar a imagem original ao histórico de desfazer
    undo_stack.push(old_pixels);
    undo_log_stack.push("scaling");
}

void apply_rotation(uint32_t* pixels, int w, int h, float angle, bool bilinear)
{
    // Criação de uma cópia da imagem original para o histórico de desfazer
    uint32_t* old_pixels = new uint32_t[w * h];
    std::memcpy(old_pixels, pixels, w * h * sizeof(uint32_t));

    // Calcular o centro da imagem
    float centerX = w / 2.0f;
    float centerY = h / 2.0f;

    // Converter o ângulo para radianos
    float radians = angle * (3.141592653589793f / 180.0f);
    float cos_angle = std::cos(-radians);
    float sin_angle = std::sin(-radians);

    // Aplicar a rotação diretamente na imagem de entrada
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Mapear as coordenadas da imagem original para a imagem rotacionada
            float srcX = cos_angle * (x - centerX) - sin_angle * (y - centerY) + centerX;
            float srcY = sin_angle * (x - centerX) + cos_angle * (y - centerY) + centerY;

            int x0 = static_cast<int>(std::floor(srcX));
            int y0 = static_cast<int>(std::floor(srcY));
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float dx = srcX - x0;
            float dy = srcY - y0;

            if (bilinear) {
                // Interpolação bilinear usando a cópia original
                float r00, g00, b00, a00;
                float r01, g01, b01, a01;
                float r10, g10, b10, a10;
                float r11, g11, b11, a11;

                uint32_t color00 = getPixel(old_pixels, w, x0, y0);
                uint32_t color01 = getPixel(old_pixels, w, x1, y0);
                uint32_t color10 = getPixel(old_pixels, w, x0, y1);
                uint32_t color11 = getPixel(old_pixels, w, x1, y1);

                convert_hex_to_RGBA(color00, &r00, &g00, &b00, &a00);
                convert_hex_to_RGBA(color01, &r01, &g01, &b01, &a01);
                convert_hex_to_RGBA(color10, &r10, &g10, &b10, &a10);
                convert_hex_to_RGBA(color11, &r11, &g11, &b11, &a11);

                float r = (1 - dx) * (1 - dy) * r00 + dx * (1 - dy) * r01 + (1 - dx) * dy * r10 + dx * dy * r11;
                float g = (1 - dx) * (1 - dy) * g00 + dx * (1 - dy) * g01 + (1 - dx) * dy * g10 + dx * dy * g11;
                float b = (1 - dx) * (1 - dy) * b00 + dx * (1 - dy) * b01 + (1 - dx) * dy * b10 + dx * dy * b11;
                float a = (1 - dx) * (1 - dy) * a00 + dx * (1 - dy) * a01 + (1 - dx) * dy * a10 + dx * dy * a11;

                pixels[y * w + x] = convert_RGBA_to_hex(r, g, b, a);
            } else {
                // Interpolação nearest neighbor usando a cópia original
                int nearestX = static_cast<int>(std::round(srcX));
                int nearestY = static_cast<int>(std::round(srcY));

                // Garantir que os índices estejam dentro dos limites
                nearestX = nearestX;
                nearestY = nearestY;

                pixels[y * w + x] = getPixel(old_pixels, w, nearestX, nearestY);
            }
        }
    }

    // Adicionar a imagem original ao histórico de desfazer
    undo_stack.push(old_pixels);
    undo_log_stack.push("rotation");
}

float my_distance(float* arr1, float* arr2, size_t dimensions)
{
    float inner = 0;

    for (size_t i = 0; i < dimensions; ++i)
    {
        inner += std::pow((arr1[i] - arr2[i]), 2);
    }

    return std::sqrt(inner);
}

#include <cmath>
#include <cstdint>

void rgb_to_lab(float* rgb, float* l, float* a, float* b) {
    // Extract the R, G, B components from the hex_color
    float r  = rgb[0];
    float g  = rgb[1];
    float b_ = rgb[2];

    // Convert RGB to sRGB
    auto to_linear = [](float c) -> float {
        return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
    };

    r = to_linear(r);
    g = to_linear(g);
    b_ = to_linear(b_);

    // Convert linear RGB to XYZ color space
    float x = r * 0.4124564f + g * 0.3575761f + b_ * 0.1804375f;
    float y = r * 0.2126729f + g * 0.7151522f + b_ * 0.0721750f;
    float z = r * 0.0193339f + g * 0.1191920f + b_ * 0.9503041f;

    // Normalize for D65 white point
    x /= 0.95047f;
    y /= 1.00000f;
    z /= 1.08883f;

    // Convert XYZ to LAB
    auto f = [](float t) -> float {
        return (t > std::pow(6.0f / 29.0f, 3.0f)) ? std::pow(t, 1.0f / 3.0f) : (1.0f / 3.0f) * std::pow(29.0f / 6.0f, 2.0f) * t + 4.0f / 29.0f;
    };

    float fx = f(x);
    float fy = f(y);
    float fz = f(z);

    *l = 116.0f * fy - 16.0f;
    *a = 500.0f * (fx - fy);
    *b = 200.0f * (fy - fz);
}

// Example usage:
// uint32_t color = 0xE9322C; // RGB color 233, 50, 44
// float l, a, b;
// rgb_to_lab(color, &l, &a, &b);

void print_color_prob(uint32_t* pixels, int w, int h)
{
    int* probs = new int[0xffffff];

    memset(probs, 0, 0xffffff);

    for (int i = 0; i < w*h; ++i)
    {
        probs[(pixels[i] >> 8)]++;
    }

    for (int i = 0; i < 0xffffff; ++i)
    {
        if (probs[i] > 10)
        {
            printf("[%d, %d, %d] = %d\n", (i >> 16) & 0xff, (i >> 8) & 0xff, (i >> 0) & 0xff, probs[i]);
        }
    }

    delete[] probs;
}