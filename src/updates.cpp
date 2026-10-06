#include "updates.hpp"
#include "version.hpp"
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QProgressDialog>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>
#include <obs.h>
#include <functional>

QByteArray installerChecksum(const QByteArray &manifest, const QString &filename)
{
	QByteArray expected;
	for (auto line : manifest.split('\n')) {
		auto parts = line.trimmed().split(' ');
		parts.removeAll(QByteArray());
		if (parts.size() == 2 && parts[1] == filename.toUtf8()) {
			if (!expected.isEmpty())
				return {};
			expected = parts[0];
		}
	}
	if (expected.size() != 64 || QByteArray::fromHex(expected).size() != 32 ||
	    QByteArray::fromHex(expected).toHex() != expected.toLower())
		return {};
	return expected.toLower();
}
namespace {
QString text(const char *key)
{
	QSettings locale(QString(":/recording-actions/locale/%1.ini")
				 .arg(QString::fromUtf8(obs_get_locale()).startsWith("de") ? "de-DE" : "en-US"),
			 QSettings::IniFormat);
	return locale.value(key, key).toString();
}
class UpdateRequest : public QObject {
	QNetworkAccessManager network{this};
	QPointer<QWidget> parent;
	bool automatic;
	bool cancelled = false;
	QPointer<QProgressDialog> progress;
	void showProgress()
	{
		if (progress || !parent)
			return;
		progress = new QProgressDialog(text("Updates.Busy"), text("Updates.Cancel"), 0, 0, parent);
		progress->setMinimumDuration(0);
		connect(progress, &QProgressDialog::canceled, this, [this] {
			cancelled = true;
			for (auto *reply : network.findChildren<QNetworkReply *>())
				reply->abort();
			deleteLater();
		});
		progress->show();
	}
	void message(QMessageBox::Icon icon, const QString &body)
	{
		if (progress) {
			delete progress.data();
			progress = nullptr;
		}
		if (!parent)
			return;
		auto *box = new QMessageBox(icon, text("Plugin.Name"), body, QMessageBox::Ok, parent);
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->open();
	}
	void fail()
	{
		if (!automatic)
			message(QMessageBox::Warning, text("Updates.Error"));
		deleteLater();
	}
	void get(const QUrl &url, qint64 limit, std::function<void(QByteArray)> done)
	{
		QNetworkRequest request(url);
		request.setHeader(QNetworkRequest::UserAgentHeader, "Recording-Actions/" RECORDING_ACTIONS_VERSION);
		request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
				     QNetworkRequest::NoLessSafeRedirectPolicy);
		request.setTransferTimeout(30000);
		auto *reply = network.get(request);
		connect(reply, &QNetworkReply::readyRead, reply, [reply, limit] {
			if (reply->bytesAvailable() > limit)
				reply->abort();
		});
		connect(reply, &QNetworkReply::finished, this, [this, reply, limit, done] {
			if (cancelled) {
				reply->deleteLater();
				return;
			}
			auto bytes = reply->readAll();
			const bool ok = reply->error() == QNetworkReply::NoError && bytes.size() <= limit;
			reply->deleteLater();
			if (!ok) {
				if (automatic)
					deleteLater();
				else
					fail();
				return;
			}
			done(bytes);
		});
	}

public:
	UpdateRequest(QObject *owner, QWidget *window, bool quiet) : QObject(owner), parent(window), automatic(quiet)
	{
		if (!automatic)
			showProgress();
	}
	~UpdateRequest() override { delete progress.data(); }
	void start()
	{
		get(QUrl("https://api.github.com/repos/Diddlik/obs-recording-actions/releases/latest"), 1024 * 1024,
		    [this](QByteArray bytes) {
			    auto release = QJsonDocument::fromJson(bytes).object();
			    const auto tag = release.value("tag_name").toString();
			    qsizetype suffix = 0;
			    auto version = QVersionNumber::fromString(tag.mid(1), &suffix);
			    if (!tag.startsWith('v') || version.segmentCount() != 3 || suffix != tag.size() - 1 ||
				release.value("draft").toBool() || release.value("prerelease").toBool()) {
				    fail();
				    return;
			    }
			    if (version <= QVersionNumber::fromString(RECORDING_ACTIONS_VERSION)) {
				    if (!automatic && parent)
					    message(QMessageBox::Information, text("Updates.Current"));
				    deleteLater();
				    return;
			    }
			    const auto filename =
				    QString("obs-recording-actions-%1-windows-x64-setup.exe").arg(version.toString());
			    const auto base =
				    QString("https://github.com/Diddlik/obs-recording-actions/releases/download/%1/")
					    .arg(tag);
			    bool installer = false, sums = false;
			    for (const auto asset : release.value("assets").toArray()) {
				    const auto entry = asset.toObject();
				    const auto name = entry.value("name").toString();
				    if (name == filename &&
					entry.value("browser_download_url").toString() == base + filename)
					    installer = true;
				    if (name == "SHA256SUMS.txt" &&
					entry.value("browser_download_url").toString() == base + name)
					    sums = true;
			    }
			    if (!installer || !sums) {
				    fail();
				    return;
			    }
			    if (!parent) {
				    deleteLater();
				    return;
			    }
			    if (progress) {
				    delete progress.data();
				    progress = nullptr;
			    }
			    auto *prompt = new QMessageBox(QMessageBox::Question, text("Plugin.Name"),
							   text("Updates.Available").arg(version.toString()),
							   QMessageBox::Yes | QMessageBox::No, parent);
			    prompt->setDefaultButton(QMessageBox::No);
			    prompt->setAttribute(Qt::WA_DeleteOnClose);
			    connect(prompt, &QMessageBox::finished, this, [this, base, filename](int answer) {
				    if (answer != QMessageBox::Yes) {
					    deleteLater();
					    return;
				    }
				    download(base, filename);
			    });
			    prompt->open();
		    });
	}
	void download(const QString &base, const QString &filename)
	{
		automatic = false;
		showProgress();
		get(QUrl(base + "SHA256SUMS.txt"), 65536, [this, base, filename](QByteArray manifest) {
			const auto expected = installerChecksum(manifest, filename);
			if (expected.isEmpty()) {
				fail();
				return;
			}
			get(QUrl(base + filename), 64 * 1024 * 1024,
			    [this, filename, expected](QByteArray installerBytes) {
				    if (QCryptographicHash::hash(installerBytes, QCryptographicHash::Sha256).toHex() !=
					expected.toLower()) {
					    fail();
					    return;
				    }
				    auto folder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
				    if (folder.isEmpty() || !QDir().mkpath(folder + "/RecordingActions")) {
					    fail();
					    return;
				    }
				    folder += "/RecordingActions";
				    QSaveFile output(folder + '/' + filename);
				    if (!output.open(QIODevice::WriteOnly) ||
					output.write(installerBytes) != installerBytes.size() || !output.commit()) {
					    fail();
					    return;
				    }
				    if (parent)
					    message(QMessageBox::Information, text("Updates.Ready"));
				    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
				    deleteLater();
			    });
		});
	}
};
} // namespace
void checkForUpdates(QObject *owner, QWidget *parent, bool automatic)
{
	// One active request per manager; destroying the manager aborts network work.
	if (owner->findChild<QObject *>("recording-actions-update"))
		return;
	auto *request = new UpdateRequest(owner, parent, automatic);
	request->setObjectName("recording-actions-update");
	request->start();
}
