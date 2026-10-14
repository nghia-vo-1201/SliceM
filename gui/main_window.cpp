#include "main_window.h"

#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

#include "slicem_version.h"
#include "updater.h"

#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "gcode/printer_profile.h"
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

  perimeters_ = new QSpinBox;
  perimeters_->setRange(1, 10);
  perimeters_->setValue(2);

  infill_density_ = new QSpinBox;
  infill_density_->setRange(0, 100);
  infill_density_->setValue(20);
  infill_density_->setSuffix(QStringLiteral(" %"));

  extrusion_width_ = new QDoubleSpinBox;
  extrusion_width_->setRange(0.10, 2.00);
  extrusion_width_->setSingleStep(0.05);
  extrusion_width_->setDecimals(2);
  extrusion_width_->setValue(0.45);
  extrusion_width_->setSuffix(QStringLiteral(" mm"));

  auto* settings_form = new QFormLayout;
  settings_form->setContentsMargins(0, 0, 0, 0);
  settings_form->addRow(QStringLiteral("Layer height"), layer_height_);
  settings_form->addRow(QStringLiteral("Perimeters"), perimeters_);
  settings_form->addRow(QStringLiteral("Infill"), infill_density_);
  settings_form->addRow(QStringLiteral("Extrusion width"), extrusion_width_);

  export_button_ = new QPushButton(QStringLiteral("Export G-code..."));
  export_button_->setEnabled(false);

  // Only the legacy dialect is implemented. Naming the one target machine in the UI is better
  // than an empty combo box implying a choice that does not exist.
  auto* printer_label = new QLabel(QStringLiteral("Printer: Original Prusa i3 MK3S"));
  printer_label->setStyleSheet(QStringLiteral("color:#888;"));

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
  side->addLayout(settings_form);
  side->addWidget(printer_label);
  side->addWidget(export_button_);
  side->addSpacing(8);
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
  create_menus();

  connect(open_button, &QPushButton::clicked, this, &MainWindow::open_dialog);
  connect(export_button_, &QPushButton::clicked, this, &MainWindow::export_gcode);
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

void MainWindow::create_menus() {
  updater_ = new UpdateChecker(this);

  QMenu* file_menu = menuBar()->addMenu(tr("&File"));
  file_menu->addAction(tr("&Open model..."), QKeySequence::Open, this, &MainWindow::open_dialog);
  export_action_ = file_menu->addAction(tr("&Export G-code..."), QKeySequence::Save, this,
                                        &MainWindow::export_gcode);
  export_action_->setEnabled(false);
  file_menu->addSeparator();
  file_menu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

  QMenu* help_menu = menuBar()->addMenu(tr("&Help"));
  help_menu->addAction(tr("Check for &updates..."), this,
                       [this]() { updater_->check(this, false); });
  help_menu->addAction(tr("&About SliceM"), this, [this]() {
    QMessageBox::about(
        this, tr("About SliceM"),
        tr("<b>SliceM %1</b><br><br>"
           "An independently authored slicer for Prusa printers.<br><br>"
           "This build slices geometry, previews layers, and exports G-code for the "
           "Original Prusa i3 MK3S. The Buddy-firmware machines (MK4, Core One) and the "
           "same-surface colour-group feature are not implemented yet.<br><br>"
           "<b>No G-code from this build has been validated on hardware.</b> Treat a first "
           "print as a watched run.")
            .arg(QStringLiteral(SLICEM_VERSION)));
  });

  setWindowTitle(tr("SliceM %1").arg(QStringLiteral(SLICEM_VERSION)));
}

void MainWindow::open_dialog() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open model"), QString(),
      QStringLiteral("STL models (*.stl);;All files (*)"));
  if (!path.isEmpty()) load_model(path);
}

bool MainWindow::write_gcode_to(const QString& path, QString* error) {
  const auto fail = [error](const QString& message) {
    if (error != nullptr) *error = message;
    return false;
  };

  if (!have_mesh_) {
    return fail(tr("No model is loaded."));
  }
  if (!have_result_) {
    return fail(tr("The model could not be sliced: %1").arg(QString::fromStdString(result_.error)));
  }

  // An open contour means the layer outline is incomplete, so every perimeter and every infill
  // boundary derived from it is wrong. Emitting anyway would produce a file that looks valid and
  // prints as a tangle, so this refuses rather than warning and continuing.
  if (result_.total_open_contours > 0) {
    return fail(tr("The mesh is not watertight: %1 open contour(s) were found.\n\n"
                   "Layer outlines at those planes are incomplete, so the toolpaths derived from "
                   "them would be wrong. Repair the mesh and slice it again.")
                    .arg(result_.total_open_contours));
  }

  slicem::GcodeSettings settings;
  settings.layer_height_mm = layer_height_->value();
  settings.perimeter_count = perimeters_->value();
  settings.infill_density = static_cast<double>(infill_density_->value()) / 100.0;
  settings.extrusion_width_mm = extrusion_width_->value();

  const slicem::PrinterProfile profile = slicem::mk3s_profile();
  const slicem::GcodeStats stats =
      slicem::write_gcode_file(result_, profile, settings, path.toStdString());

  if (!stats.ok) {
    have_gcode_ = false;
    update_summary();
    return fail(QString::fromStdString(stats.error));
  }

  gcode_stats_ = stats;
  gcode_path_ = path;
  have_gcode_ = true;
  update_summary();
  return true;
}

void MainWindow::export_gcode() {
  if (!have_result_) {
    return;
  }
  if (result_.total_open_contours > 0) {
    QString why;
    write_gcode_to(QString(), &why);
    QMessageBox::critical(this, tr("Cannot export"), why);
    return;
  }

  QString suggested = QStringLiteral("out.gcode");
  if (!path_.isEmpty()) {
    const QFileInfo info(path_);
    suggested = info.dir().filePath(info.completeBaseName() + QStringLiteral(".gcode"));
  }
  const QString path = QFileDialog::getSaveFileName(this, tr("Export G-code"), suggested,
                                                    tr("G-code (*.gcode);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }

  QString error;
  if (!write_gcode_to(path, &error)) {
    QMessageBox::critical(this, tr("Export failed"),
                          tr("SliceM refused to write this job:\n\n%1").arg(error));
    return;
  }
  const slicem::GcodeStats& stats = gcode_stats_;

  QMessageBox::information(
      this, tr("G-code exported"),
      tr("<b>%1</b><br><br>"
         "%2 layers, top Z %3 mm<br>"
         "%4 mm of filament (%5 cm&sup3;)<br><br>"
         "This file has not been validated on hardware. Check it in a G-code viewer, and watch "
         "the first print with the stop control within reach.")
          .arg(QFileInfo(path).fileName())
          .arg(stats.layers)
          .arg(stats.print_z_max_mm, 0, 'f', 3)
          .arg(stats.filament_used_mm, 0, 'f', 1)
          .arg(stats.filament_used_cm3, 0, 'f', 2));
}

void MainWindow::update_export_enabled() {
  const bool ready = have_result_ && result_.total_open_contours == 0;
  if (export_button_ != nullptr) {
    export_button_->setEnabled(ready);
  }
  if (export_action_ != nullptr) {
    export_action_->setEnabled(ready);
  }
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
  // Any previously exported file describes the old slice, so its figures must not keep showing.
  have_gcode_ = false;

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
  update_export_enabled();
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
        "Load an STL to slice it, then export G-code for the MK3S.\n\n"
        "The Buddy-firmware machines (MK4, Core One) and the same-surface colour-group\n"
        "feature are not implemented yet.\n\n"
        "No G-code from this build has been validated on hardware."));
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
    if (have_gcode_) {
      os << "\nExported " << QFileInfo(gcode_path_).fileName().toStdString() << "\n"
         << "  path       " << gcode_stats_.extruded_path_length_mm << " mm\n"
         << "  deposited  " << gcode_stats_.deposited_volume_mm3 << " mm^3\n"
         << "  filament   " << gcode_stats_.filament_used_mm << " mm ("
         << gcode_stats_.filament_used_cm3 << " cm^3)\n"
         << "  top z      " << gcode_stats_.print_z_max_mm << " mm\n"
         << "\nNOT VALIDATED ON HARDWARE.\n";
    } else {
      os << "\nColour groups are not implemented yet.\n";
    }
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
