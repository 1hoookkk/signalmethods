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
    L.timelineOpen = (L.room == Room::edit || L.room == Room::sound) && (! tl.keys.empty() || tl.playing);
    L.compute ((float) getWidth(), (float) getHeight());
    view.rect = L.field;
    buildTray();
    keys.clear();
    const float kh = 14.0f;
    keys.push_back ({ "room0", "FRAMES", { 8.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::frames });
    keys.push_back ({ "room3", "MORPH", { 72.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::morph });
    keys.push_back ({ "room1", "EDIT", { 136.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::edit });
    keys.push_back ({ "room2", "READ", { 200.0f, L.rooms.getY() + 5.0f, 60.0f, kh }, L.room == Room::sound });
    const bool audioOn = audio != nullptr && audio->isPlaying();
    keys.push_back ({ "listen", audioOn ? "STOP" : "LISTEN", { L.resp.getX() + 8.0f, L.resp.getY() + 6.0f, 60.0f, kh }, audioOn });
    keys.push_back ({ "wet", "FILTER", { L.resp.getX() + 72.0f, L.resp.getY() + 6.0f, 60.0f, kh }, wet });
    if (L.room != Room::sound) keys.push_back ({ "source", analyse ? "NOISE" : sourceSample ? "SAMPLE" : "SAW", { L.resp.getX() + 136.0f, L.resp.getY() + 6.0f, 64.0f, kh }, sourceSample });
    keys.push_back ({ "analyse", "ANALYSE", { L.resp.getX() + 204.0f, L.resp.getY() + 6.0f, 72.0f, kh }, analyse });
    if (L.room == Room::frames)
    {
        keys.push_back ({ "sortRoot", "LOW > HIGH", { L.sortRow.getX() + 8.0f, 5.0f, 84.0f, kh }, ! sortNear });
        const juce::String like = body.corner[0] >= 0 ? "LIKE " + lib.frames[(size_t) body.corner[0]].name.substring (0, 14) : juce::String ("LIKE M0 Q0");
        keys.push_back ({ "sortNear", like, { L.sortRow.getX() + 96.0f, 5.0f, 170.0f, kh }, sortNear });
        keys.push_back ({ "take", "TAKE", { L.field.getX() + 8.0f, L.field.getBottom() - 22.0f, 60.0f, kh }, false });
        for (int i = 0; i < 4; ++i)
        {
            const float sx = L.field.getX() + 84.0f + i * 100.0f;
            keys.push_back ({ "slot" + juce::String (i), kCornerNames[i], { sx, L.field.getBottom() - 22.0f, 92.0f, kh }, selectedSlot == i });
        }
        keys.push_back ({ "room3", "MORPH ROOM", { L.field.getRight() - 96.0f, L.field.getBottom() - 22.0f, 88.0f, kh }, false });
    }
    else if (L.room == Room::morph)
    {
        const auto r = padRect();
        keys.push_back ({ "fine", "FINE", { r.getX(), r.getBottom() + 44.0f, 52.0f, kh }, fine });
        keys.push_back ({ "sweep0", "SWEEP MORPH", { r.getX() + 60.0f, r.getBottom() + 44.0f, 96.0f, kh }, sweep[0] });
        keys.push_back ({ "sweep1", "SWEEP Q", { r.getX() + 164.0f, r.getBottom() + 44.0f, 72.0f, kh }, sweep[1] });
        keys.push_back ({ "compare", "COMPARE", { r.getX() + 244.0f, r.getBottom() + 44.0f, 76.0f, kh }, compare });
        keys.push_back ({ "capture", "CAPTURE", { r.getRight() - 216.0f, r.getBottom() + 44.0f, 68.0f, kh }, false });
        keys.push_back ({ "export", "EXPORT", { r.getRight() - 144.0f, r.getBottom() + 44.0f, 68.0f, kh }, false });
        keys.push_back ({ "edit", "EDIT", { r.getRight() - 72.0f, r.getBottom() + 44.0f, 72.0f, kh }, false });
        for (int i = 0; i < 4; ++i)
        {
            const bool right = (i & 1) != 0, top = (i & 2) != 0;
            keys.push_back ({ "goto" + juce::String (i), kCornerNames[i], { right ? r.getRight() - 52.0f : r.getX(), top ? r.getY() - 36.0f : r.getBottom() + 22.0f, 52.0f, kh }, false });
        }
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
    if (L.room == Room::edit || L.room == Room::sound) keys.push_back ({ "addkey", "+ KEY", { L.tlAx.getRight() - 60.0f, L.tl.getY() + 3.0f, 60.0f, kh }, false });
}

void Workstation::press (const juce::String& id)
{
    if (id == "room0") setRoom (Room::frames);
    else if (id == "room3") { if (body.corner[0] < 0 && body.corner[1] < 0 && body.corner[2] < 0 && body.corner[3] < 0) { if (playFrame >= 0) { body.corner[0] = playFrame; fillCorners(); } else { status = "choose a frame first"; redraw(); return; } } setRoom (Room::morph); }
    else if (id == "sortRoot") { sortNear = false; stripDirty = true; }
    else if (id == "sortNear") { sortNear = true; stripDirty = true; }
    else if (id.startsWith ("group")) { const int g = id.substring (5).getIntValue(); open[(size_t) g] = ! open[(size_t) g]; stripDirty = true; }
    else if (id.startsWith ("slot")) { const int k = id.substring (4).getIntValue(); selectedSlot = selectedSlot == k ? -1 : k; status = selectedSlot >= 0 ? juce::String ("TAKE fills ") + kCornerNames[k] : juce::String(); }
    else if (id == "take") takeScan();
    else if (id == "source") { sourceSample = ! sourceSample; if (audio) audio->useClip (sourceSample); }
    else if (id == "analyse") { analyse = ! analyse; if (audio) { audio->useNoise (analyse); audio->useClip (! analyse && sourceSample); } if (! analyse) measured.clear(); }
    else if (id.startsWith ("goto") && L.room == Room::morph) { selectedSlot = id.substring (4).getIntValue(); setRoom (Room::frames); status = juce::String ("choose a frame for ") + kCornerNames[selectedSlot]; }
    else if (id == "fine") fine = ! fine;
    else if (id == "compare") {}
    else if (id == "room1") { if (body.corner[(size_t) editCorner] >= 0) openEditor (editCorner); else status = "pick a corner first"; }
    else if (id == "room2") setRoom (Room::sound);
    else if (id.startsWith ("sweep")) { const int k = id.substring (5).getIntValue(); sweep[(size_t) k] = ! sweep[(size_t) k]; if (sweep[0] || sweep[1] || sweep[2]) startTimerHz (60); }
    else if (id == "capture" && L.room == Room::morph)
    {
        if (! body.ready()) status = "nothing to capture";
        else
        {
            juce::String name = "cap " + lib.frames[(size_t) body.corner[0]].name.substring (0, 14) + " " + juce::String (body.morph, 2) + "/" + juce::String (body.q, 2);
            const int idx = lib.addNamed (body.wheelWords (lib.frames), name, kGroups - 1, true);
            stripDirty = true;
            status = "captured " + lib.frames[(size_t) idx].name;
        }
    }
    else if (id == "home") { view.az = -37.5; view.el = 30.0; view.zoom = 1.0; view.panX = view.panY = 0.0; }
    else if (id == "pair") { pairMode = ! pairMode; pairA = pairB = -1; pairT = 0.0; status = pairMode ? "pick two nodes" : ""; }
    else if (id == "capture") capture();
    else if (id.startsWith ("corner")) { assignCorner (id.substring (6).getIntValue()); chosen[(size_t) id.substring (6).getIntValue()] = true; fillCorners(); }
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
