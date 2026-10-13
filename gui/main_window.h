#pragma once

#include <QMainWindow>
#include <QString>

#include "slicing/mesh.h"
#include "slicing/slicer.h"

class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QRadioButton;
class QSlider;

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

 private:
  void open_dialog();
  void reslice();
  void update_summary();
  void update_mode();
  void create_menus();

  UpdateChecker* updater_ = nullptr;

  Viewport* viewport_ = nullptr;
  QDoubleSpinBox* layer_height_ = nullptr;
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
};

}  // namespace slicem_gui
