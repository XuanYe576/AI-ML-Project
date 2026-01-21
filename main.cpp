#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QToolBar>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
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
        QAction *openAction = toolbar->addAction("Open Sheet");
        QAction *saveAction = toolbar->addAction("Save Sheet As");
        toolbar->addSeparator();

        recordAction = toolbar->addAction("Record");
        recordAction->setCheckable(true);

        processorCombo = new QComboBox(this);
        processorCombo->addItem("YIN");
        processorCombo->addItem("APBP");
        toolbar->addWidget(processorCombo);

        QAction *processAction = toolbar->addAction("Run");

        formatCombo = new QComboBox(this);
        formatCombo->addItem("wav");
        formatCombo->addItem("m4a"); // falls back to AAC/MP4 where supported
        toolbar->addWidget(formatCombo);

        connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
        connect(saveAction, &QAction::triggered, this, &MainWindow::saveFileAs);
        connect(recordAction, &QAction::toggled, this, &MainWindow::toggleRecording);
        connect(processAction, &QAction::triggered, this, &MainWindow::runProcessing);

        pitchLog = new QPlainTextEdit(this);
        pitchLog->setReadOnly(true);
        pitchLog->setPlaceholderText("Processor output will appear here.");
        auto *dock = new QDockWidget("Processor Output", this);
        dock->setWidget(pitchLog);
        addDockWidget(Qt::BottomDockWidgetArea, dock);

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

    ~MainWindow() override
    {
        if (yinProcess) {
            yinProcess->kill();
            yinProcess->waitForFinished(1000);
        }
    }

private:
    QTextEdit *editor{};
    QAction *recordAction{};
    QComboBox *processorCombo{};
    QComboBox *formatCombo{};
    QString outputDir;
    QString lastRecordingPath;
    QPlainTextEdit *pitchLog{};
    QProcess *yinProcess{};

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
            this, "Open music sheet", QString(), "Text Files (*.musicxml *.mxl *.mid *.xml *.txt *.md);;All Files (*)");
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
            this, "Save music sheet", QString(), "Text Files (*.musicxml *.mxl *.mid *.xml *.txt *.md);;All Files (*)");
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

    QString resolveScriptPath(const QString &scriptName) const
    {
        const QStringList candidates = {
            QDir(QCoreApplication::applicationDirPath()).filePath(scriptName),
            QDir(QCoreApplication::applicationDirPath()).filePath(scriptName + ".py"),
            QDir(QCoreApplication::applicationDirPath()).filePath("../" + scriptName),
            QDir(QCoreApplication::applicationDirPath()).filePath("../" + scriptName + ".py"),
            QDir(QCoreApplication::applicationDirPath()).filePath("../../" + scriptName),
            QDir(QCoreApplication::applicationDirPath()).filePath("../../" + scriptName + ".py"),
            QDir(QCoreApplication::applicationDirPath()).filePath("../../../" + scriptName),
            QDir(QCoreApplication::applicationDirPath()).filePath("../../../" + scriptName + ".py"),
            QDir(QCoreApplication::applicationDirPath()).filePath("../../../../" + scriptName),
            QDir(QCoreApplication::applicationDirPath()).filePath("../../../../" + scriptName + ".py"),
            QDir::current().filePath(scriptName),
            QDir::current().filePath(scriptName + ".py"),
        };

        for (const auto &cand : candidates) {
            if (QFile::exists(cand)) {
                return QDir(cand).canonicalPath().isEmpty() ? cand : QDir(cand).canonicalPath();
            }
        }
        return {};
    }

    QString runPythonScript(const QString &scriptName, const QStringList &args) const
    {
        QString scriptPath = resolveScriptPath(scriptName);
        if (scriptPath.isEmpty()) {
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

    void runProcessing()
    {
        const QString processor = processorCombo ? processorCombo->currentText() : QStringLiteral("YIN");
        const bool useYin = (processor.compare("YIN", Qt::CaseInsensitive) == 0);
        const QString scriptName = useYin ? QStringLiteral("YINdetection") : QStringLiteral("audio_processor.py");

        QString scriptPath = resolveScriptPath(scriptName);
        if (scriptPath.isEmpty()) {
            QMessageBox::warning(this, "Processing", QString("Could not locate %1 script.").arg(scriptName));
            return;
        }

        const QString defaultDir = lastRecordingPath.isEmpty() ? QString() : QFileInfo(lastRecordingPath).absolutePath();
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            "Select audio to process",
            defaultDir,
            "Audio Files (*.wav *.m4a *.mp3 *.flac);;All Files (*)");
        if (filePath.isEmpty())
            return;

        if (yinProcess) {
            yinProcess->kill();
            yinProcess->deleteLater();
            yinProcess = nullptr;
        }

        if (!pitchLog)
            return;
        pitchLog->clear();
        pitchLog->appendPlainText(QString("Running %1 on: %2").arg(processor, filePath));

        if (!useYin) {
            const QString output = runPythonScript(scriptName, {filePath});
            if (output.isEmpty()) {
                pitchLog->appendPlainText("Processor returned no output.");
            } else {
                pitchLog->appendPlainText(output);
            }
            return;
        }

        yinProcess = new QProcess(this);

        connect(yinProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            if (!pitchLog || !yinProcess)
                return;
            const QString chunk = QString::fromUtf8(yinProcess->readAllStandardOutput());
            const auto lines = chunk.split('\n', Qt::SkipEmptyParts);
            for (const auto &line : lines) {
                pitchLog->appendPlainText(line.trimmed());
            }
        });
        connect(yinProcess, &QProcess::readyReadStandardError, this, [this]() {
            if (!yinProcess)
                return;
            const QByteArray err = yinProcess->readAllStandardError();
            qWarning() << "Processor stderr:" << err;
            if (pitchLog) {
                pitchLog->appendPlainText(QString::fromUtf8(err).trimmed());
            }
        });
        connect(yinProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [this](int code, QProcess::ExitStatus) {
                    if (pitchLog) {
                        pitchLog->appendPlainText(QString("Processor finished (code %1).").arg(code));
                    }
                });

        yinProcess->setProgram(pythonExecutable());
        yinProcess->setArguments({scriptPath, "--audio", filePath, "--stream"});
        yinProcess->setWorkingDirectory(QCoreApplication::applicationDirPath());
        yinProcess->start();
        if (!yinProcess->waitForStarted(3000)) {
            QMessageBox::warning(this, "Processing", "Failed to start.");
            yinProcess->deleteLater();
            yinProcess = nullptr;
        }
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.show();

    return app.exec();
}
