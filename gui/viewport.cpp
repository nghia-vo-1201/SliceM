#include "viewport.h"

#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "geometry/types.h"

namespace slicem_gui {

namespace {

const char* kVertexSource = R"(#version 330 core
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
uniform mat4 mvp;
uniform mat3 view_rot;
out vec3 view_normal;
void main() {
  view_normal = view_rot * in_normal;
  gl_Position = mvp * vec4(in_pos, 1.0);
}
)";

const char* kFragmentSource = R"(#version 330 core
in vec3 view_normal;
uniform vec4 color;
uniform int lit;
out vec4 out_color;
void main() {
  if (lit == 0) {
    out_color = color;
    return;
  }
  vec3 n = normalize(view_normal);
  vec3 key = normalize(vec3(0.35, 0.45, 0.82));   // view space, up-left-front
  vec3 fill = normalize(vec3(-0.6, -0.2, 0.5));
  // abs(): meshes with flipped winding still read as solid rather than going black.
  float d = abs(dot(n, key)) * 0.62 + abs(dot(n, fill)) * 0.18;
  out_color = vec4(color.rgb * (0.28 + d), color.a);
}
)";

constexpr double kPi = 3.14159265358979323846;
constexpr double kFovDeg = 45.0;

QVector3D to_world(double x, double y, double z) {
  return QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

}  // namespace

Viewport::Viewport(QWidget* parent) : QOpenGLWidget(parent) {
  setMinimumSize(320, 240);
  setFocusPolicy(Qt::StrongFocus);
  target_ = to_world(bed_x_ / 2, bed_y_ / 2, 0);
}

Viewport::~Viewport() {
  if (gl_ready_) {
    makeCurrent();
    mesh_vbo_.destroy();
    bed_vbo_.destroy();
    layer_vbo_.destroy();
    mesh_vao_.destroy();
    bed_vao_.destroy();
    layer_vao_.destroy();
    doneCurrent();
  }
}

void Viewport::set_bed_size(double x_mm, double y_mm) {
  bed_x_ = x_mm;
  bed_y_ = y_mm;
  bed_dirty_ = true;
  update();
}

void Viewport::set_mesh(const slicem::TriangleMesh& mesh) {
  clear_slice_result();
  mesh_data_.clear();
  mesh_vertex_count_ = 0;
  has_mesh_ = false;

  const slicem::Bounds3 b = slicem::compute_bounds(mesh);
  if (!b.valid || mesh.triangles.empty()) {
    mesh_dirty_ = true;
    update();
    return;
  }

  // Centre on the bed in XY and rest the lowest point on the plate. Done in double mm so the
  // nm grid is only narrowed to float after the offset is removed.
  const double cx = (slicem::units_to_mm(b.min.x) + slicem::units_to_mm(b.max.x)) / 2.0;
  const double cy = (slicem::units_to_mm(b.min.y) + slicem::units_to_mm(b.max.y)) / 2.0;
  offset_x_ = bed_x_ / 2.0 - cx;
  offset_y_ = bed_y_ / 2.0 - cy;
  offset_z_ = -slicem::units_to_mm(b.min.z);

  std::vector<double> p(mesh.vertices.size() * 3);
  for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
    p[i * 3 + 0] = slicem::units_to_mm(mesh.vertices[i].x) + offset_x_;
    p[i * 3 + 1] = slicem::units_to_mm(mesh.vertices[i].y) + offset_y_;
    p[i * 3 + 2] = slicem::units_to_mm(mesh.vertices[i].z) + offset_z_;
  }

  mesh_data_.reserve(mesh.triangles.size() * 18);
  for (const slicem::Triangle& t : mesh.triangles) {
    const double* a = &p[static_cast<std::size_t>(t.v[0]) * 3];
    const double* bb = &p[static_cast<std::size_t>(t.v[1]) * 3];
    const double* c = &p[static_cast<std::size_t>(t.v[2]) * 3];
    const double ux = bb[0] - a[0], uy = bb[1] - a[1], uz = bb[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    double nx = uy * vz - uz * vy;
    double ny = uz * vx - ux * vz;
    double nz = ux * vy - uy * vx;
    const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0) {
      nx /= len;
      ny /= len;
      nz /= len;
    }
    for (const double* q : {a, bb, c}) {
      mesh_data_.push_back(static_cast<float>(q[0]));
      mesh_data_.push_back(static_cast<float>(q[1]));
      mesh_data_.push_back(static_cast<float>(q[2]));
      mesh_data_.push_back(static_cast<float>(nx));
      mesh_data_.push_back(static_cast<float>(ny));
      mesh_data_.push_back(static_cast<float>(nz));
    }
  }
  mesh_vertex_count_ = static_cast<int>(mesh.triangles.size() * 3);

  model_min_ = to_world(slicem::units_to_mm(b.min.x) + offset_x_,
                        slicem::units_to_mm(b.min.y) + offset_y_, 0.0);
  model_max_ = to_world(slicem::units_to_mm(b.max.x) + offset_x_,
                        slicem::units_to_mm(b.max.y) + offset_y_,
                        slicem::units_to_mm(b.max.z) + offset_z_);
  has_mesh_ = true;
  mesh_dirty_ = true;
  reset_view();
}

void Viewport::clear_mesh() {
  clear_slice_result();
  mesh_data_.clear();
  mesh_vertex_count_ = 0;
  has_mesh_ = false;
  mesh_dirty_ = true;
  reset_view();
}

void Viewport::set_slice_result(const slicem::SliceResult& result) {
  layer_data_.clear();
  layer_ranges_.clear();
  for (const slicem::Layer& layer : result.layers) {
    LayerRange range;
    range.first = static_cast<int>(layer_data_.size() / 3);
    const float z = static_cast<float>(slicem::units_to_mm(layer.slice_z) + offset_z_);
    for (const slicem::Polygon& ring : layer.contours) {
      const std::size_t n = ring.size();
      if (n < 2) continue;
      for (std::size_t i = 0; i < n; ++i) {
        const slicem::Point2& a = ring[i];
        const slicem::Point2& b = ring[(i + 1) % n];
        layer_data_.push_back(static_cast<float>(slicem::units_to_mm(a.x) + offset_x_));
        layer_data_.push_back(static_cast<float>(slicem::units_to_mm(a.y) + offset_y_));
        layer_data_.push_back(z);
        layer_data_.push_back(static_cast<float>(slicem::units_to_mm(b.x) + offset_x_));
        layer_data_.push_back(static_cast<float>(slicem::units_to_mm(b.y) + offset_y_));
        layer_data_.push_back(z);
      }
    }
    range.count = static_cast<int>(layer_data_.size() / 3) - range.first;
    layer_ranges_.push_back(range);
  }
  layer_dirty_ = true;
  update();
}

void Viewport::clear_slice_result() {
  layer_data_.clear();
  layer_ranges_.clear();
  layer_dirty_ = true;
  update();
}

void Viewport::set_mode(Mode mode) {
  mode_ = mode;
  update();
}

void Viewport::set_layer(int index) {
  layer_ = index;
  update();
}

void Viewport::set_camera(double yaw_deg, double pitch_deg) {
  yaw_ = yaw_deg;
  pitch_ = std::clamp(pitch_deg, -89.0, 89.0);
  update();
}

void Viewport::reset_view() {
  QVector3D lo, hi;
  if (has_mesh_) {
    lo = model_min_;
    hi = model_max_;
  } else {
    lo = to_world(0, 0, 0);
    hi = to_world(bed_x_, bed_y_, 0);
  }
  target_ = (lo + hi) * 0.5f;
  const double radius = 0.5 * static_cast<double>((hi - lo).length());
  distance_ = std::max(radius, 5.0) / std::sin(kFovDeg * kPi / 360.0) * 1.1;
  yaw_ = -60.0;
  pitch_ = 35.0;
  update();
}

void Viewport::rebuild_bed() {
  bed_data_.clear();
  auto add = [this](double x, double y, double z) {
    bed_data_.push_back(static_cast<float>(x));
    bed_data_.push_back(static_cast<float>(y));
    bed_data_.push_back(static_cast<float>(z));
  };
  // Plate surface (6 vertices, triangles), just under Z=0 so grid lines win the depth test.
  const double z = -0.05;
  add(0, 0, z); add(bed_x_, 0, z); add(bed_x_, bed_y_, z);
  add(0, 0, z); add(bed_x_, bed_y_, z); add(0, bed_y_, z);
  // Grid every 10 mm.
  int grid = 0;
  for (double x = 10.0; x < bed_x_; x += 10.0) {
    add(x, 0, 0); add(x, bed_y_, 0);
    grid += 2;
  }
  for (double y = 10.0; y < bed_y_; y += 10.0) {
    add(0, y, 0); add(bed_x_, y, 0);
    grid += 2;
  }
  bed_grid_count_ = grid;
  // Outline.
  add(0, 0, 0); add(bed_x_, 0, 0);
  add(bed_x_, 0, 0); add(bed_x_, bed_y_, 0);
  add(bed_x_, bed_y_, 0); add(0, bed_y_, 0);
  add(0, bed_y_, 0); add(0, 0, 0);
  bed_outline_count_ = 8;
}

void Viewport::initializeGL() {
  gl_.initializeOpenGLFunctions();
  program_.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexSource);
  program_.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentSource);
  program_.link();

  for (QOpenGLVertexArrayObject* vao : {&mesh_vao_, &bed_vao_, &layer_vao_}) {
    vao->create();
  }
  mesh_vbo_.create();
  bed_vbo_.create();
  layer_vbo_.create();

  gl_.glEnable(GL_DEPTH_TEST);
  gl_.glClearColor(0.16f, 0.17f, 0.19f, 1.0f);
  gl_ready_ = true;
  bed_dirty_ = true;
  mesh_dirty_ = true;
  layer_dirty_ = true;
}

void Viewport::resizeGL(int, int) {}

void Viewport::upload_pending() {
  auto upload = [this](QOpenGLVertexArrayObject& vao, QOpenGLBuffer& vbo,
                       const std::vector<float>& data, int stride_floats) {
    QOpenGLVertexArrayObject::Binder binder(&vao);
    vbo.bind();
    vbo.allocate(data.empty() ? nullptr : data.data(),
                 static_cast<int>(data.size() * sizeof(float)));
    const int stride = stride_floats * static_cast<int>(sizeof(float));
    gl_.glEnableVertexAttribArray(0);
    gl_.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    if (stride_floats == 6) {
      gl_.glEnableVertexAttribArray(1);
      gl_.glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                                reinterpret_cast<void*>(3 * sizeof(float)));
    }
    vbo.release();
  };
  if (bed_dirty_) {
    rebuild_bed();
    upload(bed_vao_, bed_vbo_, bed_data_, 3);
    bed_dirty_ = false;
  }
  if (mesh_dirty_) {
    upload(mesh_vao_, mesh_vbo_, mesh_data_, 6);
    mesh_dirty_ = false;
  }
  if (layer_dirty_) {
    upload(layer_vao_, layer_vbo_, layer_data_, 3);
    layer_dirty_ = false;
  }
}

QVector3D Viewport::eye_position() const {
  const double yaw = yaw_ * kPi / 180.0;
  const double pitch = pitch_ * kPi / 180.0;
  const QVector3D dir(static_cast<float>(std::cos(pitch) * std::cos(yaw)),
                      static_cast<float>(std::cos(pitch) * std::sin(yaw)),
                      static_cast<float>(std::sin(pitch)));
  return target_ + dir * static_cast<float>(distance_);
}

void Viewport::draw_lines(QOpenGLBuffer&, int first, int count, const QVector4D& color) {
  if (count <= 0) return;
  program_.setUniformValue("lit", 0);
  program_.setUniformValue("color", color);
  gl_.glDrawArrays(GL_LINES, first, count);
}

void Viewport::paintGL() {
  upload_pending();
  gl_.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
  view_.setToIdentity();
  view_.lookAt(eye_position(), target_, QVector3D(0, 0, 1));
  proj_.setToIdentity();
  const float near_plane = static_cast<float>(std::max(0.1, distance_ * 0.01));
  const float far_plane = static_cast<float>(distance_ * 10.0 + 2000.0);
  proj_.perspective(static_cast<float>(kFovDeg), aspect, near_plane, far_plane);

  program_.bind();
  program_.setUniformValue("mvp", proj_ * view_);
  program_.setUniformValue("view_rot", view_.normalMatrix());

  // Bed: surface, grid, outline.
  {
    QOpenGLVertexArrayObject::Binder binder(&bed_vao_);
    program_.setUniformValue("lit", 0);
    program_.setUniformValue("color", QVector4D(0.22f, 0.24f, 0.27f, 1.0f));
    gl_.glDrawArrays(GL_TRIANGLES, 0, 6);
    draw_lines(bed_vbo_, 6, bed_grid_count_, QVector4D(0.34f, 0.37f, 0.41f, 1.0f));
    draw_lines(bed_vbo_, 6 + bed_grid_count_, bed_outline_count_, QVector4D(0.95f, 0.55f, 0.15f, 1.0f));
  }

  if (mode_ == Mode::Solid) {
    if (mesh_vertex_count_ > 0) {
      QOpenGLVertexArrayObject::Binder binder(&mesh_vao_);
      program_.setUniformValue("lit", 1);
      program_.setUniformValue("color", QVector4D(0.35f, 0.62f, 0.92f, 1.0f));
      gl_.glDrawArrays(GL_TRIANGLES, 0, mesh_vertex_count_);
    }
  } else if (layer_ >= 0 && layer_ < static_cast<int>(layer_ranges_.size())) {
    QOpenGLVertexArrayObject::Binder binder(&layer_vao_);
    const LayerRange& r = layer_ranges_[static_cast<std::size_t>(layer_)];
    draw_lines(layer_vbo_, r.first, r.count, QVector4D(0.35f, 0.95f, 0.45f, 1.0f));
  }
  program_.release();
}

void Viewport::mousePressEvent(QMouseEvent* e) {
  last_pos_ = e->pos();
}

void Viewport::mouseMoveEvent(QMouseEvent* e) {
  const QPoint d = e->pos() - last_pos_;
  last_pos_ = e->pos();
  if (e->buttons() & Qt::LeftButton) {
    yaw_ -= d.x() * 0.5;
    pitch_ = std::clamp(pitch_ + d.y() * 0.5, -89.0, 89.0);
    update();
  } else if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) {
    // Move the target along the screen axes; scale so the model follows the cursor.
    const QVector3D eye = eye_position();
    const QVector3D forward = (target_ - eye).normalized();
    const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 0, 1)).normalized();
    const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
    const double per_pixel =
        2.0 * distance_ * std::tan(kFovDeg * kPi / 360.0) / std::max(1, height());
    target_ += (-right * static_cast<float>(d.x() * per_pixel)) +
               (up * static_cast<float>(d.y() * per_pixel));
    update();
  }
}

void Viewport::wheelEvent(QWheelEvent* e) {
  const double steps = e->angleDelta().y() / 120.0;
  distance_ = std::clamp(distance_ * std::pow(0.88, steps), 2.0, 5000.0);
  update();
}

}  // namespace slicem_gui
