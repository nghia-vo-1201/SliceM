#pragma once

#include <QMainWindow>
#include <QString>

#include "gcode/slice_to_gcode.h"
#include "slicing/mesh.h"
#include "slicing/slicer.h"

class QAction;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;

namespace slicem_gui {

class Viewport;
class UpdateChecker;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  MainWindow();
  void load_model(const QString& path);
  Viewport* viewport() const { return viewport_; }
  void show_layer(int index);
  void show_layer_mode();

  // The export without the dialog, so the whole widgets-to-file path can be exercised from the
  // command line. `error` is filled only on failure.
  bool write_gcode_to(const QString& path, QString* error);

 private:
  void open_dialog();
  void export_gcode();
  void reslice();
  void update_summary();
  void update_mode();
  void update_export_enabled();
  void create_menus();

  UpdateChecker* updater_ = nullptr;

  Viewport* viewport_ = nullptr;
  QDoubleSpinBox* layer_height_ = nullptr;
  QSpinBox* perimeters_ = nullptr;
  QSpinBox* infill_density_ = nullptr;
  QDoubleSpinBox* extrusion_width_ = nullptr;
  QPushButton* export_button_ = nullptr;
  QAction* export_action_ = nullptr;
  QRadioButton* solid_radio_ = nullptr;
  QRadioButton* layers_radio_ = nullptr;
  QLabel* warning_ = nullptr;
  QPlainTextEdit* summary_ = nullptr;
  QSlider* layer_slider_ = nullptr;
  QLabel* layer_label_ = nullptr;

  QString path_;
  bool binary_ = false;
  slicem::TriangleMesh mesh_;
  bool have_mesh_ = false;
  slicem::SliceResult result_;
  bool have_result_ = false;
  QString load_error_;

  slicem::GcodeStats gcode_stats_;
  bool have_gcode_ = false;
  QString gcode_path_;
};

}  // namespace slicem_gui
