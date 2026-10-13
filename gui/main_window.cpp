#include "main_window.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "geometry/polygon.h"
#include "geometry/types.h"
#include "slicing/stl_reader.h"
#include "viewport.h"

namespace slicem_gui {

MainWindow::MainWindow() {
  setWindowTitle(QStringLiteral("SliceM"));

  auto* central = new QWidget;
  auto* root = new QHBoxLayout(central);

  // Viewport with the layer slider on its right edge.
  viewport_ = new Viewport;
  viewport_->set_bed_size(250.0, 210.0);
  layer_slider_ = new QSlider(Qt::Vertical);
  layer_slider_->setEnabled(false);
  layer_label_ = new QLabel;
  layer_label_->setAlignment(Qt::AlignHCenter);
  layer_label_->setMinimumWidth(70);
  auto* slider_col = new QVBoxLayout;
  slider_col->addWidget(layer_slider_, 1, Qt::AlignHCenter);
  slider_col->addWidget(layer_label_);
  root->addWidget(viewport_, 1);
  root->addLayout(slider_col);

  // Side panel.
  auto* side = new QVBoxLayout;
  auto* open_button = new QPushButton(QStringLiteral("Open STL..."));
  layer_height_ = new QDoubleSpinBox;
  layer_height_->setRange(0.05, 0.50);
  layer_height_->setSingleStep(0.05);
  layer_height_->setDecimals(2);
  layer_height_->setValue(0.20);
  layer_height_->setSuffix(QStringLiteral(" mm"));
  auto* height_row = new QHBoxLayout;
  height_row->addWidget(new QLabel(QStringLiteral("Layer height")));
  height_row->addWidget(layer_height_);

  solid_radio_ = new QRadioButton(QStringLiteral("Solid model"));
  layers_radio_ = new QRadioButton(QStringLiteral("Layer preview"));
  solid_radio_->setChecked(true);
  auto* reset_button = new QPushButton(QStringLiteral("Reset view"));

  warning_ = new QLabel;
  warning_->setWordWrap(true);
  warning_->setVisible(false);
  warning_->setStyleSheet(QStringLiteral(
      "background:#8b1a1a; color:white; padding:8px; font-weight:bold; border-radius:3px;"));

  summary_ = new QPlainTextEdit;
  summary_->setReadOnly(true);

  side->addWidget(open_button);
  side->addLayout(height_row);
  side->addWidget(solid_radio_);
  side->addWidget(layers_radio_);
  side->addWidget(reset_button);
  side->addWidget(warning_);
  side->addWidget(summary_, 1);
  auto* side_widget = new QWidget;
  side_widget->setLayout(side);
  side_widget->setFixedWidth(300);
  root->addWidget(side_widget);

  setCentralWidget(central);

  connect(open_button, &QPushButton::clicked, this, &MainWindow::open_dialog);
  connect(reset_button, &QPushButton::clicked, viewport_, &Viewport::reset_view);
  connect(layer_height_, &QDoubleSpinBox::editingFinished, this, [this]() {
    if (have_mesh_) reslice();
  });
  connect(solid_radio_, &QRadioButton::toggled, this, [this]() { update_mode(); });
  connect(layer_slider_, &QSlider::valueChanged, this, [this](int v) { show_layer(v); });

  update_summary();
  update_mode();
  resize(1100, 700);
}

void MainWindow::open_dialog() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open model"), QString(),
      QStringLiteral("STL models (*.stl);;All files (*)"));
  if (!path.isEmpty()) load_model(path);
}

void MainWindow::load_model(const QString& path) {
  const slicem::StlLoadResult loaded = slicem::load_stl(path.toStdString());
  if (!loaded.ok) {
    load_error_ = QString::fromStdString(loaded.error);
    update_summary();
    return;
  }
  load_error_.clear();
  path_ = path;
  binary_ = loaded.binary;
  mesh_ = loaded.mesh;
  have_mesh_ = true;
  viewport_->set_mesh(mesh_);
  reslice();
}

void MainWindow::reslice() {
  slicem::SliceSettings settings;
  settings.layer_height_mm = layer_height_->value();
  result_ = slicem::slice_mesh(mesh_, settings);
  have_result_ = result_.ok;
  if (have_result_) {
    viewport_->set_slice_result(result_);
    const int last = static_cast<int>(result_.layers.size()) - 1;
    const bool was_enabled = layer_slider_->isEnabled();
    const int previous = layer_slider_->value();
    layer_slider_->setRange(0, std::max(0, last));
    layer_slider_->setEnabled(last >= 0);
    layer_slider_->setValue(was_enabled ? std::min(previous, std::max(0, last)) : 0);
    show_layer(layer_slider_->value());
  } else {
    viewport_->clear_slice_result();
    layer_slider_->setEnabled(false);
  }
  update_summary();
  update_mode();
}

void MainWindow::show_layer_mode() {
  layers_radio_->setChecked(true);
}

void MainWindow::show_layer(int index) {
  if (layer_slider_->value() != index) layer_slider_->setValue(index);
  viewport_->set_layer(index);
  if (have_result_ && index >= 0 && index < static_cast<int>(result_.layers.size())) {
    const slicem::Layer& layer = result_.layers[static_cast<std::size_t>(index)];
    layer_label_->setText(QStringLiteral("Layer %1 / %2\nZ %3 mm")
                              .arg(index + 1)
                              .arg(result_.layers.size())
                              .arg(slicem::units_to_mm(layer.slice_z), 0, 'f', 2));
  } else {
    layer_label_->clear();
  }
}

void MainWindow::update_mode() {
  const bool layers = layers_radio_->isChecked();
  layers_radio_->setEnabled(have_result_);
  if (!have_result_ && layers) {
    solid_radio_->setChecked(true);
    return;
  }
  viewport_->set_mode(layers ? Viewport::Mode::Layers : Viewport::Mode::Solid);
  layer_slider_->setVisible(true);
  layer_label_->setVisible(layers && have_result_);
}

void MainWindow::update_summary() {
  if (!load_error_.isEmpty()) {
    summary_->setPlainText(QStringLiteral("Could not load model:\n  ") + load_error_);
    return;
  }
  if (!have_mesh_) {
    summary_->setPlainText(QStringLiteral(
        "Load an STL to slice it.\n\n"
        "This build reports layer geometry only. Toolpath generation, G-code export and the\n"
        "same-surface colour-group feature are not implemented yet, so nothing here can be\n"
        "sent to a printer."));
    warning_->setVisible(false);
    return;
  }

  std::ostringstream os;
  os << std::fixed << std::setprecision(3);
  const slicem::Bounds3 b = slicem::compute_bounds(mesh_);
  os << path_.toStdString() << "\n\n"
     << (binary_ ? "binary STL" : "ASCII STL") << "\n"
     << mesh_.triangles.size() << " triangles, " << mesh_.vertices.size() << " welded vertices\n"
     << "size  " << slicem::units_to_mm(b.max.x - b.min.x) << " x "
     << slicem::units_to_mm(b.max.y - b.min.y) << " x "
     << slicem::units_to_mm(b.max.z - b.min.z) << " mm\n\n";

  if (!have_result_) {
    os << "Slicing failed:\n  " << result_.error << "\n";
    warning_->setVisible(false);
  } else {
    std::size_t contours = 0;
    double area_sum = 0.0;
    for (const slicem::Layer& layer : result_.layers) {
      contours += layer.contours.size();
      for (const slicem::Polygon& c : layer.contours) area_sum += std::fabs(slicem::signed_area_mm2(c));
    }
    os << "layers         " << result_.layers.size() << " at " << layer_height_->value() << " mm\n"
       << "contours       " << contours << "\n"
       << "open contours  " << result_.total_open_contours << "\n";
    if (!result_.layers.empty()) {
      os << "mean area      " << area_sum / static_cast<double>(result_.layers.size())
         << " mm^2 per layer\n";
    }
    os << "\nToolpaths, G-code export and colour groups are not implemented yet.\n";
    if (result_.total_open_contours > 0) {
      warning_->setText(
          QStringLiteral("WARNING: mesh is not watertight. %1 open contour(s) found. Layer "
                         "outlines at those planes are incomplete and must not be used to "
                         "derive toolpaths.")
              .arg(result_.total_open_contours));
      warning_->setVisible(true);
    } else {
      warning_->setVisible(false);
    }
  }
  summary_->setPlainText(QString::fromStdString(os.str()));
}

}  // namespace slicem_gui
