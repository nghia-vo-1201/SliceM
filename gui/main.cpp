#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "slicing/slicer.h"
#include "slicing/stl_reader.h"

namespace {

std::string slice_report(const std::string& path, double layer_height_mm) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(3);

  const slicem::StlLoadResult loaded = slicem::load_stl(path);
  if (!loaded.ok) {
    return "Could not load model:\n  " + loaded.error;
  }

  const slicem::Bounds3 bounds = slicem::compute_bounds(loaded.mesh);
  os << path << "\n\n"
     << (loaded.binary ? "binary STL" : "ASCII STL") << ", " << loaded.mesh.triangles.size()
     << " triangles, " << loaded.mesh.vertices.size() << " welded vertices\n"
     << "size  " << slicem::units_to_mm(bounds.max.x - bounds.min.x) << " x "
     << slicem::units_to_mm(bounds.max.y - bounds.min.y) << " x "
     << slicem::units_to_mm(bounds.max.z - bounds.min.z) << " mm\n\n";

  slicem::SliceSettings settings;
  settings.layer_height_mm = layer_height_mm;
  const slicem::SliceResult sliced = slicem::slice_mesh(loaded.mesh, settings);
  if (!sliced.ok) {
    return os.str() + "Slicing failed:\n  " + sliced.error;
  }

  std::size_t contours = 0;
  double area_sum = 0.0;
  for (const slicem::Layer& layer : sliced.layers) {
    contours += layer.contours.size();
    for (const slicem::Polygon& contour : layer.contours) {
      area_sum += std::fabs(slicem::signed_area_mm2(contour));
    }
  }

  os << "layers         " << sliced.layers.size() << " at " << layer_height_mm << " mm\n"
     << "contours       " << contours << "\n"
     << "open contours  " << sliced.total_open_contours << "\n";
  if (sliced.total_open_contours > 0) {
    os << "\nThis mesh is not watertight at some planes. Layer outlines there are\n"
          "incomplete, so it is not yet safe to derive toolpaths from them.\n";
  }
  if (!sliced.layers.empty()) {
    os << "mean area      " << area_sum / static_cast<double>(sliced.layers.size())
       << " mm^2 per layer\n";
  }
  return os.str();
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);

  QMainWindow window;
  window.setWindowTitle(QStringLiteral("SliceM"));

  auto* central = new QWidget;
  auto* layout = new QVBoxLayout(central);

  auto* controls = new QHBoxLayout;
  auto* layer_height = new QDoubleSpinBox;
  layer_height->setRange(0.05, 0.50);
  layer_height->setSingleStep(0.05);
  layer_height->setDecimals(2);
  layer_height->setValue(0.20);
  layer_height->setSuffix(QStringLiteral(" mm"));
  auto* open_button = new QPushButton(QStringLiteral("Open STL..."));
  controls->addWidget(new QLabel(QStringLiteral("Layer height")));
  controls->addWidget(layer_height);
  controls->addWidget(open_button);
  controls->addStretch();

  auto* output = new QPlainTextEdit;
  output->setReadOnly(true);
  output->setPlainText(QStringLiteral(
      "Load an STL to slice it.\n\n"
      "This build reports layer geometry only. Toolpath generation, G-code export and the\n"
      "same-surface colour-group feature are not implemented yet, so nothing here can be\n"
      "sent to a printer."));

  layout->addLayout(controls);
  layout->addWidget(output);
  window.setCentralWidget(central);

  QObject::connect(open_button, &QPushButton::clicked, [&]() {
    const QString path = QFileDialog::getOpenFileName(&window, QStringLiteral("Open model"),
                                                      QString(),
                                                      QStringLiteral("STL models (*.stl);;All files (*)"));
    if (path.isEmpty()) {
      return;
    }
    output->setPlainText(
        QString::fromStdString(slice_report(path.toStdString(), layer_height->value())));
  });

  window.resize(760, 520);
  window.show();
  return app.exec();
}
