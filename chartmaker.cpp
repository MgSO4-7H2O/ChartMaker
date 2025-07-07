#include "chartmaker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QUrl>

// --- InputThread 实现 ---

void InputThread::run()
{
#ifdef Q_OS_WIN
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE) return;

    INPUT_RECORD irInBuf[128];
    DWORD cNumRead;

    while (!isInterruptionRequested()) {
        // 直接从控制台输入缓冲区读取原始事件
        if (ReadConsoleInput(hStdin, irInBuf, 128, &cNumRead)) {
            for (DWORD i = 0; i < cNumRead; i++) {
                if (irInBuf[i].EventType == KEY_EVENT && irInBuf[i].Event.KeyEvent.bKeyDown) {
                    // 将Windows的虚拟键码转换为Qt键值
                    emit keyPressed(irInBuf[i].Event.KeyEvent.wVirtualKeyCode);
                }
            }
        }
    }
#else
    char c;
    while (!isInterruptionRequested()) {
        if (read(STDIN_FILENO, &c, 1) > 0) {
            emit keyPressed(c);
        }
    }
#endif
}


// --- ChartMaker 实现 ---

ChartMaker::ChartMaker(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
    connect(this, &QCoreApplication::aboutToQuit, this, &ChartMaker::saveChart);

    m_inputThread = new InputThread();
    connect(m_inputThread, &InputThread::keyPressed, this, &ChartMaker::onKeyPressed, Qt::QueuedConnection);

#ifdef Q_OS_WIN
    m_hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(m_hStdin, &m_dwOriginalMode);
#else
    tcgetattr(STDIN_FILENO, &m_originalTermios);
#endif
}

ChartMaker::~ChartMaker()
{
    m_inputThread->requestInterruption();
    m_inputThread->quit();
    m_inputThread->wait(1000); // 等待线程结束
    delete m_inputThread;

    restoreOriginalMode();
}

void ChartMaker::run(const QString& audioFilePath, const QString& outputJsonPath)
{
    enableRawMode();
    m_inputThread->start(); // 启动键盘监听线程

    m_outputFilePath = outputJsonPath;

    if (!QFile::exists(audioFilePath)) {
        qWarning() << "!!! FATAL: Audio file does not exist at path:" << audioFilePath;
        this->quit();
        return;
    }

    qDebug() << "Loading audio file:" << audioFilePath;
    qDebug() << "Press keys (D, F, Space, J, K) to the rhythm. Press Ctrl+C to stop and save.";

    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, &ChartMaker::onPlaybackStateChanged);
    m_player.setAudioOutput(&m_audioOutput);
    m_player.setSource(QUrl::fromLocalFile(audioFilePath));
    m_player.play();
}

void ChartMaker::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        if (!m_timer.isValid()) {
            m_timer.start();
            qDebug() << "Music started. Timer running. Start tapping!";
        }
    } else if (state == QMediaPlayer::StoppedState && m_player.position() > 0) {
        qDebug() << "Music finished. Saving chart...";
        this->quit();
    }
}

void ChartMaker::onKeyPressed(int key)
{
    if (m_player.isPlaying()) {
        qint64 ms = m_timer.elapsed();
        m_records.append({ms, key});
        // 使用QKeySequence将Qt::Key枚举值转换为可读字符串
        qDebug() << QString("Logged Key: %1 at %2 ms").arg(QKeySequence(key).toString()).arg(ms);
    }
}

void ChartMaker::saveChart()
{
    qDebug() << "Saving chart to" << m_outputFilePath;
    QJsonArray notesArray;
    for (const auto& record : m_records) {
        QJsonObject noteObject;
        int lane = -1;
        switch(record.key) {
        case Qt::Key_D: lane = 0; break;
        case Qt::Key_F: lane = 1; break;
        case Qt::Key_Space: lane = 2; break;
        case Qt::Key_J: lane = 3; break;
        case Qt::Key_K: lane = 4; break;
        }
        if (lane != -1) {
            noteObject["time"] = record.timestamp;
            noteObject["lane"] = lane;
            notesArray.append(noteObject);
        }
    }
    QJsonDocument doc(notesArray);
    QFile file(m_outputFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "Successfully saved" << notesArray.size() << "notes.";
    } else {
        qWarning() << "Error: Could not open file for writing:" << m_outputFilePath;
    }
}

void ChartMaker::enableRawMode()
{
#ifdef Q_OS_WIN
    DWORD newMode = m_dwOriginalMode;
    newMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    SetConsoleMode(m_hStdin, newMode);
#else
    struct termios newTermios = m_originalTermios;
    newTermios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
#endif
    qDebug() << "Terminal RAW mode enabled.";
}

void ChartMaker::restoreOriginalMode()
{
#ifdef Q_OS_WIN
    SetConsoleMode(m_hStdin, m_dwOriginalMode);
#else
    tcsetattr(STDIN_FILENO, TCSANOW, &m_originalTermios);
#endif
    qDebug() << "Terminal mode restored.";
}
