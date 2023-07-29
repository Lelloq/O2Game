﻿#define _CRTDBG_MAP_ALLOC
#undef NDEBUG
#include <algorithm>
#include <windows.h>
#include <commdlg.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "MyGame.h"
#include "EnvironmentSetup.hpp"
#include "Data/bms.hpp"
#include <filesystem>
#include "../Engine/Win32ErrorHandling.h"
#include "Data/Util/Util.hpp"
#include "../Engine/Configuration.hpp"
#include "Resources/DefaultConfiguration.h"
#include <shlobj.h>
#include "Data/OJM.hpp"
#include <fstream>
#include "../Engine/MsgBox.hpp"

#if _WIN32
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) {

	if (uMsg == BFFM_INITIALIZED)
	{
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}

	return 0;
}

std::filesystem::path BrowseFolder(std::wstring saved_path) {
	wchar_t path[MAX_PATH];

	const wchar_t* path_param = saved_path.c_str();

	BROWSEINFO bi = { 0 };
	bi.lpszTitle = (L"Browse for Music Folder (.ojn files folder!)");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM)path_param;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

	if (pidl != 0)
	{
		//get the name of the folder and put it in path
		SHGetPathFromIDList(pidl, path);

		//free memory used
		IMalloc* imalloc = 0;
		if (SUCCEEDED(SHGetMalloc(&imalloc)))
		{
			imalloc->Free(pidl);
			imalloc->Release();
		}

		return path;
	}

	return "";
}

bool API_Query() {
#ifdef _DEBUG
	return true;
#else
	std::cout << "[Auth] Authenticating session to /~estrol-game/authorize-access" << std::endl;

	try {
		curlpp::Cleanup myCleanup;
		curlpp::Easy myRequest;

		curlpp::options::Url url("https://cdn.estrol.dev/~estrol-game/authorize-access");
		myRequest.setOpt(url);

		std::ostringstream os;
		os << myRequest;
		std::string response = os.str();

		return response == "{\"error\": 200, \"message\": \"OK\"}";
	}
	catch (curlpp::RuntimeError) {
		return false;
	}
#endif
}

int Run(int argc, wchar_t* argv[]) {
	try {
		Configuration::SetDefaultConfiguration(defaultConfiguration);

		std::filesystem::path parentPath = std::filesystem::path(argv[0]).parent_path();
		if (parentPath.empty()) {
			parentPath = std::filesystem::current_path();
		}

		if (SetCurrentDirectoryW((LPWSTR)parentPath.wstring().c_str()) == FALSE) {
			std::cout << "GetLastError(): " << GetLastError() << ", with path: " << parentPath.string();
			
			MessageBoxA(NULL, "Failed to set directory!", "EstGame Error", MB_ICONERROR);
			return -1;
		}

		std::filesystem::path path = Configuration::Load("Music", "Folder");
		if (path.empty() || !std::filesystem::exists(path)) {
			path = BrowseFolder(parentPath.wstring());
		}

		if (path.empty() || !std::filesystem::exists(path)) {
			return -1;
		}
		else {
			Configuration::Set("Music", "Folder", path.string());
		}

		auto hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		Win32Exception::ThrowIfError(hr);

		for (int i = 1; i < argc; i++) {
			std::wstring arg = argv[i];

			// --autoplay, -a
			if (arg.find(L"--autoplay") != std::wstring::npos || arg.find(L"-a") != std::wstring::npos) {
				EnvironmentSetup::SetInt("ParameterAutoplay", 1);
			}

			// --rate, -r [float value range 0.5 - 2.0]
			if (arg.find(L"--rate") != std::wstring::npos || arg.find(L"-r") != std::wstring::npos) {
				if (i + 1 < argc) {
					float rate = std::stof(argv[i + 1]);
					rate = std::clamp(rate, 0.5f, 2.0f);

					EnvironmentSetup::Set("ParameterRate", std::to_string(rate));
				}
			}

			if (std::filesystem::exists(argv[i]) && EnvironmentSetup::GetPath("FILE").empty()) {
				std::filesystem::path path = argv[i];

				EnvironmentSetup::SetPath("FILE", path);
			}
		}
		
		MyGame game;
		if (game.Init()) {
			double frameLimit = std::atof(Configuration::Load("Game", "FrameLimit").c_str());
			game.Run(frameLimit);
		}

		CoUninitialize();
		return 0;
	}
	catch (std::exception& e) {
		MessageBoxA(NULL, e.what(), "EstGame Error", MB_ICONERROR);
		return -1;
	}
}

int HandleStructualException(int code) {
	MessageBoxA(NULL, ("StructureExceptionHandlingOccured: " + std::to_string(code)).c_str(), "FATAL ERROR", MB_ICONERROR);
	return EXCEPTION_EXECUTE_HANDLER;
}

#if _DEBUG
int wmain(int argc, wchar_t* argv[]) {
	return Run(argc, argv);

	__try {
		
	}
	__except (HandleStructualException(GetExceptionCode())) {
		return -1;
	}

	/*std::filesystem::path path = "E:\\Games\\O2JamINT\\Musi1\\BGM.ojm";
	OJM ojm = {};
	ojm.Load(path);

	if (ojm.IsValid()) {
		for (auto& sample : ojm.Samples) {
			std::fstream file("E:\\Games\\O2JamINT\\Musi1\\out\\" + std::to_string(sample.RefValue) + ".ogg", std::ios::out | std::ios::binary);
			if (!file.is_open()) {
				DebugBreak();
			}

			file.write((const char*)sample.AudioData.data(), sample.AudioData.size());
			file.close();
		}
	}*/
}
#else=
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {
	if (!API_Query()) {
		MessageBoxA(NULL, "Failed to Authenticate to master server!", "Auth Failed", MB_ICONERROR);
		return -1;
	}
	
	__try {
		return Run(__argc, __wargv);
	}
	__except (HandleStructualException(GetExceptionCode())) {
		return -1;
	}
}
#endif