# GameTranslator
This application allows users to capture a screenshot or an active window, extract Japanese text from the image, and translate it into English using the Google Translate API. It utilizes the Tesseract OCR library for text extraction and the OpenCV library for image processing.

## Getting Started

### Prerequisites

- Windows operating system
- [Tesseract OCR](https://github.com/tesseract-ocr/tesseract/wiki/4.0-with-LSTM#400-alpha-for-windows)
- [Google Cloud Translate API](https://cloud.google.com/translate/docs/getting-started)
- Visual Studio

### Setup

1. Clone the repository to your local machine.
2. Open the project in Visual Studio.
3. Install the required dependencies via NuGet or equivalent package manager.
4. Set the `TESSDATA_PREFIX` environment variable to the path of your `tessdata` directory, typically located in the Tesseract installation directory. Alternatively, you can move the `tessdata` directory to the project folder.
5. Obtain an API key for the Google Cloud Translate API and keep it handy.

### Running the Application

1. Build and run the application.
2. Enter your Google Cloud Translate API key when prompted.
3. Follow the on-screen instructions to capture a screenshot or an active window.
4. The application will extract Japanese text from the image, translate it into English, and display the translation in the console.

## Image Processing

The application preprocesses the image before text extraction to improve OCR accuracy. The image processing steps include:

1. Conversion to grayscale
2. Thresholding to binary
3. Denoising using the Non-Local Means Denoising algorithm
4. Sharpening using a sharpening filter

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Tesseract OCR for text extraction
- OpenCV for image processing
- Google Cloud Translate API for text translation
