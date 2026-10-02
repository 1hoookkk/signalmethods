import math
import os
import sys
from typing import List, Optional, Tuple
import numpy as np

from PySide6 import QtCore, QtGui, QtOpenGL, QtOpenGLWidgets, QtWidgets
from PySide6.QtGui import QMatrix4x4, QVector3D

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import trench_core

TERRAIN_VERT_SRC = """#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vPos;
out vec3 vNormal;
out vec2 vUV;

void main() {
    vPos = aPos;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
"""

TERRAIN_FRAG_SRC = """#version 330 core
in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;

uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uLaserZ;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vPos);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 16.0);

    float h = clamp((vPos.y + 0.5) / 1.1, 0.0, 1.0);
    vec3 colCharcoal = vec3(0.07, 0.09, 0.08);
    vec3 colDeepCyan = vec3(0.08, 0.32, 0.28);
    vec3 colPhosphor = vec3(0.24, 0.78, 0.74);
    vec3 colMint = vec3(0.75, 0.94, 0.84);

    vec3 baseCol;
    if (h < 0.35) {
        baseCol = mix(colCharcoal, colDeepCyan, h / 0.35);
    } else if (h < 0.75) {
        baseCol = mix(colDeepCyan, colPhosphor, (h - 0.35) / 0.4);
    } else {
        baseCol = mix(colPhosphor, colMint, (h - 0.75) / 0.25);
    }

    vec3 ambient = baseCol * 0.45;
    vec3 diffuse = baseCol * diff * 0.55;
    vec3 specular = vec3(0.8, 1.0, 0.9) * spec * 0.25;
    vec3 finalColor = ambient + diffuse + specular;

    float dLaser = abs(vPos.z - uLaserZ);
    if (dLaser < 0.035) {
        float glow = pow(1.0 - (dLaser / 0.035), 2.5);
        finalColor += vec3(0.25, 0.9, 0.8) * glow * 1.6;
    }

    FragColor = vec4(finalColor, 1.0);
}
"""

LINE_VERT_SRC = """#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
"""

LINE_FRAG_SRC = """#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
"""

class Camera3D:
    def __init__(self):
        self.yaw: float = 38.0
        self.pitch: float = 28.0
        self.distance: float = 3.3
        self.target: QVector3D = QVector3D(0.0, 0.0, 0.0)

    def orbit(self, dx: float, dy: float):
        self.yaw += dx * 0.4
        self.pitch = max(-88.0, min(88.0, self.pitch + dy * 0.4))

    def pan(self, dx: float, dy: float):
        yaw_rad = math.radians(self.yaw)
        pitch_rad = math.radians(self.pitch)
        right = QVector3D(-math.cos(yaw_rad), 0.0, -math.sin(yaw_rad)).normalized()
        up = QVector3D(0.0, 1.0, 0.0)
        speed = self.distance * 0.0018
        self.target += right * (dx * speed) + up * (dy * speed)

    def zoom(self, delta: float):
        factor = 1.0 - (delta * 0.0012)
        self.distance = max(0.6, min(14.0, self.distance * factor))

    def get_position(self) -> QVector3D:
        yaw_rad = math.radians(self.yaw)
        pitch_rad = math.radians(self.pitch)
        x = self.target.x() + self.distance * math.cos(pitch_rad) * math.sin(yaw_rad)
        y = self.target.y() + self.distance * math.sin(pitch_rad)
        z = self.target.z() + self.distance * math.cos(pitch_rad) * math.cos(yaw_rad)
        return QVector3D(x, y, z)

    def get_view_matrix(self) -> QMatrix4x4:
        pos = self.get_position()
        up = QVector3D(0.0, 1.0, 0.0)
        view = QMatrix4x4()
        view.lookAt(pos, self.target, up)
        return view

class Terrain3DWidget(QtOpenGLWidgets.QOpenGLWidget):
    NUM_FREQS = 160
    NUM_MORPHS = 64

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        fmt = QtGui.QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QtGui.QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        fmt.setSamples(4)
        self.setFormat(fmt)

        self.camera = Camera3D()
        self.last_mouse_pos = QtCore.QPoint()
        self.dragging_button: Optional[QtCore.Qt.MouseButton] = None

        self.morph: float = 0.5
        self.q: float = 0.5
        self.datum_hz: float = 44100.0
        self.host_hz: float = 44100.0

        self.freqs_hz = np.geomspace(20.0, 20000.0, self.NUM_FREQS)
        self.morph_grid = np.linspace(0.0, 1.0, self.NUM_MORPHS)

        self._gl_initialized: bool = False
        self.gl: Optional[QtOpenGL.QOpenGLFunctions_3_3_Core] = None

        self.prog_terrain: Optional[QtOpenGL.QOpenGLShaderProgram] = None
        self.prog_lines: Optional[QtOpenGL.QOpenGLShaderProgram] = None

        self.vao_terrain: Optional[QtOpenGL.QOpenGLVertexArrayObject] = None
        self.vbo_terrain: Optional[QtOpenGL.QOpenGLBuffer] = None

        self.vao_laser: Optional[QtOpenGL.QOpenGLVertexArrayObject] = None
        self.vbo_laser: Optional[QtOpenGL.QOpenGLBuffer] = None

        self.vao_rails: Optional[QtOpenGL.QOpenGLVertexArrayObject] = None
        self.vbo_rails: Optional[QtOpenGL.QOpenGLBuffer] = None

        self.terrain_vertex_count: int = 0
        self.laser_vertex_count: int = 0
        self.rails_vertex_count: int = 0

        self._current_body: Optional[trench_core.Body] = None
        self._raw_grid_db: np.ndarray = np.zeros((self.NUM_MORPHS, self.NUM_FREQS), dtype=np.float32)

        self.setFocusPolicy(QtCore.Qt.FocusPolicy.StrongFocus)
        self.setMinimumSize(480, 360)

    def initializeGL(self):
        profile = QtOpenGL.QOpenGLVersionProfile()
        profile.setVersion(3, 3)
        profile.setProfile(QtGui.QSurfaceFormat.OpenGLContextProfile.CoreProfile)
        self.gl = QtOpenGL.QOpenGLVersionFunctionsFactory.get(profile, self.context())
        if not self.gl:
            return
        self.gl.initializeOpenGLFunctions()

        self.prog_terrain = QtOpenGL.QOpenGLShaderProgram(self)
        self.prog_terrain.addShaderFromSourceCode(QtOpenGL.QOpenGLShader.ShaderTypeBit.Vertex, TERRAIN_VERT_SRC)
        self.prog_terrain.addShaderFromSourceCode(QtOpenGL.QOpenGLShader.ShaderTypeBit.Fragment, TERRAIN_FRAG_SRC)
        self.prog_terrain.link()

        self.prog_lines = QtOpenGL.QOpenGLShaderProgram(self)
        self.prog_lines.addShaderFromSourceCode(QtOpenGL.QOpenGLShader.ShaderTypeBit.Vertex, LINE_VERT_SRC)
        self.prog_lines.addShaderFromSourceCode(QtOpenGL.QOpenGLShader.ShaderTypeBit.Fragment, LINE_FRAG_SRC)
        self.prog_lines.link()

        self.vao_terrain = QtOpenGL.QOpenGLVertexArrayObject(self)
        self.vao_terrain.create()
        self.vao_terrain.bind()
        self.vbo_terrain = QtOpenGL.QOpenGLBuffer(QtOpenGL.QOpenGLBuffer.Type.VertexBuffer)
        self.vbo_terrain.create()
        self.vbo_terrain.bind()
        self.prog_terrain.bind()
        self.prog_terrain.enableAttributeArray(0)
        self.prog_terrain.setAttributeBuffer(0, 0x1406, 0, 3, 32)
        self.prog_terrain.enableAttributeArray(1)
        self.prog_terrain.setAttributeBuffer(1, 0x1406, 12, 3, 32)
        self.prog_terrain.enableAttributeArray(2)
        self.prog_terrain.setAttributeBuffer(2, 0x1406, 24, 2, 32)
        self.prog_terrain.release()
        self.vao_terrain.release()

        self.vao_laser = QtOpenGL.QOpenGLVertexArrayObject(self)
        self.vao_laser.create()
        self.vao_laser.bind()
        self.vbo_laser = QtOpenGL.QOpenGLBuffer(QtOpenGL.QOpenGLBuffer.Type.VertexBuffer)
        self.vbo_laser.create()
        self.vbo_laser.bind()
        self.prog_lines.bind()
        self.prog_lines.enableAttributeArray(0)
        self.prog_lines.setAttributeBuffer(0, 0x1406, 0, 3, 12)
        self.prog_lines.release()
        self.vao_laser.release()

        self.vao_rails = QtOpenGL.QOpenGLVertexArrayObject(self)
        self.vao_rails.create()
        self.vao_rails.bind()
        self.vbo_rails = QtOpenGL.QOpenGLBuffer(QtOpenGL.QOpenGLBuffer.Type.VertexBuffer)
        self.vbo_rails.create()
        self.vbo_rails.bind()
        self.prog_lines.bind()
        self.prog_lines.enableAttributeArray(0)
        self.prog_lines.setAttributeBuffer(0, 0x1406, 0, 3, 12)
        self.prog_lines.release()
        self.vao_rails.release()

        self._gl_initialized = True
        self._build_static_rails()

        if self._current_body:
            self.rebuild_terrain(self._current_body, self.q)
        else:
            self._build_flat_terrain()

    def resizeGL(self, w: int, h: int):
        if self.gl:
            self.gl.glViewport(0, 0, w, h)

    def paintGL(self):
        if not self.gl or not self._gl_initialized:
            return

        self.gl.glClearColor(0.071, 0.086, 0.082, 1.0)
        self.gl.glClear(0x00004000 | 0x00000100)
        self.gl.glEnable(0x0B71)
        self.gl.glEnable(0x0BE2)
        self.gl.glBlendFunc(0x0302, 0x0303)

        aspect = float(self.width()) / float(max(1, self.height()))
        proj = QMatrix4x4()
        proj.perspective(45.0, aspect, 0.1, 50.0)

        view = self.camera.get_view_matrix()
        model = QMatrix4x4()
        mvp = proj * view * model

        cam_pos = self.camera.get_position()
        light_dir = QVector3D(0.4, 0.8, 0.5).normalized()
        laser_z = (self.morph - 0.5) * 2.0

        if self.terrain_vertex_count > 0 and self.vao_terrain and self.prog_terrain:
            self.prog_terrain.bind()
            self.prog_terrain.setUniformValue("uMVP", mvp)
            self.prog_terrain.setUniformValue("uModel", model)
            self.prog_terrain.setUniformValue("uCameraPos", cam_pos)
            self.prog_terrain.setUniformValue("uLightDir", light_dir)
            self.prog_terrain.setUniformValue1f("uLaserZ", float(laser_z))

            self.vao_terrain.bind()
            self.gl.glDrawArrays(0x0004, 0, self.terrain_vertex_count)
            self.vao_terrain.release()
            self.prog_terrain.release()

        if self.prog_lines:
            self.prog_lines.bind()
            self.prog_lines.setUniformValue("uMVP", mvp)

            if self.rails_vertex_count > 0 and self.vao_rails:
                self.prog_lines.setUniformValue("uColor", QtGui.QVector4D(0.18, 0.32, 0.28, 0.7))
                self.vao_rails.bind()
                self.gl.glDrawArrays(0x0001, 0, self.rails_vertex_count)
                self.vao_rails.release()

            if self.laser_vertex_count > 0 and self.vao_laser:
                self.prog_lines.setUniformValue("uColor", QtGui.QVector4D(0.75, 0.96, 0.85, 1.0))
                self.vao_laser.bind()
                self.gl.glLineWidth(2.5)
                self.gl.glDrawArrays(0x0003, 0, self.laser_vertex_count)
                self.vao_laser.release()

            self.prog_lines.release()

    def _build_static_rails(self):
        if not self._gl_initialized or not self.vbo_rails:
            return

        lines: List[float] = []
        floor_y = -0.52
        x_min, x_max = -1.2, 1.2
        z_min, z_max = -1.0, 1.0

        for z in np.linspace(z_min, z_max, 9):
            lines.extend([x_min, floor_y, float(z), x_max, floor_y, float(z)])
        for x in np.linspace(x_min, x_max, 9):
            lines.extend([float(x), floor_y, z_min, float(x), floor_y, z_max])

        pillar_h = 0.55
        corners = [(x_min, z_min), (x_max, z_min), (x_min, z_max), (x_max, z_max)]
        for cx, cz in corners:
            lines.extend([cx, floor_y, cz, cx, floor_y + pillar_h, cz])

        arr = np.array(lines, dtype=np.float32)
        self.rails_vertex_count = len(arr) // 3
        self.vbo_rails.bind()
        self.vbo_rails.allocate(arr.tobytes(), arr.nbytes)
        self.vbo_rails.release()

    def _build_flat_terrain(self):
        self._raw_grid_db = np.zeros((self.NUM_MORPHS, self.NUM_FREQS), dtype=np.float32)
        self._rebuild_mesh_buffers()
        self._rebuild_laser_buffer()

    def set_position(self, morph: float, q: float):
        old_q = self.q
        self.morph = max(0.0, min(1.0, morph))
        self.q = max(0.0, min(1.0, q))

        if abs(self.q - old_q) > 1e-4 and self._current_body:
            self.rebuild_terrain(self._current_body, self.q)
        else:
            self._rebuild_laser_buffer()
            self.update()

    def rebuild_terrain(self, body: trench_core.Body, q: float):
        self._current_body = body
        self.q = max(0.0, min(1.0, q))

        for j, m in enumerate(self.morph_grid):
            bqs = body.cascade(float(m), float(self.q), 0.0, self.datum_hz, self.host_hz)
            db = trench_core.cascade_response_db(bqs, self.freqs_hz, self.host_hz)
            self._raw_grid_db[j, :] = db.astype(np.float32)

        if self._gl_initialized:
            self._rebuild_mesh_buffers()
            self._rebuild_laser_buffer()
            self.update()

    def _rebuild_mesh_buffers(self):
        if not self._gl_initialized or not self.vbo_terrain:
            return

        x_vals = np.linspace(-1.2, 1.2, self.NUM_FREQS, dtype=np.float32)
        z_vals = np.linspace(-1.0, 1.0, self.NUM_MORPHS, dtype=np.float32)

        y_vals = np.clip(self._raw_grid_db, -36.0, 24.0)
        y_scaled = (y_vals + 36.0) / 60.0 * 1.1 - 0.5

        gx, gz = np.meshgrid(x_vals, z_vals)
        gy = y_scaled

        dx_x = np.gradient(gx, axis=1)
        dx_y = np.gradient(gy, axis=1)
        dx_z = np.gradient(gz, axis=1)

        dz_x = np.gradient(gx, axis=0)
        dz_y = np.gradient(gy, axis=0)
        dz_z = np.gradient(gz, axis=0)

        nx = dz_y * dx_z - dz_z * dx_y
        ny = dz_z * dx_x - dz_x * dx_z
        nz = dz_x * dx_y - dz_y * dx_x

        length = np.sqrt(nx * nx + ny * ny + nz * nz) + 1e-7
        nx /= length
        ny /= length
        nz /= length

        u_vals = np.linspace(0.0, 1.0, self.NUM_FREQS, dtype=np.float32)
        v_vals = np.linspace(0.0, 1.0, self.NUM_MORPHS, dtype=np.float32)
        gu, gv = np.meshgrid(u_vals, v_vals)

        p00 = np.stack([gx[:-1, :-1], gy[:-1, :-1], gz[:-1, :-1], nx[:-1, :-1], ny[:-1, :-1], nz[:-1, :-1], gu[:-1, :-1], gv[:-1, :-1]], axis=-1)
        p10 = np.stack([gx[1:, :-1], gy[1:, :-1], gz[1:, :-1], nx[1:, :-1], ny[1:, :-1], nz[1:, :-1], gu[1:, :-1], gv[1:, :-1]], axis=-1)
        p01 = np.stack([gx[:-1, 1:], gy[:-1, 1:], gz[:-1, 1:], nx[:-1, 1:], ny[:-1, 1:], nz[:-1, 1:], gu[:-1, 1:], gv[:-1, 1:]], axis=-1)
        p11 = np.stack([gx[1:, 1:], gy[1:, 1:], gz[1:, 1:], nx[1:, 1:], ny[1:, 1:], nz[1:, 1:], gu[1:, 1:], gv[1:, 1:]], axis=-1)

        tri1 = np.stack([p00, p10, p01], axis=2).reshape(-1, 3, 8)
        tri2 = np.stack([p01, p10, p11], axis=2).reshape(-1, 3, 8)

        all_tris = np.concatenate([tri1, tri2], axis=0).reshape(-1, 8).astype(np.float32)
        self.terrain_vertex_count = all_tris.shape[0]

        self.vbo_terrain.bind()
        self.vbo_terrain.allocate(all_tris.tobytes(), all_tris.nbytes)
        self.vbo_terrain.release()

    def _rebuild_laser_buffer(self):
        if not self._gl_initialized or not self.vbo_laser:
            return

        x_vals = np.linspace(-1.2, 1.2, self.NUM_FREQS, dtype=np.float32)
        laser_z = (self.morph - 0.5) * 2.0

        if self._current_body:
            bqs = self._current_body.cascade(float(self.morph), float(self.q), 0.0, self.datum_hz, self.host_hz)
            db = trench_core.cascade_response_db(bqs, self.freqs_hz, self.host_hz)
            y_clamped = np.clip(db, -36.0, 24.0)
            y_scaled = (y_clamped + 36.0) / 60.0 * 1.1 - 0.5 + 0.008
        else:
            y_scaled = np.zeros(self.NUM_FREQS, dtype=np.float32)

        line_pts = np.zeros((self.NUM_FREQS, 3), dtype=np.float32)
        line_pts[:, 0] = x_vals
        line_pts[:, 1] = y_scaled.astype(np.float32)
        line_pts[:, 2] = laser_z

        self.laser_vertex_count = self.NUM_FREQS
        self.vbo_laser.bind()
        self.vbo_laser.allocate(line_pts.tobytes(), line_pts.nbytes)
        self.vbo_laser.release()

    def mousePressEvent(self, event: QtGui.QMouseEvent):
        self.last_mouse_pos = event.position().toPoint()
        self.dragging_button = event.button()
        event.accept()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent):
        pos = event.position().toPoint()
        dx = pos.x() - self.last_mouse_pos.x()
        dy = pos.y() - self.last_mouse_pos.y()
        self.last_mouse_pos = pos

        if event.buttons() & QtCore.Qt.MouseButton.MiddleButton or (event.buttons() & QtCore.Qt.MouseButton.LeftButton and event.modifiers() == QtCore.Qt.KeyboardModifier.NoModifier):
            self.camera.orbit(dx, -dy)
            self.update()
        elif event.buttons() & QtCore.Qt.MouseButton.RightButton or (event.buttons() & QtCore.Qt.MouseButton.LeftButton and (event.modifiers() & QtCore.Qt.KeyboardModifier.ShiftModifier)):
            self.camera.pan(dx, dy)
            self.update()

        event.accept()

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent):
        self.dragging_button = None
        event.accept()

    def wheelEvent(self, event: QtGui.QWheelEvent):
        delta = event.angleDelta().y()
        self.camera.zoom(delta)
        self.update()
        event.accept()
