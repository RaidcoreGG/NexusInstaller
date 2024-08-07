﻿#include <Windows.h>
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

struct FileInfo
{
	std::string ProductName;
	std::string Description;
};

static Texture*					Background			= nullptr;
static ImFont*					Font				= nullptr;
static ImFont*					BoldFont			= nullptr;

static ID3D11Device*			Device				= nullptr;
static ID3D11DeviceContext*		DeviceContext		= nullptr;
static IDXGISwapChain*			SwapChain			= nullptr;
static ID3D11RenderTargetView*	RenderTargetView	= nullptr;

static std::string				GamePath;
static std::filesystem::path	GameDirectory;
static std::string				Message;

bool IsNexusInstalled = false;

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
void LoadFont(ImFontAtlas* aFontAtlas, float aFontSize, unsigned aResourceID, ImFont** aOutFont)
{
	if (!aFontAtlas)
	{
		return;
	}

	*aOutFont = nullptr;

	LPVOID res{};
	DWORD sz{};

	HRSRC hRes = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(aResourceID), RT_FONT);
	if (!hRes)
	{
		return;
	}

	HGLOBAL hLRes = LoadResource(GetModuleHandle(NULL), hRes);

	if (!hLRes)
	{
		return;
	}

	LPVOID pLRes = LockResource(hLRes);

	if (!pLRes)
	{
		return;
	}

	DWORD dwResSz = SizeofResource(GetModuleHandle(NULL), hRes);

	if (!dwResSz)
	{
		return;
	}

	res = pLRes;
	sz = dwResSz;

	*aOutFont = aFontAtlas->AddFontFromMemoryTTF(res, sz, aFontSize);
}

FileInfo GetFileInfo(std::filesystem::path aPath)
{
	FileInfo fi{};

	DWORD fviSz = GetFileVersionInfoSizeA(aPath.string().c_str(), 0);
	char* buffer = new char[fviSz];
	GetFileVersionInfoA(aPath.string().c_str(), 0, fviSz, buffer);

	struct LANGANDCODEPAGE
	{
		WORD wLanguage;
		WORD wCodePage;
	} *lpTranslate;

	UINT cbTranslate = 0;
	if (!VerQueryValue(buffer, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate))
	{
		return fi;
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
			fi.Description = description;
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
			fi.ProductName = description;
		}
	}

	return fi;
}

bool IsNexusAddon(std::filesystem::path aPath)
{
	bool ret = false;

	HMODULE hMod = LoadLibrary(aPath.string().c_str());

	if (hMod)
	{
		FARPROC getAddonDef = GetProcAddress(hMod, "GetAddonDef");

		if (getAddonDef)
		{
			ret = true;
		}

		FreeLibrary(hMod);
	}

	return ret;
}
void MoveNexusCompatibleToAddons(std::filesystem::path aDirectory)
{
	if (!std::filesystem::exists(aDirectory)) { return; }

	for (const std::filesystem::directory_entry entry : std::filesystem::directory_iterator(aDirectory))
	{
		std::filesystem::path dllPath = entry.path();

		if (dllPath.extension() != ".dll") { continue; }

		if (String::Contains(dllPath.string(), "CoherentUI64.dll")) { continue; }
		if (String::Contains(dllPath.string(), "d3dcompiler_43.dll")) { continue; }
		if (String::Contains(dllPath.string(), "ffmpegsumo.dll")) { continue; }
		if (String::Contains(dllPath.string(), "icudt.dll")) { continue; }
		if (String::Contains(dllPath.string(), "libEGL.dll")) { continue; }
		if (String::Contains(dllPath.string(), "libGLESv2.dll")) { continue; }

		if (IsNexusAddon(dllPath))
		{
			std::filesystem::rename(dllPath, GameDirectory / "addons" / dllPath.filename());
		}
	}
}

enum class EAddon
{
	None,
	AddonLoader,
	ArcDPS,
	Nexus,
	ReShade,
	Other
};


std::filesystem::path addonLoader;
std::filesystem::path d3d11;
std::filesystem::path d3d11_chainload;
std::filesystem::path dxgi;
std::filesystem::path bin64dxgi;
std::filesystem::path bin64cefdxgi;

EAddon ADDONLOADER		= EAddon::None;
EAddon D3D11			= EAddon::None;
EAddon D3D11_CL			= EAddon::None;
EAddon DXGI				= EAddon::None;
EAddon DXGI_B64			= EAddon::None;
EAddon DXGI_B64_CEF		= EAddon::None;

EAddon GetAddonType(std::filesystem::path aPath)
{
	if (!std::filesystem::exists(aPath))
	{
		return EAddon::None;
	}

	FileInfo fileInfo = GetFileInfo(aPath);

	if (fileInfo.ProductName == "ReShade")
	{
		return EAddon::ReShade;
	}
	
	if (fileInfo.Description == "arcdps")
	{
		return EAddon::ArcDPS;
	}
	
	if (fileInfo.ProductName == "Nexus")
	{
		return EAddon::Nexus;
	}

	uintmax_t fileSize = std::filesystem::file_size(aPath);

	if (fileSize < 50000)
	{
		// probably addon loader.
		// unfortunately, it sets fuck all, so we cannot read file info or exports
		return EAddon::AddonLoader;
	}

	return EAddon::Other;
}
std::string GetAddonTypeMsg(EAddon aType)
{
	switch (aType)
	{
	case EAddon::None: return "";

	case EAddon::AddonLoader:	return "Addon Loader";;
	case EAddon::ArcDPS:		return "ArcDPS";;
	case EAddon::Nexus:			return "Nexus";;
	case EAddon::ReShade:		return "ReShade";;
	case EAddon::Other:			return "Unknown";;
	}
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
	IsNexusInstalled = false;

	addonLoader		= GameDirectory / "addonLoader.dll";
	d3d11			= GameDirectory / "d3d11.dll";
	d3d11_chainload	= GameDirectory / "d3d11_chainload.dll";
	dxgi			= GameDirectory / "dxgi.dll";
	bin64dxgi		= GameDirectory / "bin64/dxgi.dll";
	bin64cefdxgi	= GameDirectory / "bin64/cef/dxgi.dll";

	ADDONLOADER		= GetAddonType(addonLoader);
	D3D11			= GetAddonType(d3d11);
	D3D11_CL		= GetAddonType(d3d11_chainload);
	DXGI			= GetAddonType(dxgi);
	DXGI_B64		= GetAddonType(bin64dxgi);
	DXGI_B64_CEF	= GetAddonType(bin64cefdxgi);

	std::string addonInfo = "";

	if (D3D11 == EAddon::Nexus)
	{
		IsNexusInstalled = true;
		Message = "Nexus is already installed.";
	}
}

bool Install()
{
	if (D3D11 == EAddon::AddonLoader)
	{
		/* FriendlyFire's addon loader does not chainload, we delete this file it is garbage. */
		if (D3D11_CL != EAddon::None)
		{
			std::filesystem::remove(d3d11_chainload);
		}

		std::filesystem::rename(d3d11, d3d11_chainload);
	}
	else if (D3D11 == EAddon::ArcDPS)
	{
		/* move arcdps to addons to be loaded by Nexus*/
		std::filesystem::rename(d3d11, GameDirectory / "addons/arcdps.dll");
	}
	else if (D3D11 == EAddon::ReShade)
	{
		std::filesystem::rename(d3d11, dxgi);
	}
	else if (D3D11 == EAddon::Nexus)
	{
		Message = "Nexus is already installed.";
		return false;
	}
	else if (D3D11 == EAddon::Other)
	{
		std::filesystem::rename(d3d11, d3d11_chainload);
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
			return false;
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

	UnblockFile(d3d11);

	// Move all detected Nexus addons to /addons
	MoveNexusCompatibleToAddons(GameDirectory);
	MoveNexusCompatibleToAddons(GameDirectory / "bin64");

	DetectAddons();

	return true;
}

void AddonInfoRow(EAddon aType, std::filesystem::path aPath)
{
	if (aType != EAddon::None)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);
		ImGui::PushFont(BoldFont);
		ImGui::TextColored(ImVec4(1, 1, 1, 1), GetAddonTypeMsg(aType).c_str());
		ImGui::PopFont();
		ImGui::TableSetColumnIndex(2);
		ImGui::TextColored(ImVec4(1, 1, 1, 1), aPath.string().c_str());
	}
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

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(Device, DeviceContext);

	LoadFromResource(TEX_BACKGROUND, &Background);

	LoadFont(io.Fonts, 20.0f, MAINFONT, &Font);
	LoadFont(io.Fonts, 20.0f, MAINFONT_BOLD, &BoldFont);
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

					ImGui::PushFont(BoldFont);
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
					ImGui::PushFont(BoldFont);
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

						DetectAddons();
					}
					ImGui::PopFont();
					ImGui::PopStyleVar();

					ImGui::PushFont(BoldFont);
					ImGui::TextColored(ImVec4(1, 1, 1, 1), "Detected Modifcations:");
					ImGui::PopFont();

					ImGui::BeginTable("##addoninfo", 3, ImGuiTableFlags_SizingFixedFit);

					AddonInfoRow(ADDONLOADER, addonLoader);
					AddonInfoRow(D3D11, d3d11);
					AddonInfoRow(D3D11_CL, d3d11_chainload);
					AddonInfoRow(DXGI, dxgi);
					AddonInfoRow(DXGI_B64, bin64dxgi);
					AddonInfoRow(DXGI_B64_CEF, bin64cefdxgi);
					
					ImGui::EndTable();
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();

				ImGui::PushFont(BoldFont);

				ImGui::SetCursorPos(ImVec2(16.0f, wndHeight - btnHeight - 8.0f - (ImGui::GetTextLineHeight() * 2)));
				ImGui::TextColored(ImVec4(0, 1, 0, 1), Message.c_str());

				ImGui::SetCursorPos(ImVec2(8.0f, wndHeight - btnHeight - 8.0f));
				if (ImGui::Button("Install Nexus", ImVec2(btnWidth, btnHeight)))
				{
					if (!IsNexusInstalled)
					{
						// this little jank is to overwrite the "is ALREADY installed" message
						if (Install())
						{
							IsNexusInstalled = true;
							Message = "Succesfully installed Nexus.";
						}
					}
				}

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

	HMODULE d3d11lib = LoadLibraryExA("d3d11.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
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
