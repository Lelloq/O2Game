#include "TimingLineManager.hpp"
#include "RhythmEngine.hpp"

TimingLineManager::TimingLineManager(RhythmEngine* engine) {
	Rect playRect = engine->GetPlayRectangle();
	m_engine = engine;
	m_timingLines = {};
	m_timingInfos = {};

	int mapLength = engine->GetAudioLength();
	auto bpms = engine->GetBPMs();

	for (int i = 0; i < bpms.size(); i++) {
		double beatTime = bpms[i].StartTime;
		double timeEnd = mapLength - 1;

		if ((size_t)(i + 1) < bpms.size()) {
			timeEnd = bpms[(size_t)(i + 1)].StartTime - 1;
		}

		while (beatTime < timeEnd) {
			double offset = engine->GetPositionFromOffset(beatTime);
			
			TimingLineDesc desc = {};
			desc.Engine = engine;
			desc.StartTime = beatTime;
			desc.Offset = offset;
			desc.ImagePos = playRect.left;
			desc.ImageSize = playRect.right;

			m_timingInfos.push(desc);

			beatTime += (60000.0 / bpms[i].Value) * bpms[i].TimeSignature;
		}
	}
}

TimingLineManager::TimingLineManager(RhythmEngine* engine, std::vector<double> list) {
	Rect playRect = engine->GetPlayRectangle();

	m_engine = engine;
	m_timingLines = {};
	m_timingInfos = {};

	for (int i = 0; i < list.size(); i++) {
		double offset = m_engine->GetPositionFromOffset(list[i]);

		TimingLineDesc desc = {};
		desc.Engine = engine;
		desc.StartTime = list[i];
		desc.Offset = offset;
		desc.ImagePos = playRect.left;
		desc.ImageSize = playRect.right;
		
		m_timingInfos.push(desc);
	}
}

TimingLineManager::~TimingLineManager() {
	for (int i = 0; i < m_timingInfos.size(); i++) {
		m_timingInfos.pop();
	}

	for (int i = 0; i < m_timingLines.size(); i++) {
		delete m_timingLines.front();
		m_timingLines.pop();
	}
}

void TimingLineManager::Init() {
	double recycle = (300000.0 / 4.0) / m_engine->GetNotespeed();

	std::sort(m_timingInfos.begin(), m_timingInfos.end(), [](const auto& a, const auto& b) {
		return a.Offset < b.Offset;
	});

	while (m_timingInfos.size() > 0) {
		auto& info = m_timingInfos.front();

		bool IsA = m_engine->GetTrackPosition() - info.Offset > m_engine->GetPrebufferTiming();
		
		if (IsA) {
			TimingLine* t = new TimingLine();
			t->Load(&info);
			
			m_timingLines.push(t);
			m_timingInfos.pop();
		}
		else {
			break;
		}
	}
}

void TimingLineManager::Update(double delta) {
	double recycle = (300000.0 / 4) / m_engine->GetNotespeed();

	if (m_timingLines.size() > 0) {
		for (auto it = m_timingLines.begin(); it != m_timingLines.end(); it++) {
			(*it)->Update(delta);
		}
	}

	while (m_timingLines.size() > 0
		&& m_timingLines.front()->GetTrackPosition() > recycle
		&& m_timingLines.front()->GetStartTime() < m_engine->GetGameAudioPosition()) {
		TimingLine* t = m_timingLines.front();
		m_timingLines.pop();

		if (m_timingInfos.size() > 0) {
			TimingLineDesc desc = m_timingInfos.front();
			m_timingInfos.pop();

			t->Load(&desc);
			m_timingLines.push(t);
		}
		else {
			delete t;
		}
	}

	while (m_timingInfos.size() > 0 
		&& m_engine->GetTrackPosition() - m_timingInfos.front().Offset > m_engine->GetPrebufferTiming()) {
		TimingLine* t = new TimingLine();
		t->Load(&m_timingInfos.front());
		m_timingLines.push(t);
		m_timingInfos.pop();
	}
}

void TimingLineManager::Render(double delta) {
	if (m_timingLines.size() > 0) {
		for (auto it = m_timingLines.begin(); it != m_timingLines.end(); it++) {
			(*it)->Render(delta);
		}
	}
}