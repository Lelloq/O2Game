#pragma once
#include <vector>
#include "../../Engine/EstEngine.hpp"
#include "../../Engine/Data/WindowsTypes.hpp"
#include "../Data/Chart.hpp"
#include "../Data/AutoReplay.hpp"
#include "GameTrack.hpp"
#include "TimingLineManager.hpp"
#include "ScoreManager.hpp"

enum class GameState {
	PreParing,
	NotGame,
	PreGame,
	Playing,
	PosGame
};

class RhythmEngine {
public:
	RhythmEngine();
	~RhythmEngine();

	bool Load(Chart* chart);
	void SetKeys(Keys* keys);

	bool Start();
	bool Stop();
	bool Ready();

	void Update(double delta);
	void Render(double delta);
	void Input(double delta);

	void OnKeyDown(const KeyState& key);
	void OnKeyUp(const KeyState& key);

	void ListenKeyEvent(std::function<void(GameTrackEvent)> callback);

	double GetAudioPosition() const;
	double GetVisualPosition() const;
	double GetGameAudioPosition() const;
	double GetTrackPosition() const;
	double GetPrebufferTiming() const;
	double GetNotespeed() const;
	double GetBPMAt(double offset) const;
	double GetCurrentBPM() const;
	double GetSongRate() const;
	int GetAudioLength() const;
	int GetGameVolume() const;

	double GetPositionFromOffset(double offset);
	double GetPositionFromOffset(double offset, int index);

	int* GetLaneSizes() const;
	int* GetLanePos() const;

	void SetHitPosition(int offset);
	void SetLaneOffset(int offset);
	int GetHitPosition() const;
	Vector2 GetResolution() const;
	Rect GetPlayRectangle() const;
	std::u8string GetTitle() const;
	
	GameState GetState() const;
	ScoreManager* GetScoreManager() const;
	std::vector<double> GetTimingWindow();
	std::vector<TimingInfo> GetBPMs() const;
	std::vector<TimingInfo> GetSVs() const;

	double GetElapsedTime() const;
	int GetPlayTime() const;

	int GetGuideLineIndex() const;
	void SetGuideLineIndex(int idx);

private:
	void UpdateNotes();
	void UpdateGamePosition();
	void UpdateVirtualResolution();
	void CreateTimingMarkers();

	void Release();

	double m_rate, m_offset, m_beatmapOffset;
	double m_currentAudioPosition;
	double m_currentVisualPosition;
	double m_currentAudioGamePosition;
	double m_currentTrackPosition;
	float m_baseBPM, m_currentBPM;
	float m_currentSVMultiplier;

	int m_currentSampleIndex = 0;
	int m_currentNoteIndex = 0;
	int m_currentBPMIndex = 0;
	int m_currentSVIndex = 0;
	int m_scrollSpeed = 0;
	int m_audioLength = 0;
	int m_hitPosition = 0;
	int m_laneOffset = 0;
	int m_audioVolume = 100;
	int m_audioOffset = 0;

	int m_guideLineIndex = 0;

	bool m_started = false;
	GameState m_state = GameState::NotGame;
	std::u8string m_title;

	Rect m_playRectangle;
	int m_laneSize[7];
	int m_lanePos[7];

	std::filesystem::path m_audioPath = "";
	Chart* m_currentChart;
	Vector2 m_virtualResolution;
	Vector2 m_gameResolution;
	std::vector<double> m_timingPositionMarkers;
	std::vector<GameTrack*> m_tracks;
	//std::vector<NoteInfo> m_notes;
	std::vector<NoteInfoDesc> m_noteDescs;
	std::vector<AutoSample> m_autoSamples;
	std::unordered_map<int, int> m_autoHitIndex;
	std::unordered_map<int, std::vector<ReplayHitInfo>> m_autoHitInfos;

	/* clock system */
	int m_PlayTime;
	std::chrono::system_clock::time_point m_startClock;

	ScoreManager* m_scoreManager;
	TimingLineManager* m_timingLineManager;
	std::function<void(GameTrackEvent)> m_eventCallback;
};