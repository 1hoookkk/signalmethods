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
        {
            const auto& v = view;
            const bool farX = v.farPlane (0), farY = v.farPlane (1);
            const double nearX = farX ? v.lo.x : v.hi.x, nearY = farY ? v.lo.y : v.hi.y, farXv = farX ? v.hi.x : v.lo.x;
            const auto ox = v.project ({ 0.0, nearY, v.lo.z }), oy = v.project ({ nearX, 0.0, v.lo.z }), oz = v.project ({ farXv, nearY, 0.5 });
            const bool left = v.project ({ farXv, nearY, 0.0 }).x < L.field.getCentreX();
            keys.push_back ({ "sweep0", "SWEEP", { ox.x + 34.0f, ox.y + 17.0f, 52.0f, kh }, sweep[0] });
            keys.push_back ({ "sweep1", "SWEEP", { oy.x + 44.0f, oy.y + 17.0f, 52.0f, kh }, sweep[1] });
            keys.push_back ({ "sweep2", "SWEEP", { left ? std::max (L.field.getX() + 4.0f, oz.x - 176.0f) : oz.x + 86.0f, oz.y - 4.0f, 52.0f, kh }, sweep[2] });
        }
        keys.push_back ({ "pair", "PAIR", { L.field.getRight() - 56.0f, 5.0f, 56.0f, kh }, pairMode });
        for (int i = 0; i < 4; ++i)
        {
            const float sx = L.field.getX() + 8.0f + i * 96.0f;
            keys.push_back ({ "slot" + juce::String (i), juce::String ("TAKE ") + kCornerNames[i], { sx, L.field.getBottom() - 22.0f, 88.0f, kh }, body.corner[(size_t) i] >= 0 });
        }
        keys.push_back ({ "toBody", "BODY ROOM", { L.field.getRight() - 96.0f, L.field.getBottom() - 22.0f, 88.0f, kh }, false });
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
    else if (id.startsWith ("sweep")) { const int k = id.substring (5).getIntValue(); sweep[(size_t) k] = ! sweep[(size_t) k]; if (sweep[0] || sweep[1] || sweep[2]) startTimerHz (60); }
    else if (id == "home") { view.az = -37.5; view.el = 30.0; view.zoom = 1.0; view.panX = view.panY = 0.0; }
    else if (id == "pair") { pairMode = ! pairMode; pairA = pairB = -1; pairT = 0.0; status = pairMode ? "pick two nodes" : ""; }
    else if (id == "capture") capture();
    else if (id.startsWith ("corner")) assignCorner (id.substring (6).getIntValue());
    else if (id.startsWith ("slot")) { assignCorner (id.substring (4).getIntValue()); playBody = false; }
    else if (id == "toBody") { if (body.corner[(size_t) editCorner] >= 0) openEditor (editCorner); else status = "take a corner first"; }
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
        else { audio->setPlaying (! audio->isPlaying()); status = audio->isPlaying() ? audio->deviceInfo() : ""; if (audio->isPlaying()) startTimerHz (30); else if (! tl.playing) stopTimer(); }
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
