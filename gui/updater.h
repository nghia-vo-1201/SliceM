#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QWidget;

namespace slicem_gui {

// Compares dotted version strings, tolerating a leading "v" and differing component counts.
// Returns >0 when `a` is newer than `b`, 0 when equal, <0 when older.
int compare_versions(const QString& a, const QString& b);

class UpdateChecker : public QObject {
  Q_OBJECT

 public:
  explicit UpdateChecker(QObject* parent = nullptr);

  // Asks GitHub for the newest published release and, if it is newer than this build, offers to
  // open the release page.
  //
  // It deliberately does not download or run anything. An updater that fetches and executes a
  // binary is a remote code execution path straight into the user's machine, and it is not worth
  // adding one to save a click — especially for software that drives a 300 C hotend.
  //
  // `quiet` suppresses the "you are up to date" and failure dialogs, for the check on startup.
  void check(QWidget* dialog_parent, bool quiet);

 private:
  QNetworkAccessManager* network_ = nullptr;
};

}  // namespace slicem_gui
