#pragma once

#include <juce_opengl/juce_opengl.h>
#include "Scene.h"
#include <array>
#include <map>
#include <memory>

namespace ws
{
class GLRenderer
{
public:
    void create (juce::OpenGLContext& ctx);
    void destroy();
    void draw (const std::vector<Batch>& batches, float width, float height, float scale, juce::Colour ground);

private:
    struct TexVertex { float x, y, u, v, r, g, b, a; };

    struct Atlas { unsigned int tex = 0; float cellW = 0.0f, cellH = 0.0f; int width = 0; std::array<float, 224> advance {}; };

    std::unique_ptr<juce::OpenGLShaderProgram> marks, textured;
    unsigned int vbo = 0, picture = 0;
    std::map<juce::String, Atlas> atlases;
    juce::Image pictureSource;

    const Atlas& atlasFor (bool monospace, float size, float scale);
    void upload (unsigned int tex, const juce::Image& img);
    void drawSolid (const Batch& b, float scale);
    void drawTextured (const std::vector<TexVertex>& quads, unsigned int tex, bool coverage, float width, float height, float scale);
    void drawText (const Batch& b, float width, float height, float scale);
    void drawImage (const Batch& b, float width, float height, float scale);
};
}
