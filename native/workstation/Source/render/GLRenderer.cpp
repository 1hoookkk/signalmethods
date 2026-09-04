#include "GLRenderer.h"

namespace ws
{
namespace
{
const char* const kMarksVS =
    "attribute vec2 pos; attribute vec4 col; attribute float size; uniform vec2 vp; varying vec4 vCol;"
    "void main() { vec2 n = pos / vp * 2.0 - 1.0; gl_Position = vec4 (n.x, -n.y, 0.0, 1.0); gl_PointSize = size; vCol = col; }";

const char* const kMarksFS =
    "varying vec4 vCol; uniform float isRound;"
    "void main() { if (isRound > 0.5) { vec2 c = gl_PointCoord - vec2 (0.5, 0.5); if (dot (c, c) > 0.25) discard; } gl_FragColor = vCol; }";

const char* const kSurfaceVS =
    "attribute vec2 pos; attribute vec3 bary; attribute vec3 idx; uniform vec2 vp; varying vec3 vBary; varying vec3 vIdx;"
    "void main() { vec2 n = pos / vp * 2.0 - 1.0; gl_Position = vec4 (n.x, -n.y, 0.0, 1.0); vBary = bary; vIdx = idx; }";

const char* const kSurfaceFS =
    "varying vec3 vBary; varying vec3 vIdx; uniform sampler2D words; uniform float rows; uniform float w; uniform float cw; uniform float sw; uniform float c2w; uniform float s2w;"
    "float decode (float word) {"
    "  float u = word + 1.0;"
    "  if (u >= 65535.5) return 1.0;"
    "  if (u <= 1.5) return 0.0;"
    "  float e = floor (u / 4096.0); float m = u - e * 4096.0;"
    "  float x = e < 0.5 ? m / 4096.0 : (m + 4096.0) / 8192.0;"
    "  return x * exp2 (e - 15.0);"
    "}"
    "float wordAt (float k) {"
    "  float a = texture2D (words, vec2 ((k + 0.5) / 30.0, (vIdx.x + 0.5) / rows)).r;"
    "  float b = texture2D (words, vec2 ((k + 0.5) / 30.0, (vIdx.y + 0.5) / rows)).r;"
    "  float c = texture2D (words, vec2 ((k + 0.5) / 30.0, (vIdx.z + 0.5) / rows)).r;"
    "  return floor (a * vBary.x + b * vBary.y + c * vBary.z);"
    "}"
    "void main() {"
    "  float db = 0.0;"
    "  for (int s = 0; s < 6; s++) {"
    "    float k = float (s) * 5.0;"
    "    float d0 = decode (wordAt (k)); float d1 = decode (wordAt (k + 1.0)); float d2 = decode (wordAt (k + 2.0)); float d3 = decode (wordAt (k + 3.0)); float d4 = decode (wordAt (k + 4.0));"
    "    float c0 = 4.0 * d0 + d1; float c1 = d1; float c2 = 4.0 * d2 + d3; float c3 = d3; float c4 = 4.0 * d4;"
    "    float b0 = c4; float b1 = (c0 - 2.0) * c4; float b2 = (1.0 - c1) * c4; float a1 = c2 - 2.0; float a2 = 1.0 - c3;"
    "    float nr = b0 + b1 * cw + b2 * c2w; float ni = -b1 * sw - b2 * s2w;"
    "    float dr = 1.0 + a1 * cw + a2 * c2w; float di = -a1 * sw - a2 * s2w;"
    "    db += 10.0 * log2 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30) * 0.30103;"
    "  }"
    "  float t = clamp ((db + 30.0) / 60.0, 0.0, 1.0);"
    "  gl_FragColor = vec4 (mix (vec3 (0.047, 0.059, 0.102), vec3 (0.847, 0.847, 0.902), t), 1.0);"
    "}";

std::unique_ptr<juce::OpenGLShaderProgram> makeProgram (juce::OpenGLContext& ctx, const char* vs, const char* fs)
{
    auto p = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    p->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vs));
    p->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fs));
    p->link();
    return p;
}
}

void GLRenderer::create (juce::OpenGLContext& ctx)
{
    using namespace juce::gl;
    marks = makeProgram (ctx, kMarksVS, kMarksFS);
    surface = makeProgram (ctx, kSurfaceVS, kSurfaceFS);
    glGenBuffers (1, &vbo);
    glGenTextures (1, &tex);
    texRows = 0;
}

void GLRenderer::destroy()
{
    using namespace juce::gl;
    if (vbo != 0) glDeleteBuffers (1, &vbo);
    if (tex != 0) glDeleteTextures (1, &tex);
    vbo = tex = 0;
    texRows = 0;
    marks.reset();
    surface.reset();
}

void GLRenderer::uploadWords (const std::vector<Frame>& frames)
{
    using namespace juce::gl;
    if (tex == 0 || (int) frames.size() == texRows) return;
    std::vector<float> data;
    data.reserve (frames.size() * 30);
    for (const auto& f : frames)
        for (int s = 0; s < kRows; ++s)
            for (int k = 0; k < kWords; ++k)
                data.push_back ((float) f.words[(size_t) s][(size_t) k]);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_R32F, 30, (GLsizei) frames.size(), 0, GL_RED, GL_FLOAT, data.data());
    glBindTexture (GL_TEXTURE_2D, 0);
    texRows = (int) frames.size();
}

void GLRenderer::draw (const FieldMesh& field, const std::vector<Batch>& batches, float width, float height, float scale)
{
    using namespace juce::gl;
    juce::OpenGLHelpers::clear (juce::Colours::black);
    if (marks == nullptr || surface == nullptr) return;
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (0x8642);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    if (! field.v.empty() && texRows > 0 && surface->getProgramID() != 0)
    {
        surface->use();
        juce::OpenGLShaderProgram::Uniform (*surface, "vp").set (width * scale, height * scale);
        juce::OpenGLShaderProgram::Uniform (*surface, "rows").set ((float) texRows);
        const double w = 2.0 * juce::MathConstants<double>::pi * field.hz / kDatumHz;
        juce::OpenGLShaderProgram::Uniform (*surface, "w").set ((float) w);
        juce::OpenGLShaderProgram::Uniform (*surface, "cw").set ((float) std::cos (w));
        juce::OpenGLShaderProgram::Uniform (*surface, "sw").set ((float) std::sin (w));
        juce::OpenGLShaderProgram::Uniform (*surface, "c2w").set ((float) std::cos (2.0 * w));
        juce::OpenGLShaderProgram::Uniform (*surface, "s2w").set ((float) std::sin (2.0 * w));
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, tex);
        juce::OpenGLShaderProgram::Uniform (*surface, "words").set (0);
        std::vector<FieldVertex> scaled = field.v;
        for (auto& x : scaled) { x.x *= scale; x.y *= scale; }
        glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (scaled.size() * sizeof (FieldVertex)), scaled.data(), GL_DYNAMIC_DRAW);
        const juce::OpenGLShaderProgram::Attribute pos (*surface, "pos"), bary (*surface, "bary"), idx (*surface, "idx");
        glVertexAttribPointer ((GLuint) pos.attributeID, 2, GL_FLOAT, GL_FALSE, sizeof (FieldVertex), (void*) 0);
        glVertexAttribPointer ((GLuint) bary.attributeID, 3, GL_FLOAT, GL_FALSE, sizeof (FieldVertex), (void*) (2 * sizeof (float)));
        glVertexAttribPointer ((GLuint) idx.attributeID, 3, GL_FLOAT, GL_FALSE, sizeof (FieldVertex), (void*) (5 * sizeof (float)));
        glEnableVertexAttribArray ((GLuint) pos.attributeID);
        glEnableVertexAttribArray ((GLuint) bary.attributeID);
        glEnableVertexAttribArray ((GLuint) idx.attributeID);
        glDrawArrays (GL_TRIANGLES, 0, (GLsizei) scaled.size());
        glDisableVertexAttribArray ((GLuint) pos.attributeID);
        glDisableVertexAttribArray ((GLuint) bary.attributeID);
        glDisableVertexAttribArray ((GLuint) idx.attributeID);
        glBindTexture (GL_TEXTURE_2D, 0);
    }
    if (marks->getProgramID() == 0) { glBindBuffer (GL_ARRAY_BUFFER, 0); return; }
    marks->use();
    juce::OpenGLShaderProgram::Uniform (*marks, "vp").set (width * scale, height * scale);
    const juce::OpenGLShaderProgram::Uniform isRound (*marks, "isRound");
    const juce::OpenGLShaderProgram::Attribute pos (*marks, "pos"), col (*marks, "col"), size (*marks, "size");
    for (const auto& b : batches)
    {
        if (b.v.empty() || b.kind == Batch::tris) continue;
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
