#include <Windows.h>
#include <Winver.h>
#include <d3d11.h>
#include <string>
#include <filesystem>
#include <windows.h>
#include <wininet.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "Resources/resource.h"

namespace String
{
	bool Contains(const std::string& aString, const std::string& aStringFind)
	{
		return aString.find(aStringFind) != std::string::npos;
	}
}

struct Texture
{
	unsigned Width;
	unsigned Height;
	ID3D11ShaderResourceView* Resource;
};

static Texture*					Background			= nullptr;

static ID3D11Device*			Device				= nullptr;
static ID3D11DeviceContext*		DeviceContext		= nullptr;
static IDXGISwapChain*			SwapChain			= nullptr;
static ID3D11RenderTargetView*	RenderTargetView	= nullptr;

static std::string				GamePath;
static std::filesystem::path	GameDirectory;
static std::string				AddonInfo;
static std::string				Message;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool FindFunction(HMODULE aModule, LPVOID aFunction, LPCSTR aName)
{
	FARPROC* fp = (FARPROC*)aFunction;
	*fp = aModule ? GetProcAddress(aModule, aName) : 0;
	return (*fp != 0);
}

bool LoadFromResource(unsigned aResourceID, Texture** aOutTexture)
{
	HRSRC imageResHandle = FindResourceA(GetModuleHandle(NULL), MAKEINTRESOURCEA(aResourceID), "PNG");
	if (!imageResHandle)
	{
		return false;
	}

	HGLOBAL imageResDataHandle = LoadResource(GetModuleHandle(NULL), imageResHandle);
	if (!imageResDataHandle)
	{
		return false;
	}

	LPVOID imageFile = LockResource(imageResDataHandle);
	if (!imageFile)
	{
		return false;
	}

	DWORD imageFileSize = SizeofResource(GetModuleHandle(NULL), imageResHandle);
	if (!imageFileSize)
	{
		return false;
	}

	int image_width = 0;
	int image_height = 0;
	unsigned char* image_data = stbi_load_from_memory((const stbi_uc*)imageFile, imageFileSize, &image_width, &image_height, NULL, 4);
	
	//int image_width = 0;
	//int image_height = 0;
	//unsigned char* image_data = stbi_load("Background.png", &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
		return false;

	Texture* tex = new Texture{};
	tex->Width = image_width;
	tex->Height = image_height;

	// Create texture
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = image_width;
	desc.Height = image_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	ID3D11Texture2D* pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = image_data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;
	HRESULT res = Device->CreateTexture2D(&desc, &subResource, &pTexture);

	HRESULT test = res;

	if (!pTexture)
	{
		stbi_image_free(image_data);
		return false;
	}

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	Device->CreateShaderResourceView(pTexture, &srvDesc, &tex->Resource);
	pTexture->Release();

	*aOutTexture = tex;

	stbi_image_free(image_data);
}

// taken from: https://stackoverflow.com/questions/34065/how-to-read-a-value-from-the-windows-registry
void GetExecutablePath()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\ArenaNet\\Guild Wars 2", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		return;
	}

	DWORD type;
	DWORD cbData;
	if (RegQueryValueEx(hKey, "Path", NULL, &type, NULL, &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return;
	}

	if (type != REG_SZ)
	{
		RegCloseKey(hKey);
		return;
	}

	std::string value(cbData / sizeof(char), '\0');
	if (RegQueryValueEx(hKey, "Path", NULL, NULL, reinterpret_cast<LPBYTE>(&value[0]), &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return;
	}

	RegCloseKey(hKey);

	GamePath = value;
	GameDirectory = GamePath;
	GameDirectory = GameDirectory.parent_path();
}

bool UnblockFile(std::filesystem::path aFileName)
{
	return DeleteFile((aFileName.string() + ":Zone.Identifier").c_str());
}

void DetectAddons()
{
	std::filesystem::path d3d11 = GameDirectory / "d3d11.dll";
	std::filesystem::path d3d11_chainload = GameDirectory / "d3d11_chainload.dll";
	std::filesystem::path dxgi = GameDirectory / "dxgi.dll";
	std::filesystem::path bin64dxgi = GameDirectory / "bin64/dxgi.dll";
	std::filesystem::path bin64cefdxgi = GameDirectory / "bin64/cef/dxgi.dll";
	std::filesystem::path addonLoader = GameDirectory / "addonLoader.dll";

	std::string addonInfo = "";

	if (std::filesystem::exists(d3d11)) { addonInfo += d3d11.string() + "\n"; }
	if (std::filesystem::exists(d3d11_chainload)) { addonInfo += d3d11_chainload.string() + "\n"; }
	if (std::filesystem::exists(dxgi)) { addonInfo += dxgi.string() + "\n"; }
	if (std::filesystem::exists(bin64dxgi)) { addonInfo += bin64dxgi.string() + "\n"; }
	if (std::filesystem::exists(bin64cefdxgi)) { addonInfo += bin64cefdxgi.string() + "\n"; }
	if (std::filesystem::exists(addonLoader)) { addonLoader += d3d11.string() + "\n"; }

	if (addonInfo == "")
	{
		addonInfo = "No modifications installed.";
	}

	AddonInfo = addonInfo;
}

void Install()
{
	std::filesystem::path d3d11 = GameDirectory / "d3d11.dll";
	std::filesystem::path d3d11_chainload = GameDirectory / "d3d11_chainload.dll";
	std::filesystem::path dxgi = GameDirectory / "dxgi.dll";
	std::filesystem::path bin64dxgi = GameDirectory / "bin64/dxgi.dll";
	std::filesystem::path bin64cefdxgi = GameDirectory / "bin64/cef/dxgi.dll";
	std::filesystem::path addonLoader = GameDirectory / "addonLoader.dll";

	// move addonloader/arcdps/any other d3d11 wrapper
	if (std::filesystem::exists(addonLoader))
	{
		// FriendlyFire's addon loader does not chainload, so we just prompt to delete it
		if (std::filesystem::exists(d3d11_chainload))
		{
			int dr = MessageBox(NULL, "Unused d3d11_chainload found. Delete?", "Delete chainload?", MB_YESNO);

			if (dr == IDYES)
			{
				std::filesystem::remove(d3d11_chainload);
			}
			else
			{
				std::filesystem::rename(d3d11_chainload, d3d11_chainload.string() + ".old");
			}
		}

		// move FF's AL's d3d11 to chainload
		std::filesystem::rename(d3d11, d3d11_chainload);
	}
	else if (std::filesystem::exists(d3d11))
	{
		std::string fileDescription, productName;

		DWORD fviSz = GetFileVersionInfoSizeA(d3d11.string().c_str(), 0);
		char* buffer = new char[fviSz];
		GetFileVersionInfoA(d3d11.string().c_str(), 0, fviSz, buffer);

		struct LANGANDCODEPAGE
		{
			WORD wLanguage;
			WORD wCodePage;
		} *lpTranslate;

		UINT cbTranslate = 0;
		if (!VerQueryValue(buffer, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate))
		{
			return;
		}
		for (unsigned int i = 0; i < (cbTranslate / sizeof(LANGANDCODEPAGE)); i++)
		{
			char subblock[256];
			//use sprintf if sprintf_s is not available
			sprintf_s(subblock, "\\StringFileInfo\\%04x%04x\\FileDescription",
				lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);
			char* description = NULL;
			UINT dwBytes;
			if (VerQueryValue(buffer, subblock, (LPVOID*)&description, &dwBytes))
			{
				fileDescription = description;
			}
		}
		for (unsigned int i = 0; i < (cbTranslate / sizeof(LANGANDCODEPAGE)); i++)
		{
			char subblock[256];
			//use sprintf if sprintf_s is not available
			sprintf_s(subblock, "\\StringFileInfo\\%04x%04x\\ProductName",
				lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);
			char* description = NULL;
			UINT dwBytes;
			if (VerQueryValue(buffer, subblock, (LPVOID*)&description, &dwBytes))
			{
				productName = description;
			}
		}

		if (!std::filesystem::exists(GameDirectory / "addons"))
		{
			std::filesystem::create_directory(GameDirectory / "addons");
		}

		if (productName == "ReShade")
		{
			std::filesystem::rename(d3d11, dxgi);
		}
		else if (fileDescription == "arcdps")
		{
			// move arcdps to addons to be loaded by Nexus
			std::filesystem::rename(d3d11, GameDirectory / "addons/arcdps.dll");
		}
		else if (productName == "Nexus")
		{
			int dr = MessageBoxA(NULL, "Nexus is already installed!", "Info", 0);
			if (dr == IDOK)
			{
				return;
			}
		}
		else
		{
			std::filesystem::rename(d3d11, d3d11_chainload);
		}
	}

	// Install Nexus
	/*using (var client = new WebClient())
	{
		client.DownloadFile("https://api.raidcore.gg/d3d11.dll", d3d11);
	}*/
	{
		HINTERNET hInternetSession;
		HINTERNET hURL;
		// I'm only going to access 1K of info.
		BOOL bResult;
		DWORD dwBytesRead = 1;

		// Make internet connection.
		hInternetSession = InternetOpen(
			"tes", // agent
			INTERNET_OPEN_TYPE_PRECONFIG,  // access
			NULL, NULL, 0);                // defaults

		// Make connection to desired page.
		hURL = InternetOpenUrl(
			hInternetSession,                       // session handle
			"https://api.raidcore.gg/d3d11.dll",  // URL to access
			NULL, 0, 0, 0);                         // defaults

		// Read page into memory buffer.

		char buf[1024];

		DWORD dwTemp;
		HANDLE hFile = CreateFile(d3d11.string().c_str(), GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (INVALID_HANDLE_VALUE == hFile) {
			return;
		}

		for (; dwBytesRead > 0;)
		{
			InternetReadFile(hURL, buf, (DWORD)sizeof(buf), &dwBytesRead);
			WriteFile(hFile, buf, dwBytesRead, &dwTemp, NULL);
		}

		// Close down connections.
		InternetCloseHandle(hURL);
		InternetCloseHandle(hInternetSession);

		CloseHandle(hFile);
	}

	//UnblockFile(d3d11);

	// Move all detected Nexus addons to /addons
	for (const std::filesystem::directory_entry entry : std::filesystem::directory_iterator(GameDirectory))
	{
		std::filesystem::path dllPath = entry.path();

		if (dllPath.extension() != ".dll") { continue; }

		HMODULE hMod = LoadLibrary(dllPath.string().c_str()); // this is cursed

		if (hMod)
		{
			FARPROC getAddonDef = GetProcAddress(hMod, "GetAddonDef");

			if (getAddonDef)
			{
				std::filesystem::rename(dllPath, GameDirectory / "addons" / dllPath.filename());
			}

			FreeLibrary(hMod);
		}
	}
	for (const std::filesystem::directory_entry entry : std::filesystem::directory_iterator(GameDirectory))
	{
		std::filesystem::path dllPath = entry.path();

		if (dllPath.extension() != ".dll") { continue; }

		if (String::Contains(dllPath.string(), "CoherentUI64.dll")) { continue; }
		if (String::Contains(dllPath.string(), "d3dcompiler_43.dll")) { continue; }
		if (String::Contains(dllPath.string(), "ffmpegsumo.dll")) { continue; }
		if (String::Contains(dllPath.string(), "icudt.dll")) { continue; }
		if (String::Contains(dllPath.string(), "libEGL.dll")) { continue; }
		if (String::Contains(dllPath.string(), "libGLESv2.dll")) { continue; }

		HMODULE hMod = LoadLibrary(dllPath.string().c_str()); // this is cursed

		if (hMod)
		{
			FARPROC getAddonDef = GetProcAddress(hMod, "GetAddonDef");

			if (getAddonDef)
			{
				std::filesystem::rename(dllPath, GameDirectory / "addons" / dllPath.filename());
			}

			FreeLibrary(hMod);
		}
	}

	Message = "Succesfully installed Nexus.";
}

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
int main()
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);

	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(NULL), LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(MAINICON)) , NULL, NULL, NULL, "Raidcore_Dx_Window_Class", NULL};
	::RegisterClassEx(&wc);
	HWND hwnd = ::CreateWindowEx(NULL, wc.lpszClassName, "Raidcore Nexus Installer", WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_MINIMIZEBOX, (desktop.right - 520) / 2, (desktop.bottom - 640) / 2, 520, 640, NULL, NULL, wc.hInstance, NULL);
	SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
	SetLayeredWindowAttributes(hwnd, 0, 0, LWA_COLORKEY);
	
	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(Device, DeviceContext);

	if (!Background)
	{
		LoadFromResource(TEX_BACKGROUND, &Background);
	}

	LPVOID res{}; DWORD sz{};
	HRSRC hRes = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(MAINFONT), RT_FONT);
	if (hRes)
	{
		HGLOBAL hLRes = LoadResource(GetModuleHandle(NULL), hRes);

		if (hLRes)
		{
			LPVOID pLRes = LockResource(hLRes);

			if (pLRes)
			{
				DWORD dwResSz = SizeofResource(GetModuleHandle(NULL), hRes);

				if (0 != dwResSz)
				{
					res = pLRes;
					sz = dwResSz;
				}
			}
		}
	}
	LPVOID resB{}; DWORD szB{};
	HRSRC hResB = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(MAINFONT_BOLD), RT_FONT);
	if (hResB)
	{
		HGLOBAL hLResB = LoadResource(GetModuleHandle(NULL), hResB);

		if (hLResB)
		{
			LPVOID pLResB = LockResource(hLResB);

			if (pLResB)
			{
				DWORD dwResSzB = SizeofResource(GetModuleHandle(NULL), hResB);

				if (0 != dwResSzB)
				{
					resB = pLResB;
					szB = dwResSzB;
				}
			}
		}
	}

	io.Fonts->AddFontFromMemoryTTF(res, sz, 20.0f);
	ImFont* boldFont = io.Fonts->AddFontFromMemoryTTF(resB, szB, 20.0f);
	io.Fonts->Build();

	ImVec4 clear_color = ImVec4(0, 0, 0, 0);

	bool done = false;

	GetExecutablePath();
	DetectAddons();

	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (!done && msg.message != WM_QUIT)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.059f, 0.475f, 0.667f, 1));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0.580f, 1, 1));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0.580f, 1, 1));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0.580f, 1, 1));

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(520, 640));
			ImGui::Begin("Raidcore_Imgui_Window", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
			{
				float wndHeight = ImGui::GetWindowHeight();
				float wndWidth = ImGui::GetWindowWidth();

				float btnWidth = (wndWidth - 24.0f) / 2;
				float btnHeight = ImGui::GetFontSize() * 2;

				ImGui::SetCursorPos(ImVec2(0, 0));
				ImGui::Image(Background->Resource, ImVec2(520, 640));;

				ImGui::SetCursorPosX(8.0f);
				ImGui::SetCursorPosY(132.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0.5f));
				ImGui::BeginChild("Raidcore_Imgui_Body", ImVec2(wndWidth - 16.0f, wndHeight - 132.0f - btnHeight - 16.0f), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
				{
					ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));

					ImGui::PushFont(boldFont);
					ImGui::TextColored(ImVec4(1, 1, 1, 1), "Game Location:");
					ImGui::PopFont();

					if (GamePath.empty())
					{
						ImGui::TextColored(ImVec4(1, 1, 0, 1), "Could not locate Guild Wars 2 executable.");
					}
					else
					{
						ImGui::TextColored(ImVec4(0.863f, 0.863f, 0.863f, 1), GamePath.c_str());
					}

					ImGui::SameLine();

					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
					ImGui::PushFont(boldFont);
					if (ImGui::SmallButton("Locate Game"))
					{
						OPENFILENAME ofn{};
						char buff[MAX_PATH]{};

						ofn.lStructSize = sizeof(OPENFILENAME);
						ofn.hwndOwner = hwnd;
						ofn.lpstrFile = buff;
						ofn.nMaxFile = MAX_PATH;
						ofn.lpstrFilter = "Guild Wars 2 Executable\0*.exe";
						ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

						if (GetOpenFileName(&ofn))
						{
							GamePath = ofn.lpstrFile != 0 ? ofn.lpstrFile : "";
							GameDirectory = GamePath;
							GameDirectory = GameDirectory.parent_path();
						}
					}
					ImGui::PopFont();
					ImGui::PopStyleVar();

					ImGui::PushFont(boldFont);
					ImGui::TextColored(ImVec4(1, 1, 1, 1), "Detected Modifcations:");
					ImGui::PopFont();
					ImGui::TextColored(ImVec4(0.863f, 0.863f, 0.863f, 1), AddonInfo.c_str());
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();

				ImGui::SetCursorPos(ImVec2(16.0f, wndHeight - btnHeight - 8.0f - (ImGui::GetTextLineHeight() * 2)));
				ImGui::TextColored(ImVec4(0, 1, 0, 1), Message.c_str());

				ImGui::PushFont(boldFont);
				ImGui::SetCursorPos(ImVec2(8.0f, wndHeight - btnHeight - 8.0f));
				if (ImGui::Button("Install Nexus", ImVec2(btnWidth, btnHeight)))
				{
					Install();
					DetectAddons();
				}
				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(wndWidth - btnWidth - 8.0f, wndHeight - btnHeight - 8.0f));
				if (ImGui::Button("Quit", ImVec2(btnWidth, btnHeight)))
				{
					done = true;
				}
				ImGui::PopFont();
			}
			ImGui::End();
			ImGui::PopStyleVar(2);

			ImGui::PopStyleColor(4);
		}

		ImGui::EndFrame();
		ImGui::Render();
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, NULL);
		DeviceContext->ClearRenderTargetView(RenderTargetView, (float*)&clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		SwapChain->Present(1, 0);
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClass(wc.lpszClassName, wc.hInstance);

	return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createDeviceAndSwapChain_func = 0;

	HMODULE d3d11lib = LoadLibrary("d3d11.dll");
	FindFunction(d3d11lib, &createDeviceAndSwapChain_func, "D3D11CreateDeviceAndSwapChain");

	if (!createDeviceAndSwapChain_func)
	{
		return -1;
	}

	if (createDeviceAndSwapChain_func(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, &featureLevel, &DeviceContext) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (SwapChain) { SwapChain->Release(); SwapChain = NULL; }
	if (DeviceContext) { DeviceContext->Release(); DeviceContext = NULL; }
	if (Device) { Device->Release(); Device = NULL; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	Device->CreateRenderTargetView(pBackBuffer, NULL, &RenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (Device != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			SwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
