#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QToolBar>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QComboBox>
#include <QAction>
#include <QUrl>
#include <QCoreApplication>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QAudioRecorder>
#include <QAudioEncoderSettings>
#include <QMultimedia>
#else
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QMediaFormat>
#include <QAudioInput>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QPermission>
#endif
#ifdef Q_OS_MAC
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <dispatch/dispatch.h>
#endif
#endif

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        setWindowTitle("Note & Record");
        resize(900, 600);

        editor = new QTextEdit(this);
        setCentralWidget(editor);

        auto *toolbar = addToolBar("Controls");
        QAction *openAction = toolbar->addAction("Open");
        QAction *saveAction = toolbar->addAction("Save As");
        toolbar->addSeparator();

        recordAction = toolbar->addAction("Record");
        recordAction->setCheckable(true);

        formatCombo = new QComboBox(this);
        formatCombo->addItem("wav");
        formatCombo->addItem("m4a"); // falls back to AAC/MP4 where supported
        toolbar->addWidget(formatCombo);

        connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
        connect(saveAction, &QAction::triggered, this, &MainWindow::saveFileAs);
        connect(recordAction, &QAction::toggled, this, &MainWindow::toggleRecording);

        // Ensure the output directory exists ahead of time.
        outputDir = QDir(QCoreApplication::applicationDirPath()).filePath("output");
        QDir().mkpath(outputDir);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        audioRecorder = new QAudioRecorder(this);
#else
#ifndef Q_OS_MAC
        captureSession = new QMediaCaptureSession();
        audioInput = new QAudioInput(this);
        mediaRecorder = new QMediaRecorder(this);
        captureSession->setAudioInput(audioInput);
        captureSession->setRecorder(mediaRecorder);
#endif
#endif
    }

private:
    QTextEdit *editor{};
    QAction *recordAction{};
    QComboBox *formatCombo{};
    QString outputDir;
    QString lastRecordingPath;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QAudioRecorder *audioRecorder{};
#else
    QMediaCaptureSession *captureSession{};
    QAudioInput *audioInput{};
    QMediaRecorder *mediaRecorder{};
#endif
#ifdef Q_OS_MAC
    AVAudioRecorder *macRecorder{};
#endif

    void openFile()
    {
        const QString fileName = QFileDialog::getOpenFileName(
            this, "Open text file", QString(), "Text Files (*.txt *.md);;All Files (*)");
        if (fileName.isEmpty())
            return;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Open Failed", "Could not open file for reading.");
            return;
        }
        editor->setPlainText(QString::fromUtf8(file.readAll()));
    }

    void saveFileAs()
    {
        const QString fileName = QFileDialog::getSaveFileName(
            this, "Save text file", QString(), "Text Files (*.txt *.md);;All Files (*)");
        if (fileName.isEmpty())
            return;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Save Failed", "Could not open file for writing.");
            return;
        }
        const QByteArray data = editor->toPlainText().toUtf8();
        if (file.write(data) != data.size()) {
            QMessageBox::warning(this, "Save Failed", "Could not write full contents.");
        }
    }

    QString nextOutputPath(const QString &extension) const
    {
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        return QDir(outputDir).filePath(QString("recording_%1.%2").arg(stamp, extension));
    }

    void toggleRecording(bool shouldRecord)
    {
        if (shouldRecord) {
            startRecording();
        } else {
            stopRecording();
        }
    }

    void startRecording()
    {
#ifdef Q_OS_MAC
        AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        if (authStatus != AVAuthorizationStatusAuthorized) {
            __block BOOL granted = NO;
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                     completionHandler:^(BOOL g) {
                                         granted = g;
                                         dispatch_semaphore_signal(sem);
                                     }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            if (!granted) {
                recordAction->setChecked(false);
                QMessageBox::warning(this, "Microphone blocked",
                                     "Enable microphone access in System Settings > Privacy & Security > Microphone.");
                return;
            }
        }
#elif QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if defined(QT_CONFIG) && QT_CONFIG(permissions)
        QMicrophonePermission micPerm;
        const auto status = qApp->checkPermission(micPerm);
        if (status == Qt::PermissionStatus::Denied) {
            QMessageBox::warning(this, "Microphone blocked",
                                 "Microphone access is required to record. Enable it in System Settings.");
            recordAction->setChecked(false);
            return;
        } else if (status == Qt::PermissionStatus::Undetermined) {
            qApp->requestPermission(micPerm, this, [this](const QPermission &perm) {
                if (perm.status() == Qt::PermissionStatus::Granted) {
                    this->startRecordingInternal();
                } else {
                    recordAction->setChecked(false);
                    QMessageBox::warning(this, "Microphone blocked",
                                         "Microphone access is required to record. Enable it in System Settings.");
                }
            });
            return;
        }
#else
        // Permissions plugin not available; rely on OS-level permission.
#endif
#endif
        startRecordingInternal();
    }

    void startRecordingInternal()
    {
        const QString extension = formatCombo->currentText().toLower();
        const QString filePath = nextOutputPath(extension);
        lastRecordingPath = filePath;

#ifdef Q_OS_MAC
        NSError *error = nil;
        NSString *nsPath = [NSString stringWithUTF8String:filePath.toUtf8().constData()];
        NSMutableDictionary *settings = [NSMutableDictionary dictionary];
        if (extension == "wav") {
            settings[AVFormatIDKey] = @(kAudioFormatLinearPCM);
            settings[AVSampleRateKey] = @44100.0;
            settings[AVNumberOfChannelsKey] = @2;
            settings[AVLinearPCMBitDepthKey] = @16;
            settings[AVLinearPCMIsFloatKey] = @NO;
        } else {
            settings[AVFormatIDKey] = @(kAudioFormatMPEG4AAC);
            settings[AVSampleRateKey] = @44100.0;
            settings[AVNumberOfChannelsKey] = @2;
        }
        macRecorder = [[AVAudioRecorder alloc] initWithURL:[NSURL fileURLWithPath:nsPath]
                                                 settings:settings
                                                    error:&error];
        if (error || !macRecorder) {
            recordAction->setChecked(false);
            QMessageBox::warning(this, "Record failed", "Could not start recorder (native).");
            return;
        }
        [macRecorder record];
        return;
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QAudioEncoderSettings settings;
        if (extension == "wav") {
            settings.setCodec("audio/pcm");
            settings.setSampleRate(44100);
            settings.setChannelCount(2);
            audioRecorder->setContainerFormat("audio/x-wav");
        } else if (extension == "m4a") {
            settings.setCodec("audio/aac");
            settings.setSampleRate(44100);
            settings.setChannelCount(2);
            audioRecorder->setContainerFormat("audio/mp4");
        }
        settings.setQuality(QMultimedia::NormalQuality);
        audioRecorder->setAudioSettings(settings);
        audioRecorder->setOutputLocation(QUrl::fromLocalFile(filePath));
        audioRecorder->record();
#else
        QMediaFormat format;
        if (extension == "wav") {
            format.setFileFormat(QMediaFormat::Wave);
            format.setAudioCodec(QMediaFormat::AudioCodec::Wave);
        } else {
            format.setFileFormat(QMediaFormat::Mpeg4Audio);
            format.setAudioCodec(QMediaFormat::AudioCodec::AAC);
        }
        mediaRecorder->setMediaFormat(format);
        mediaRecorder->setQuality(QMediaRecorder::Quality::NormalQuality);
        mediaRecorder->setOutputLocation(QUrl::fromLocalFile(filePath));
        mediaRecorder->record();
#endif
    }

    QString pythonExecutable() const
    {
        // Prefer python3, fallback to python.
        const QString envPy = QProcessEnvironment::systemEnvironment().value("PYTHON", "");
        if (!envPy.isEmpty())
            return envPy;
        return (QFile::exists("/usr/bin/python3") || QFile::exists("C:/Python311/python.exe"))
                   ? QStringLiteral("python3")
                   : QStringLiteral("python");
    }

    QString runPythonScript(const QString &scriptName, const QStringList &args) const
    {
        QString scriptPath = QDir(QCoreApplication::applicationDirPath()).filePath(scriptName);
        if (!QFile::exists(scriptPath)) {
            // Fallback to project root when running from a build tree.
            scriptPath = QDir(QCoreApplication::applicationDirPath()).filePath("../" + scriptName);
        }
        if (!QFile::exists(scriptPath)) {
            return QString("script not found: %1").arg(scriptName);
        }

        QProcess proc;
        proc.setProgram(pythonExecutable());
        proc.setArguments(QStringList{scriptPath} + args);
        proc.setWorkingDirectory(QCoreApplication::applicationDirPath());
        proc.start();
        if (!proc.waitForFinished(10000)) {
            proc.kill();
            return QString("timeout running %1").arg(scriptName);
        }
        const QByteArray out = proc.readAllStandardOutput();
        const QByteArray err = proc.readAllStandardError();
        if (!err.isEmpty()) {
            qWarning() << scriptName << "stderr:" << err;
        }
        return QString::fromUtf8(out).trimmed();
    }

    void stopRecording()
    {
#ifdef Q_OS_MAC
        if (macRecorder) {
            if (macRecorder.recording) {
                [macRecorder stop];
            }
            macRecorder = nil;
        }
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        audioRecorder->stop();
#else
        mediaRecorder->stop();
#endif
        recordAction->setChecked(false);
        if (!lastRecordingPath.isEmpty()) {
            const QString summary = runPythonScript("audio_processor.py", {lastRecordingPath});
            const QString mlSummary = runPythonScript("ml_model.py", {lastRecordingPath});
            qDebug() << "Audio summary:" << summary;
            qDebug() << "ML summary:" << mlSummary;
        }
        QMessageBox::information(this, "Recording saved", "Audio saved to the output folder.");
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.show();

    return app.exec();
}
