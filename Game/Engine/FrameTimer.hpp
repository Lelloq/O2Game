#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <SDL2/SDL.h>
#include "../../Engine/UDim2.hpp"
#include "../../Engine/Vector2.hpp"
#include "../../Engine/Color3.hpp"
#include "../../Engine/Data/WindowsTypes.hpp"
#include "../../Engine/VulkanDriver/Texture2DVulkan.h"

class Texture2D;
class Color3;

class FrameTimer {
public:
	FrameTimer();
	FrameTimer(std::vector<Texture2D*> frames);
	FrameTimer(std::vector<std::string> frames);
	FrameTimer(std::vector<std::filesystem::path> frames);
	FrameTimer(std::vector<SDL_Texture*> frames);
	FrameTimer(std::vector<Texture2D_Vulkan*> frames);
	~FrameTimer();

	bool Repeat;
	bool AlphaBlend;
	UDim2 Position;
	UDim2 Size;
	Color3 TintColor;

	Vector2 AnchorPoint;

	Vector2 AbsolutePosition;
	Vector2 AbsoluteSize;

	void Draw(double delta);
	void Draw(double delta, Rect* clip);
	void SetFPS(float fps);
	void ResetIndex();
	void LastIndex();
	void SetIndexAt(int idx);

	void CalculateSize();
	
protected:
	std::vector<Texture2D*> m_frames;
	int m_currentFrame;
	double m_frameTime;
	double m_currentTime;
};