#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QTimer>

#include "main_window.h"
#include "viewport.h"

int main(int argc, char** argv) {
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);
  format.setSamples(4);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication app(argc, argv);

  // Optional: slicem-gui [model.stl]. The remaining options exist for headless-ish smoke
  // testing: render once, save the viewport framebuffer, quit.
  QCommandLineParser parser;
  parser.addPositionalArgument(QStringLiteral("model"), QStringLiteral("STL file to open"));
  QCommandLineOption layer_opt(QStringLiteral("layer"), QStringLiteral("show layer preview at N"),
                               QStringLiteral("n"));
  QCommandLineOption grab_opt(QStringLiteral("grab"), QStringLiteral("save viewport PNG and exit"),
                              QStringLiteral("png"));
  parser.addOption(layer_opt);
  parser.addOption(grab_opt);
  parser.process(app);

  slicem_gui::MainWindow window;
  window.show();
  if (!parser.positionalArguments().isEmpty()) {
    window.load_model(parser.positionalArguments().first());
  }
  if (parser.isSet(layer_opt)) {
    window.show_layer_mode();
    window.show_layer(parser.value(layer_opt).toInt());
  }
  if (parser.isSet(grab_opt)) {
    const QString out = parser.value(grab_opt);
    QTimer::singleShot(1500, &app, [&]() {
      window.viewport()->grabFramebuffer().save(out);
      app.quit();
    });
  }
  return app.exec();
}
