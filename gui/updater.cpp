#include "updater.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QWidget>

#include <algorithm>

#include "slicem_version.h"

namespace slicem_gui {
namespace {

QList<int> parse_version(const QString& text) {
  QString cleaned = text.trimmed();
  if (cleaned.startsWith('v', Qt::CaseInsensitive)) {
    cleaned.remove(0, 1);
  }
  // Drop any pre-release or build suffix: "1.2.3-beta.1" compares as 1.2.3.
  const int cut = cleaned.indexOf(QRegularExpression(QStringLiteral("[-+]")));
  if (cut >= 0) {
    cleaned.truncate(cut);
  }

  QList<int> parts;
  const QStringList fields = cleaned.split('.', Qt::SkipEmptyParts);
  for (const QString& field : fields) {
    bool ok = false;
    const int value = field.toInt(&ok);
    parts.append(ok ? value : 0);
  }
  return parts;
}

}  // namespace

int compare_versions(const QString& a, const QString& b) {
  const QList<int> left = parse_version(a);
  const QList<int> right = parse_version(b);
  const int count = std::max(left.size(), right.size());
  for (int i = 0; i < count; ++i) {
    const int lv = i < left.size() ? left[i] : 0;
    const int rv = i < right.size() ? right[i] : 0;
    if (lv != rv) {
      return lv < rv ? -1 : 1;
    }
  }
  return 0;
}

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)) {}

void UpdateChecker::check(QWidget* dialog_parent, bool quiet) {
  QNetworkRequest request{QUrl(QStringLiteral(SLICEM_RELEASES_API))};
  request.setRawHeader("Accept", "application/vnd.github+json");
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QStringLiteral("SliceM/%1").arg(QStringLiteral(SLICEM_VERSION)));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply* reply = network_->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, dialog_parent, quiet]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      if (!quiet) {
        QMessageBox::warning(dialog_parent, tr("Check for updates"),
                             tr("Could not reach GitHub:\n\n%1").arg(reply->errorString()));
      }
      return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    if (!document.isObject()) {
      if (!quiet) {
        QMessageBox::warning(dialog_parent, tr("Check for updates"),
                             tr("GitHub returned a response this build could not read."));
      }
      return;
    }

    const QJsonObject release = document.object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    if (tag.isEmpty()) {
      if (!quiet) {
        QMessageBox::information(dialog_parent, tr("Check for updates"),
                                 tr("No published releases yet."));
      }
      return;
    }

    const QString current = QStringLiteral(SLICEM_VERSION);
    if (compare_versions(tag, current) <= 0) {
      if (!quiet) {
        QMessageBox::information(
            dialog_parent, tr("Check for updates"),
            tr("SliceM %1 is the latest version.").arg(current));
      }
      return;
    }

    QString notes = release.value(QStringLiteral("body")).toString().trimmed();
    constexpr int kMaxNotes = 1200;
    if (notes.size() > kMaxNotes) {
      notes.truncate(kMaxNotes);
      notes.append(QStringLiteral("\n..."));
    }

    QString page = release.value(QStringLiteral("html_url")).toString();
    // Only follow a URL that actually points at this project's releases. The response is
    // third-party data, so it does not get to choose where the user's browser is sent.
    if (!page.startsWith(QStringLiteral(SLICEM_RELEASES_PAGE))) {
      page = QStringLiteral(SLICEM_RELEASES_PAGE);
    }

    QMessageBox box(dialog_parent);
    box.setWindowTitle(tr("Update available"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr("SliceM %1 is available. You are running %2.").arg(tag, current));
    if (!notes.isEmpty()) {
      box.setDetailedText(notes);
    }
    QPushButton* open = box.addButton(tr("Open release page"), QMessageBox::AcceptRole);
    box.addButton(tr("Later"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == open) {
      QDesktopServices::openUrl(QUrl(page));
    }
  });
}

}  // namespace slicem_gui
