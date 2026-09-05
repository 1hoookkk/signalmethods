#pragma once

#include <juce_opengl/juce_opengl.h>
#include "Scene.h"
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

    std::unique_ptr<juce::OpenGLShaderProgram> marks, textured;
    unsigned int vbo = 0, atlas = 0, picture = 0;
    float atlasScale = 0.0f, atlasSize = 0.0f, cellW = 0.0f, cellH = 0.0f;
    int atlasW = 0, atlasH = 0;
    juce::Image pictureSource;

    void buildAtlas (float size, float scale);
    void upload (unsigned int tex, const juce::Image& img);
    void drawSolid (const Batch& b, float scale);
    void drawTextured (const std::vector<TexVertex>& quads, unsigned int tex, bool coverage, float width, float height, float scale);
    void drawText (const Batch& b, float width, float height, float scale);
    void drawImage (const Batch& b, float width, float height, float scale);
};
}
