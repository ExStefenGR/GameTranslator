#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <map>
#include <string>
#include <curl/curl.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <memory>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using json = nlohmann::json;


// Use using declarations instead of the old typedef style
using HtmlEntitiesMap = std::map<std::string, std::string>;

// Define a map to store HTML entities and their corresponding characters
HtmlEntitiesMap htmlEntities = 
{
	{"amp", "&"},
	{"lt", "<"},
	{"gt", ">"},
	{"quot", "\""},
	{"#39", "'"}
};

std::string extractTranslation(const std::string& response) 
{
	json jsonResponse = json::parse(response);
	std::string translatedText = jsonResponse["data"]["translations"][0]["translatedText"];
	return translatedText;
}

std::string decodeHtmlEntities(const std::string& input) 
{
	std::string result;
	std::string entity;
	bool insideEntity = false;

	for (char ch : input) {
		if (insideEntity) {
			if (ch == ';') {
				insideEntity = false;
				if (htmlEntities.count(entity)) {
					result += htmlEntities[entity];
				}
				else {
					result += entity + ";"; // Not a recognized entity, keep it unchanged
				}
				entity.clear();
			}
			else {
				entity += ch;
			}
		}
		else {
			if (ch == '&') {
				insideEntity = true;
				entity.clear();
			}
			else {
				result += ch;
			}
		}
	}

	return result;
}

std::string getTextFromImage(const char* imagePath) {
	auto api = std::make_unique<tesseract::TessBaseAPI>();

	if (_putenv("TESSDATA_PREFIX=./tessdata/") != 0) {
		std::cerr << "Failed to set TESSDATA_PREFIX environment variable." << std::endl;
		return "";
	}


	// Initialize Tesseract with both horizontal and vertical Japanese language models
	if (api->Init(nullptr, "jpn+jpn_vert")) {
		std::cerr << "Could not initialize tesseract." << std::endl;
		return "";
	}

	Pix* image = pixRead(imagePath);
	if (!image) {
		std::cerr << "Could not read the image." << std::endl;
		api->End();
		return "";
	}

	api->SetImage(image);
	auto outText = api->GetUTF8Text();
	if (!outText) {
		std::cerr << "OCR failed to extract text." << std::endl;
		api->End();
		pixDestroy(&image);
		return "";
	}

	std::string result(outText);

	api->End();
	delete[] outText;
	pixDestroy(&image);

	return result;
}




size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) 
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

std::string urlEncode(const std::string& value) 
{
	CURL* curl = curl_easy_init();
	char* encoded = curl_easy_escape(curl, value.c_str(), value.length());
	std::string result(encoded);
	curl_free(encoded);
	curl_easy_cleanup(curl);
	return result;
}

std::string translateText(const std::string& text, const std::string& apiKey) 
{
	std::string url = "https://translation.googleapis.com/language/translate/v2?key=" + apiKey;
	std::string data = "&q=" + urlEncode(text) + "&source=ja&target=en";

	CURL* curl;
	CURLcode res;
	std::string response;

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if (!curl) {
		std::cerr << "Failed to initialize cURL." << std::endl;
		curl_global_cleanup();
		return "";
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "UTF-8");

	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		return "";
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	return response;
}


bool captureScreenshot(const char* imagePath)
{

	std::filesystem::remove(imagePath);
	HDC hdcScreen = GetDC(nullptr);
	HDC hdcMemDC = CreateCompatibleDC(hdcScreen);

	POINT topLeft, bottomRight;
	std::cout << "Click and hold the left mouse button at the top-left corner of the area to capture, then drag and release at the bottom-right corner." << std::endl;

	// Wait for the left mouse button to be pressed
	while ((GetKeyState(VK_LBUTTON) & 0x100) == 0) {
		Sleep(10);
	}

	// Get the top-left corner position
	GetCursorPos(&topLeft);

	// Wait for the left mouse button to be released
	while ((GetKeyState(VK_LBUTTON) & 0x100) != 0) {
		Sleep(10);
	}

	// Get the bottom-right corner position
	GetCursorPos(&bottomRight);

	int width = bottomRight.x - topLeft.x;
	int height = bottomRight.y - topLeft.y;

	HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
	SelectObject(hdcMemDC, hbmScreen);
	BitBlt(hdcMemDC, 0, 0, width, height, hdcScreen, topLeft.x, topLeft.y, SRCCOPY);

	BITMAPINFOHEADER bmi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
	int bitsSize = 4 * width * height;
	std::vector<char> bits(bitsSize);
	GetDIBits(hdcMemDC, hbmScreen, 0, height, bits.data(), (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

	stbi_write_png(imagePath, width, height, 4, bits.data(), 4 * width);

	DeleteObject(hbmScreen);
	DeleteDC(hdcMemDC);
	ReleaseDC(nullptr, hdcScreen);

	return true;
}

bool captureActiveWindow(const char* imagePath) 
{
	std::filesystem::remove(imagePath);

	HWND hwnd = GetForegroundWindow();
	if (hwnd == nullptr) {
		std::cerr << "Failed to get active window handle." << std::endl;
		return false;
	}

	RECT windowRect;
	if (!GetWindowRect(hwnd, &windowRect)) {
		std::cerr << "Failed to get active window coordinates." << std::endl;
		return false;
	}

	int width = windowRect.right - windowRect.left;
	int height = windowRect.bottom - windowRect.top;

	HDC hdcScreen = GetDC(NULL);
	HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
	HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);
	SelectObject(hdcMemDC, hbmScreen);
	BitBlt(hdcMemDC, 0, 0, width, height, hdcScreen, windowRect.left, windowRect.top, SRCCOPY);

	BITMAPINFOHEADER bmi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
	int bitsSize = 4 * width * height;
	std::vector<char> bits(bitsSize);
	GetDIBits(hdcMemDC, hbmScreen, 0, height, bits.data(), (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

	stbi_write_png(imagePath, width, height, 4, bits.data(), 4 * width);

	DeleteObject(hbmScreen);
	DeleteDC(hdcMemDC);
	ReleaseDC(nullptr, hdcScreen);

	return true;
}

void imageProcessor(const char* imagePath) 
{
	cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
	if (image.empty()) {
		std::cerr << "Could not read the image." << std::endl;
		return;
	}

	// Convert the image to grayscale
	cv::Mat gray;
	cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

	// Threshold the image to binary
	cv::Mat binary;
	cv::threshold(gray, binary, 128, 255, cv::THRESH_BINARY);

	// Denoise the image using the Non-Local Means Denoising algorithm with reduced strength
	cv::Mat denoised;
	cv::fastNlMeansDenoising(binary, denoised, 4, 7, 21); // Further reduced h parameter from 15 to 8

	// Apply a sharpening filter to the image
	cv::Mat sharpeningKernel = (cv::Mat_<float>(3, 3) <<
		-1, -1, -1,
		-1, 9, -1,
		-1, -1, -1);

	cv::Mat sharpened;
	cv::filter2D(denoised, sharpened, -1, sharpeningKernel);

	// Save the preprocessed image back to disk
	cv::imwrite(imagePath, sharpened);
}

int main() {
	const char* imagePath = "screenshot.png";
	std::string apiKey;

	// Prompt the user for their API key
	std::cout << "Please enter your API key: ";
	std::cin >> apiKey;

	while (true) {
		std::cout << " " << std::endl;

		char choice;
		while (true) {
			if (GetAsyncKeyState('1')) {
				choice = '1';
				break;
			}
			else if (GetAsyncKeyState('2')) {
				choice = '2';
				break;
			}
			else if (GetAsyncKeyState('Q') || GetAsyncKeyState('q')) {
				choice = 'Q';
				break;
			}
			Sleep(100);  // Sleep for 100ms to prevent high CPU usage
		}

		if (choice == '1') {
			captureActiveWindow(imagePath);
		}
		else if (choice == '2') {
			captureScreenshot(imagePath);
		}
		else if (choice == 'Q' || choice == 'q') {
			break;
		}
		else {
			std::cout << "Invalid choice. Try again." << std::endl;
			continue;
		}
		// Preprocess the image
		imageProcessor(imagePath);

		std::string japaneseText = getTextFromImage(imagePath);

		if (japaneseText.empty()) {
			std::cerr << "No text was extracted from the image." << std::endl;
			continue;
		}

		std::string translatedText = translateText(japaneseText, apiKey);
		std::string actualTranslation = extractTranslation(translatedText);

		// Decode HTML entities in the translated text
		actualTranslation = decodeHtmlEntities(actualTranslation);
		std::cout << "Translated text: " << actualTranslation << std::endl;
	}

	return 0;
}
