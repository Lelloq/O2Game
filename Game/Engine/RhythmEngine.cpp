#include "RhythmEngine.hpp"
#include <unordered_map>
#include <filesystem>

#include "../../Engine/EstEngine.hpp"
#include "../EnvironmentSetup.hpp"
#include "../../Engine/Configuration.hpp"

#include "NoteImageCacheManager.hpp"
#include "GameAudioSampleCache.hpp"
#include "NoteResult.hpp"

#include <chrono>
#include <codecvt>

struct ManiaKeyState {
	Keys key;
	bool isPressed;
};

namespace {
	std::vector<NoteImageType> Key2Type = {
		NoteImageType::LANE_1,
		NoteImageType::LANE_2,
		NoteImageType::LANE_3,
		NoteImageType::LANE_4,
		NoteImageType::LANE_5,
		NoteImageType::LANE_6,
		NoteImageType::LANE_7,
	};

	std::vector<NoteImageType> Key2HoldType = {
		NoteImageType::HOLD_LANE_1,
		NoteImageType::HOLD_LANE_2,
		NoteImageType::HOLD_LANE_3,
		NoteImageType::HOLD_LANE_4,
		NoteImageType::HOLD_LANE_5,
		NoteImageType::HOLD_LANE_6,
		NoteImageType::HOLD_LANE_7,
	};

	std::unordered_map<int, ManiaKeyState> KeyMapping = {
		{ 0, { Keys::A, false }},
		{ 1, {Keys::S, false} },
		{ 2, {Keys::D, false} },
		{ 3, {Keys::Space, false} },
		{ 4, {Keys::J, false} },
		{ 5, {Keys::K, false} },
		{ 6, {Keys::L, false} },
	};

	int trackOffset[] = { 5, 33, 55, 82, 114, 142, 164 };
}

RhythmEngine::RhythmEngine() {
	m_currentAudioGamePosition = 0;
	m_currentVisualPosition = 0;
	m_currentTrackPosition = 0;
	m_rate = 1;
	m_offset = 0;
	m_scrollSpeed = 180;

	m_timingPositionMarkers = std::vector<double>();
}

RhythmEngine::~RhythmEngine() {
	Release();
}

bool RhythmEngine::Load(Chart* chart) {
	m_state = GameState::PreParing;
	m_currentChart = chart;

	m_autoHitIndex.clear();
	m_autoHitInfos.clear();

	int currentX = m_laneOffset;
	for (int i = 0; i < 7; i++) {
		m_tracks.push_back(new GameTrack( this, i, currentX ));
		m_autoHitIndex[i] = 0;

		if (m_eventCallback) {
			m_tracks[i]->ListenEvent([&](GameTrackEvent e) {
				m_eventCallback(e);
			});
		}

		int size = GameNoteResource::GetNoteTexture(Key2Type[i])->TextureRect.right;
		
		m_lanePos[i] = currentX;
		m_laneSize[i] = size;
		currentX += size;
	}

	m_playRectangle = { m_laneOffset, 0, currentX, m_hitPosition };

	std::filesystem::path audioPath = chart->m_beatmapDirectory;
	audioPath /= chart->m_audio;

	if (EnvironmentSetup::GetInt("Mirror")) {
		chart->ApplyMod(Mod::MIRROR);
	}
	else if (EnvironmentSetup::GetInt("Random")) {
		chart->ApplyMod(Mod::RANDOM);
	}
	else if (EnvironmentSetup::GetInt("Rearrange")) {
		void* lane_data = EnvironmentSetup::GetObj("LaneData");

		chart->ApplyMod(Mod::REARRANGE, lane_data);
	}

	if (std::filesystem::exists(audioPath) && audioPath.has_extension()) {
		m_audioPath = audioPath;
	}

	for (auto& sample : chart->m_autoSamples) {
		m_autoSamples.push_back(sample);
	}

	if (EnvironmentSetup::GetInt("Autoplay") == 1) {
		std::cout << "AutoPlay enabled!" << std::endl;
		auto replay = AutoReplay::CreateReplay(chart);
		std::sort(replay.begin(), replay.end(), [](const ReplayHitInfo& a, const ReplayHitInfo& b) {
			return a.Time < b.Time;
		});

		for (auto& hit : replay) {
			m_autoHitInfos[hit.Lane].push_back(hit);
		}
	}

	auto audioVolume = Configuration::Load("Game", "AudioVolume");
	if (audioVolume.size() > 0) {
		try {
			m_audioVolume = std::atoi(audioVolume.c_str());
		}
		catch (std::invalid_argument e) {
			std::cout << "Game.ini::AudioVolume invalid volume: " << audioVolume << " reverting to 100 value" << std::endl;
			m_audioVolume = 100;
		}
	}

	auto audioOffset = Configuration::Load("Game", "AudioOffset");
	if (audioOffset.size() > 0) {
		try {
			m_audioOffset = std::atoi(audioOffset.c_str());
		}

		catch (std::invalid_argument e) {
			std::cout << "Game.ini::AudioOffset invalid offset: " << audioOffset << " reverting to 0 value" << std::endl;
			m_audioOffset = 0;
		}
	}

	auto autoSound = Configuration::Load("Game", "AutoSound");
	bool IsAutoSound = false;
	if (autoSound.size() > 0) {
		try {
			IsAutoSound = std::atoi(autoSound.c_str()) == 1;
		}

		catch (std::invalid_argument e) {
			std::cout << "Game.ini::AutoSound invalid value: " << autoSound << " reverting to 0 value" << std::endl;
			IsAutoSound = false;
		}
	}

	if (Configuration::Load("Gameplay", "Notespeed").size() > 0) {
		m_scrollSpeed = std::atoi(Configuration::Load("Gameplay", "Notespeed").c_str());
	}

	if (EnvironmentSetup::Get("SongRate").size() > 0) {
		m_rate = std::stod(EnvironmentSetup::Get("SongRate").c_str());
		m_rate = std::clamp(m_rate, 0.5, 2.0);
	}

	m_title = chart->m_title;
	if (m_rate != 1.0) {
#if defined(WIN32)
		int widelen = MultiByteToWideChar(CP_UTF8, 0, (char*)m_title.c_str(), -1, NULL, 0);
		wchar_t* widestr = new wchar_t[widelen];
		MultiByteToWideChar(CP_UTF8, 0, (char*)m_title.c_str(), -1, widestr, widelen);

		// format [%.2f] <title>
		wchar_t buffer[256];
		swprintf_s(buffer, L"[%.2fx] %s", m_rate, widestr);

		// convert back to utf8
		int mblen = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
		char8_t* mbstr = new char8_t[mblen];

		WideCharToMultiByte(CP_UTF8, 0, buffer, -1, (char*)mbstr, mblen, NULL, NULL);

		m_title = mbstr;
		delete[] widestr;
		delete[] mbstr;
#endif
	}

	m_beatmapOffset = chart->m_bpms[0].StartTime;
	m_audioLength = chart->GetLength();
	m_baseBPM = chart->BaseBPM;
	m_currentBPM = m_baseBPM;
	m_currentSVMultiplier = chart->InitialSvMultiplier;

	GameAudioSampleCache::SetRate(m_rate);
	GameAudioSampleCache::Load(chart, Configuration::Load("Game", "AudioPitch") == "1");
	
	CreateTimingMarkers();
	UpdateVirtualResolution();

	for (auto& note : chart->m_notes) {
		NoteInfoDesc desc = {};
		desc.ImageType = Key2Type[note.LaneIndex];
		desc.ImageBodyType = Key2HoldType[note.LaneIndex];
		desc.StartTime = note.StartTime;
		desc.Lane = note.LaneIndex;
		desc.Type = note.Type;
		desc.EndTime = -1;
		desc.InitialTrackPosition = GetPositionFromOffset(note.StartTime);
		desc.EndTrackPosition = -1;
		desc.KeysoundIndex = note.Keysound;
		desc.StartBPM = GetBPMAt(note.StartTime);
		desc.Volume = note.Volume * m_audioVolume;
		desc.Pan = note.Pan * m_audioVolume;

		if (note.Type == NoteType::HOLD) {
			desc.EndTime = note.EndTime;
			desc.EndTrackPosition = GetPositionFromOffset(note.EndTime);
			desc.EndBPM = GetBPMAt(note.EndTime);
		}

		if ((m_audioOffset != 0 && desc.KeysoundIndex != -1) || IsAutoSound) {
			AutoSample newSample = {};
			newSample.StartTime = desc.StartTime;
			newSample.Pan = note.Pan;
			newSample.Volume = note.Volume;
			newSample.Index = desc.KeysoundIndex;

			m_autoSamples.push_back(newSample);
			desc.KeysoundIndex = -1;
		}

		m_noteDescs.push_back(desc);
	}

	std::sort(m_noteDescs.begin(), m_noteDescs.end(), [](auto& a, auto& b) {
		if (a.StartTime != b.StartTime) {
			return a.StartTime < b.StartTime;
		}
		else {
			return a.EndTime < b.EndTime;
		}
	});

	std::sort(m_autoSamples.begin(), m_autoSamples.end(), [](const AutoSample& a, const AutoSample& b) {
		return a.StartTime < b.StartTime;
	});

	UpdateGamePosition();
	UpdateNotes();

	m_timingLineManager = chart->m_customMeasures.size() > 0 ? new TimingLineManager(this, chart->m_customMeasures) : new TimingLineManager(this);
	m_scoreManager = new ScoreManager();

	m_timingLineManager->Init();
	m_state = GameState::NotGame;
	return true;
}

void RhythmEngine::SetKeys(Keys* keys) {
	for (int i = 0; i < 7; i++) {
		KeyMapping[i].key = keys[i];
	}
}

bool RhythmEngine::Start() { // no, use update event instead
	m_currentAudioPosition -= 3000;
	m_state = GameState::Playing;

	m_startClock = std::chrono::system_clock::now();
	return true;
}

bool RhythmEngine::Stop() { 
	return true;
}

bool RhythmEngine::Ready() {
	return m_state == GameState::NotGame;
}

void RhythmEngine::Update(double delta) {
	if (m_state == GameState::NotGame || m_state == GameState::PosGame) return;

	// Since I'm coming from Roblox, and I had no idea how to Real-Time sync the audio
	// I decided to use this method again from Roblox project I did in past.
	m_currentAudioPosition += (delta * m_rate) * 1000;

	if (m_currentAudioPosition > m_audioLength + 6000) { // Avoid game ended too early
		m_state = GameState::PosGame;
		::printf("Audio stopped!\n");
	}

	UpdateVirtualResolution();
	UpdateGamePosition();
	UpdateNotes();

	m_timingLineManager->Update(delta);

	for (auto& it : m_tracks) {
		it->Update(delta);
	}

	// Sample event updates
	for (int i = m_currentSampleIndex; i < m_autoSamples.size(); i++) {
		auto& sample = m_autoSamples[i];
		if (m_currentAudioPosition >= sample.StartTime) {
			if (sample.StartTime - m_currentAudioPosition < 5) {
				GameAudioSampleCache::Play(sample.Index, sample.Volume * m_audioVolume, sample.Pan * 100);
			}

			m_currentSampleIndex++;
		}
		else {
			break;
		}
	}

	auto currentTime = std::chrono::system_clock::now();
	auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_startClock);
	m_PlayTime = static_cast<int>(elapsedTime.count() / 1000);
}

void RhythmEngine::Render(double delta) {
	if (m_state == GameState::NotGame || m_state == GameState::PosGame) return;
	
	m_timingLineManager->Render(delta);

	for (auto& it : m_tracks) {
		it->Render(delta);
	}
}

void RhythmEngine::Input(double delta) {
	if (m_state == GameState::NotGame || m_state == GameState::PosGame) return;

	// Autoplay updates
	for (int i = 0; i < 7; i++) {
		int& index = m_autoHitIndex[i];

		if (index < m_autoHitInfos[i].size()
			&& m_currentAudioGamePosition >= m_autoHitInfos[i][index].Time) {

			auto& info = m_autoHitInfos[i][index];

			if (info.Type == ReplayHitType::KEY_DOWN) {
				m_tracks[i]->OnKeyDown();
			}
			else {
				m_tracks[i]->OnKeyUp();
			}

			index++;
		}
	}
}

void RhythmEngine::OnKeyDown(const KeyState& state) {
	if (m_state == GameState::NotGame || m_state == GameState::PosGame) return;

	if (state.key == Keys::F3) {
		m_scrollSpeed -= 10;
		std::cout << "Decrease Note Speed" << std::endl;
	}
	else if (state.key == Keys::F4) {
		m_scrollSpeed += 10;
		std::cout << "Increase Note Speed" << std::endl;
	}

	for (auto& key : KeyMapping) {
		if (key.second.key == state.key) {
			key.second.isPressed = true;

			if (key.first < m_tracks.size()) {
				m_tracks[key.first]->OnKeyDown();
			}
		}
	}
}

void RhythmEngine::OnKeyUp(const KeyState& state) {
	if (m_state == GameState::NotGame || m_state == GameState::PosGame) return;

	for (auto& key : KeyMapping) {
		if (key.second.key == state.key) {
			key.second.isPressed = false;

			if (key.first < m_tracks.size()) {
				m_tracks[key.first]->OnKeyUp();
			}
		}
	}
}

void RhythmEngine::ListenKeyEvent(std::function<void(GameTrackEvent)> callback) {
	m_eventCallback = callback;
}

double RhythmEngine::GetAudioPosition() const {
	return m_currentAudioPosition;
}

double RhythmEngine::GetVisualPosition() const {
	return m_currentVisualPosition;
}

double RhythmEngine::GetGameAudioPosition() const {
	return m_currentAudioGamePosition;
}

double RhythmEngine::GetTrackPosition() const {
	return m_currentTrackPosition;
}

double RhythmEngine::GetPrebufferTiming() const {
	return -300000.0 / GetNotespeed();
}

double RhythmEngine::GetNotespeed() const {
	double speed = static_cast<double>(m_scrollSpeed);
	double scrollingFactor = 1920.0 / 1366.0;
	float virtualRatio = m_virtualResolution.Y / m_gameResolution.Y;
	float value = (speed / 10.0) / (20.0 * m_rate) * scrollingFactor * virtualRatio;

	return value;
}

double RhythmEngine::GetBPMAt(double offset) const {
	auto& bpms = m_currentChart->m_bpms;
	int min = 0, max = bpms.size() - 1;

	if (max == 0) {
		return bpms[0].Value;
	}

	while (min <= max) {
		int mid = (min + max) / 2;

		bool afterMid = mid < 0 || bpms[mid].StartTime <= offset;
		bool beforeMid = mid + 1 >= bpms.size() || bpms[mid + 1].StartTime > offset;

		if (afterMid && beforeMid) {
			return bpms[mid].Value;
		}
		else if (afterMid) {
			max = mid - 1;
		}
		else {
			min = mid + 1;
		}
	}

	return bpms[0].Value;
}

double RhythmEngine::GetCurrentBPM() const {
	return m_currentBPM;
}

double RhythmEngine::GetSongRate() const {
	return m_rate;
}

int RhythmEngine::GetAudioLength() const {
	return m_audioLength;
}

int RhythmEngine::GetGameVolume() const {
	return m_audioVolume;
}

GameState RhythmEngine::GetState() const {
	return m_state;
}

ScoreManager* RhythmEngine::GetScoreManager() const {
	return m_scoreManager;
}

std::vector<double> RhythmEngine::GetTimingWindow() {
	float ratio = std::clamp(2.0f - m_currentSVMultiplier, 0.3f, 2.0f);
	
	return { kNoteCoolHitWindowMax * ratio, kNoteGoodHitWindowMax * ratio, kNoteBadHitWindowMax * ratio, kNoteEarlyMissWindowMin * ratio };
}

std::vector<TimingInfo> RhythmEngine::GetBPMs() const {
	return m_currentChart->m_bpms;
}

std::vector<TimingInfo> RhythmEngine::GetSVs() const {
	return m_currentChart->m_svs;
}

double RhythmEngine::GetElapsedTime() const { // Get game frame
	return static_cast<double>(SDL_GetTicks()) / 1000.0;
}

int RhythmEngine::GetPlayTime() const { // Get game time
	return m_PlayTime;
}

int RhythmEngine::GetGuideLineIndex() const {
	return m_guideLineIndex;
}

void RhythmEngine::SetGuideLineIndex(int idx) {
	m_guideLineIndex = idx;
}

void RhythmEngine::UpdateNotes() {
	for (int i = m_currentNoteIndex; i < m_noteDescs.size(); i++) {
		auto& desc = m_noteDescs[i];
		
		if (m_currentAudioGamePosition + (3000.0 / GetNotespeed()) > desc.StartTime
			|| (m_currentTrackPosition - desc.InitialTrackPosition > GetPrebufferTiming())) {

			m_tracks[desc.Lane]->AddNote(&desc);

			m_currentNoteIndex += 1;
		}
		else {
			break;
		}
	}
}

void RhythmEngine::UpdateGamePosition() {
	m_currentAudioGamePosition = m_currentAudioPosition + m_offset;
	m_currentVisualPosition = m_currentAudioGamePosition;// * m_rate;

	while (m_currentBPMIndex + 1 < m_currentChart->m_bpms.size() && m_currentVisualPosition >= m_currentChart->m_bpms[m_currentBPMIndex + 1].StartTime) {
		m_currentBPMIndex += 1;
	}

	while (m_currentSVIndex < m_currentChart->m_svs.size() && m_currentVisualPosition >= m_currentChart->m_svs[m_currentSVIndex].StartTime) {
		m_currentSVIndex += 1;
	}

	m_currentTrackPosition = GetPositionFromOffset(m_currentVisualPosition, m_currentSVIndex);

	if (m_currentSVIndex > 0) {
		float svMultiplier = m_currentChart->m_svs[m_currentSVIndex - 1].Value;
		if (svMultiplier != m_currentSVMultiplier) {
			m_currentSVMultiplier = svMultiplier;
		}
	}

	if (m_currentBPMIndex > 0) {
		m_currentBPM = m_currentChart->m_bpms[m_currentBPMIndex - 1].Value;
	}
}

void RhythmEngine::UpdateVirtualResolution() {
	double width = Window::GetInstance()->GetBufferHeight();
	double height = Window::GetInstance()->GetBufferHeight();

	m_gameResolution = { width, height };

	float ratio = (float)width / (float)height;
	if (ratio >= 16.0f / 9.0f) {
		m_virtualResolution = { width * ratio, height };
	}
	else {
		m_virtualResolution = { width, height / ratio };
	}
}

void RhythmEngine::CreateTimingMarkers() {
	if (m_currentChart->m_svs.size() > 0) {
		auto& svs = m_currentChart->m_svs;
		double pos = std::round(svs[0].StartTime * m_currentChart->InitialSvMultiplier * 100);
		m_timingPositionMarkers.push_back(pos);

		for (int i = 1; i < svs.size(); i++) {
			pos += std::round((svs[i].StartTime - svs[i - 1].StartTime) * (svs[i - 1].Value * 100));

			m_timingPositionMarkers.push_back(pos);
		}
	}
}

double RhythmEngine::GetPositionFromOffset(double offset) {
	int index;

	for (index = 0; index < m_currentChart->m_svs.size(); index++) {
		if (offset < m_currentChart->m_svs[index].StartTime) {
			break;
		}
	}

	return GetPositionFromOffset(offset, index);
}

double RhythmEngine::GetPositionFromOffset(double offset, int index) {
	if (index == 0) {
		return offset * m_currentChart->InitialSvMultiplier * 100;
	}

	index -= 1;

	double pos = m_timingPositionMarkers[index];
	pos += (offset - m_currentChart->m_svs[index].StartTime) * (m_currentChart->m_svs[index].Value * 100);

	return pos;
}

int* RhythmEngine::GetLaneSizes() const {
	return (int*)&m_laneSize;
}

int* RhythmEngine::GetLanePos() const {
	return (int*)&m_lanePos;
}

void RhythmEngine::SetHitPosition(int offset) {
	m_hitPosition = offset;
}

void RhythmEngine::SetLaneOffset(int offset) {
	m_laneOffset = offset;
}

int RhythmEngine::GetHitPosition() const {
	return m_hitPosition;
}

Vector2 RhythmEngine::GetResolution() const {
	return m_gameResolution;
}

Rect RhythmEngine::GetPlayRectangle() const {
	return m_playRectangle;
}

std::u8string RhythmEngine::GetTitle() const {
	return m_title;
}

void RhythmEngine::Release() {
	for (int i = 0; i < m_tracks.size(); i++) {
		delete m_tracks[i];
	}

	delete m_timingLineManager;
	NoteImageCacheManager::Release();
	GameAudioSampleCache::StopAll();
}
