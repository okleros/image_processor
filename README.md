# KIMS - Kleros Image Manipulation Software

KIMS (Kleros Image Manipulation Software) is an image processing software developed by Gutemberg Andrade or Kleros (pseudonym) for a university project in Image Processing with teacher [Yuri](https://lia.ufc.br/~yuri). It provides a user-friendly interface for performing various image manipulation tasks using the Dear Imgui library for GUI, Implot for plotting functionalities, and SDL2/SDL2Image for platform compatibility.

## Features

- **Intuitive User Interface:** KIMS offers a clean and intuitive interface powered by dear imgui, making it easy for users to navigate and perform image manipulation tasks.
- **Image Manipulation Tools:** The software provides a range of image processing tools including but not limited to:
  - Image filtering (e.g., blur*, sharpen*, edge detection*)
  - Color adjustments (e.g., brightness*, contrast, saturation)
  - Geometric transformations (e.g., rotation*, scaling*, cropping*)
- **Real-time Plotting:** Utilizing implot, KIMS allows users to visualize data and analysis results through interactive plots integrated directly into the software.
- **Save and Export:** Users can save their manipulated images in various formats and export analysis results for further use or sharing.
- **Not yet implemented:** Features marked with *

## Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/okleros/image_processor.git
   ```

2. Ensure you have the following dependencies installed:
   - SDL2
   - SDL2 Image

3. For Ubuntu/Debian based distributions, the installation of those dependencies are as follows:
  
   ```bash
   sudo apt install libsdl2-dev -y
   sudo apt install libsdl2-image-dev -y
   ```

4. Build the software by typing `make`.

## Usage

1. Run the executable generated after building.
  ```bash
   ./main
   ```
2. Use KIMS for various image manipulation tasks similar to GIMP, but with a simpler interface.

## Implemented Functionalities

- [x] Negativo
- [x] Transformações logarítmicas
- [x] Potencia (gamma)
- [ ] Linear definida por partes
- [x] Esteganografia
- [x] Exibição histograma
- [x] Equalização histograma
- [x] Limiarização
- [x] Aplicação de filtro genérico por convolução
- [x] Filtro de suavização da média [simples e ponderada]
- [x] Filtragem pela mediana
- [x] Aguçamento (nitidez) laplaciano
- [x] Aguçamento (nitidez) high boost
- [x] Filtros de sobel - x e y separados
- [x] Detecção não linear de bordas pelo gradiente
- [ ] Calculo da trasformada de fourier
- [ ] Criar ferramenta para transformação entre sistemas de cores RGB <-> HSV
- [x] Algoritmos de escala de cinza (média simples e ponderada)
- [ ] Negativo
- [x] Chroma key
- [x] Suavização e aguçamento em imagens coloridas
- [x] Equalização histograma em imagens coloridas (HSI)
- [x] Ajuste de matiz, saturação e brilho
- [ ] Ajuste de canal (C/R, M/G, Y/B)
- [ ] Sépia (escala de cinza amarelada)
- [ ] Escala pelo vizinho mais próximo e linear
- [ ] Rotação pelo vizinho mais próximo e linear


## Supported Platforms

KIMS is cross-platform and can run on Windows, macOS, and Linux. However, ensure you have the required dependencies installed for your platform.

## Usage Examples

KIMS can be used in various scenarios where image manipulation is required, similar to how GIMP is utilized. Examples include:
- Editing photographs
- Creating digital artwork
- Touching up images for web or print publication

## Contributing

Feel free to use KIMS however you want. Contributions are welcome, but there are no specific guidelines or expectations for contributors.

## Contact

For inquiries or support, please refer to the [image_processor](https://github.com/okleros/image_processor) repository on GitHub.
