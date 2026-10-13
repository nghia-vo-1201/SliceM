#pragma once

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>

#include <vector>

#include "slicing/mesh.h"
#include "slicing/slicer.h"

namespace slicem_gui {

// 3D view of the build plate, the loaded mesh (solid mode) and one sliced layer's contours
// (layer mode). Pure presentation: all geometry comes from the engine, converted to float mm
// only for drawing. World space is millimetres, Z up, bed corner at the origin.
class Viewport : public QOpenGLWidget {
  Q_OBJECT
 public:
  enum class Mode { Solid, Layers };

  explicit Viewport(QWidget* parent = nullptr);
  ~Viewport() override;

  void set_bed_size(double x_mm, double y_mm);
  // Uploads the mesh (centred on the bed, resting on Z=0). Clears any slice result.
  void set_mesh(const slicem::TriangleMesh& mesh);
  void clear_mesh();
  // Uploads all layer outlines once; scrubbing afterwards only changes the draw range.
  void set_slice_result(const slicem::SliceResult& result);
  void clear_slice_result();

  void set_mode(Mode mode);
  void set_layer(int index);
  void reset_view();
  void set_camera(double yaw_deg, double pitch_deg);

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void wheelEvent(QWheelEvent* e) override;

 private:
  struct LayerRange {
    int first = 0;
    int count = 0;
  };

  void rebuild_bed();
  void upload_pending();
  QVector3D eye_position() const;
  void draw_lines(QOpenGLBuffer& vbo, int first, int count, const QVector4D& color);

  QOpenGLFunctions_3_3_Core gl_;
  QOpenGLShaderProgram program_;
  bool gl_ready_ = false;

  // Solid mesh: interleaved position(3) + normal(3), 3 vertices per triangle.
  QOpenGLVertexArrayObject mesh_vao_;
  QOpenGLBuffer mesh_vbo_{QOpenGLBuffer::VertexBuffer};
  std::vector<float> mesh_data_;
  bool mesh_dirty_ = false;
  int mesh_vertex_count_ = 0;

  // Line sets: position(3) only.
  QOpenGLVertexArrayObject bed_vao_;
  QOpenGLBuffer bed_vbo_{QOpenGLBuffer::VertexBuffer};
  std::vector<float> bed_data_;
  bool bed_dirty_ = true;
  int bed_grid_count_ = 0;   // grid line vertices
  int bed_outline_count_ = 0;  // outline line vertices (follow the grid ones)

  QOpenGLVertexArrayObject layer_vao_;
  QOpenGLBuffer layer_vbo_{QOpenGLBuffer::VertexBuffer};
  std::vector<float> layer_data_;
  std::vector<LayerRange> layer_ranges_;
  bool layer_dirty_ = false;

  double bed_x_ = 250.0;
  double bed_y_ = 210.0;
  bool has_mesh_ = false;
  QVector3D model_min_;  // world space, after centring on the bed
  QVector3D model_max_;
  double offset_x_ = 0.0;  // mm added to engine coordinates to get world coordinates
  double offset_y_ = 0.0;
  double offset_z_ = 0.0;

  Mode mode_ = Mode::Solid;
  int layer_ = 0;

  // Orbit camera around target_.
  QVector3D target_;
  double distance_ = 400.0;
  double yaw_ = -60.0;   // degrees, around Z
  double pitch_ = 35.0;  // degrees above the horizon
  QMatrix4x4 view_;
  QMatrix4x4 proj_;
  QPoint last_pos_;
};

}  // namespace slicem_gui
