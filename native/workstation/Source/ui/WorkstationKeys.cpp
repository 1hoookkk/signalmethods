#include "Workstation.h"
#include "Style.h"
#include <cmath>

namespace ws
{
void Workstation::buildTray()
{
    trayRows.clear();
    if (L.room == Room::sound) return;
    for (int g = 0; g < kGroups; ++g)
    {
        trayRows.push_back ({ -1, g, true });
        if (! open[(size_t) g]) continue;
        for (int i = 0; i < (int) lib.frames.size(); ++i)
            if (lib.frames[(size_t) i].group == g) trayRows.push_back ({ i, g, false });
    }
}

void Workstation::layoutKeys()
{
    L.timelineOpen = ! tl.keys.empty() || tl.playing;
    L.compute ((float) getWidth(), (float) getHeight());
    view.rect = L.field;
    buildTray();
    keys.clear();
    const float kh = 14.0f;
    keys.push_back ({ "room0", "FRAMES", { 8.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::frames });
    keys.push_back ({ "room1", "EDIT", { 72.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::edit });
    keys.push_back ({ "room2", "SOUND", { 136.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::sound });
    const bool audioOn = audio != nullptr && audio->isPlaying();
    keys.push_back ({ "listen", audioOn ? "STOP" : "LISTEN", { L.resp.getX() + 8.0f, L.resp.getY() + 6.0f, 60.0f, kh }, audioOn });
    keys.push_back ({ "wet", "FILTER", { L.resp.getX() + 72.0f, L.resp.getY() + 6.0f, 60.0f, kh }, wet });
    if (L.room == Room::frames)
    {
        keys.push_back ({ "home", "HOME", { L.sortRow.getX() + 8.0f, 5.0f, 52.0f, kh }, false });
        keys.push_back ({ "pair", "PAIR", { L.field.getRight() - 56.0f, 5.0f, 56.0f, kh }, pairMode });
        keys.push_back ({ "capture", "CAPTURE", { L.field.getRight() - 128.0f, 5.0f, 68.0f, kh }, false });
        const auto sq = L.outer;
        for (int i = 0; i < 4; ++i)
        {
            const float x = (i & 1) ? sq.getRight() + 8.0f : sq.getX() - 56.0f, y = (i & 2) ? sq.getY() : sq.getBottom() - kh;
            keys.push_back ({ "corner" + juce::String (i), kCornerNames[i], { x, y, 48.0f, kh }, body.corner[(size_t) i] >= 0 });
        }
        const float bx = sq.getX(), by = sq.getBottom() + 30.0f;
        for (int s = 0; s < kRows; ++s) keys.push_back ({ "row" + juce::String (s), juce::String (s + 1), { bx + 40.0f + s * 24.0f, by, 20.0f, kh }, body.rowOn[(size_t) s] });
        keys.push_back ({ "playbody", "BODY", { bx, by + 24.0f, 68.0f, kh }, playBody });
        keys.push_back ({ "edit", "EDIT", { bx + 72.0f, by + 24.0f, 68.0f, kh }, false });
        keys.push_back ({ "export", "EXPORT", { bx, by + 48.0f, 68.0f, kh }, false });
        keys.push_back ({ "copy", "COPY", { bx + 72.0f, by + 48.0f, 68.0f, kh }, copyFrom >= 0 });
        keys.push_back ({ "sharpen", "SHARPEN", { bx, by + 72.0f, 68.0f, kh }, false });
        keys.push_back ({ "unity", "UNITY", { bx + 72.0f, by + 72.0f, 68.0f, kh }, body.unity });
    }
    else if (L.room == Room::edit)
    {
        for (int s = 0; s < kRows; ++s)
        {
            const auto r = L.stageRect (s);
            keys.push_back ({ "row" + juce::String (s), juce::String (s + 1), { r.getX(), r.getY() - 18.0f, 20.0f, kh }, body.rowOn[(size_t) s] });
            keys.push_back ({ "lock" + juce::String (s), "LOCK", { r.getRight() - 48.0f, r.getBottom() + 48.0f, 48.0f, kh }, lockRow[(size_t) s] });
        }
        keys.push_back ({ "ceiling", "CEILING", { L.stageRect (kRows - 1).getRight() - 116.0f, L.stageRect (kRows - 1).getBottom() + 48.0f, 60.0f, kh }, false });
        for (int i = 0; i < 4; ++i) keys.push_back ({ "goto" + juce::String (i), kCornerNames[i], { L.thumbRect (i).getX(), L.thumbRect (i).getY() - 32.0f, 52.0f, kh }, editing == i });
    }
    else
    {
        keys.push_back ({ "speech", "SPEECH", { L.sortRow.getX() + 8.0f, 5.0f, 60.0f, kh }, sound.speech });
        keys.push_back ({ "bells", "BELLS", { L.sortRow.getX() + 72.0f, 5.0f, 56.0f, kh }, ! sound.speech });
        keys.push_back ({ "frame", "FRAME", { L.field.getRight() - 60.0f, 5.0f, 60.0f, kh }, false });
    }
    if (L.timelineOpen)
    {
        keys.push_back ({ "play", tl.playing ? "STOP" : "PLAY", { L.tlAx.getRight() - 188.0f, L.tl.getY() + 3.0f, 60.0f, kh }, tl.playing });
        keys.push_back ({ "loop", "LOOP", { L.tlAx.getRight() - 124.0f, L.tl.getY() + 3.0f, 60.0f, kh }, tl.loop });
    }
    keys.push_back ({ "addkey", "+ KEY", { L.tlAx.getRight() - 60.0f, L.tl.getY() + 3.0f, 60.0f, kh }, false });
}

void Workstation::press (const juce::String& id)
{
    if (id == "room0") setRoom (Room::frames);
    else if (id == "room1") { if (body.corner[(size_t) editCorner] >= 0) openEditor (editCorner); else status = "pick a corner first"; }
    else if (id == "room2") setRoom (Room::sound);
    else if (id == "home") { view.az = -37.5; view.el = 30.0; view.zoom = 1.0; view.panX = view.panY = 0.0; }
    else if (id == "pair") { pairMode = ! pairMode; pairA = pairB = -1; pairT = 0.0; status = pairMode ? "pick two nodes" : ""; }
    else if (id == "capture") capture();
    else if (id.startsWith ("corner")) assignCorner (id.substring (6).getIntValue());
    else if (id.startsWith ("goto")) openEditor (id.substring (4).getIntValue());
    else if (id.startsWith ("row")) { const int s = id.substring (3).getIntValue(); body.rowOn[(size_t) s] = ! body.rowOn[(size_t) s]; }
    else if (id == "playbody") playBody = ! playBody;
    else if (id == "edit") { if (body.corner[(size_t) editCorner] >= 0) openEditor (editCorner); else status = "pick a corner first"; }
    else if (id == "export") exportBody (exportDir.getChildFile ("ws_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".body240"));
    else if (id == "copy") { copyFrom = copyFrom >= 0 ? -1 : editCorner; status = copyFrom >= 0 ? "click the corner to paste into" : ""; }
    else if (id == "sharpen") sharpenQ();
    else if (id == "unity") body.unity = ! body.unity;
    else if (id == "ceiling") ceiling();
    else if (id.startsWith ("lock")) { const int s = id.substring (4).getIntValue(); lockRow[(size_t) s] = ! lockRow[(size_t) s]; }
    else if (id == "speech") sound.speech = true;
    else if (id == "bells") sound.speech = false;
    else if (id == "frame") frameFromSlice();
    else if (id == "listen")
    {
        if (audio == nullptr) status = "no audio device";
        else if (L.room == Room::sound && sound.mono->empty()) status = "load a sound first";
        else { audio->setPlaying (! audio->isPlaying()); if (audio->isPlaying()) startTimerHz (30); else if (! tl.playing) stopTimer(); }
    }
    else if (id == "wet") { wet = ! wet; if (audio) audio->setWet (wet); }
    else if (id == "play")
    {
        tl.playing = ! tl.playing;
        if (tl.playing) { playT0 = juce::Time::getMillisecondCounterHiRes(); playFrom = tl.playhead >= tl.duration ? 0.0 : tl.playhead; startTimerHz (60); }
        else if (audio == nullptr || ! audio->isPlaying()) stopTimer();
    }
    else if (id == "loop") tl.loop = ! tl.loop;
    else if (id == "addkey")
    {
        if (spot) tl.keys.push_back ({ tl.playhead, *spot });
        else status = "press the surface first";
    }
    redraw();
}
}
