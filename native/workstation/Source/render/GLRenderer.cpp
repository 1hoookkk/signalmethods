#include "GLRenderer.h"

namespace ws
{
namespace
{
const char* const kVS =
    "attribute vec2 pos; attribute vec4 col; attribute float size; uniform vec2 vp; varying vec4 vCol;"
    "void main() { vec2 n = pos / vp * 2.0 - 1.0; gl_Position = vec4 (n.x, -n.y, 0.0, 1.0); gl_PointSize = size; vCol = col; }";

const char* const kFS =
    "varying vec4 vCol; uniform float isRound;"
    "void main() { if (isRound > 0.5) { vec2 c = gl_PointCoord - vec2 (0.5, 0.5); if (dot (c, c) > 0.25) discard; } gl_FragColor = vCol; }";
}

void GLRenderer::create (juce::OpenGLContext& ctx)
{
    using namespace juce::gl;
    marks = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    marks->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (kVS));
    marks->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (kFS));
    marks->link();
    glGenBuffers (1, &vbo);
}

void GLRenderer::destroy()
{
    using namespace juce::gl;
    if (vbo != 0) glDeleteBuffers (1, &vbo);
    vbo = 0;
    marks.reset();
}

void GLRenderer::draw (const std::vector<Batch>& batches, float width, float height, float scale)
{
    using namespace juce::gl;
    juce::OpenGLHelpers::clear (juce::Colours::black);
    if (marks == nullptr || marks->getProgramID() == 0) return;
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (0x8642);
    marks->use();
    juce::OpenGLShaderProgram::Uniform (*marks, "vp").set (width * scale, height * scale);
    const juce::OpenGLShaderProgram::Uniform isRound (*marks, "isRound");
    const juce::OpenGLShaderProgram::Attribute pos (*marks, "pos"), col (*marks, "col"), size (*marks, "size");
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    for (const auto& b : batches)
    {
        if (b.v.empty()) continue;
        std::vector<Vertex> scaled = b.v;
        for (auto& x : scaled) { x.x *= scale; x.y *= scale; x.size *= scale; }
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (scaled.size() * sizeof (Vertex)), scaled.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer ((GLuint) pos.attributeID, 2, GL_FLOAT, GL_FALSE, sizeof (Vertex), (void*) 0);
        glEnableVertexAttribArray ((GLuint) pos.attributeID);
        glVertexAttribPointer ((GLuint) col.attributeID, 4, GL_FLOAT, GL_FALSE, sizeof (Vertex), (void*) (2 * sizeof (float)));
        glEnableVertexAttribArray ((GLuint) col.attributeID);
        glVertexAttribPointer ((GLuint) size.attributeID, 1, GL_FLOAT, GL_FALSE, sizeof (Vertex), (void*) (6 * sizeof (float)));
        glEnableVertexAttribArray ((GLuint) size.attributeID);
        isRound.set (b.round ? 1.0f : 0.0f);
        if (b.kind == Batch::points) glDrawArrays (GL_POINTS, 0, (GLsizei) scaled.size());
        else
        {
            glLineWidth (scaled[0].size);
            glDrawArrays (b.kind == Batch::lines ? GL_LINES : GL_LINE_STRIP, 0, (GLsizei) scaled.size());
        }
        glDisableVertexAttribArray ((GLuint) pos.attributeID);
        glDisableVertexAttribArray ((GLuint) col.attributeID);
        glDisableVertexAttribArray ((GLuint) size.attributeID);
    }
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
}
