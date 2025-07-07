#pragma once
#ifndef CHARTMAKER_H
#define CHARTMAKER_H

#include <QCoreApplication>
#include <QThread>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QList>

// 根据操作系统包含不同的底层API头文件
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

// 用于存储一次按键记录的数据结构
struct KeyEventRecord {
    qint64 timestamp;
    int key;
};

/**
 * @class InputThread
 * @brief 在后台独立线程中专门负责读取原始键盘输入
 */
class InputThread : public QThread
{
    Q_OBJECT
public:
    // 线程的执行体
    void run() override;
signals:
    // 当捕获到按键时，发出此信号
    void keyPressed(int key);
};


/**
 * @class ChartMaker
 * @brief 打谱工具的核心应用类
 */
class ChartMaker : public QCoreApplication
{
    Q_OBJECT
public:
    ChartMaker(int &argc, char **argv);
    ~ChartMaker();

    // 程序的执行入口
    void run(const QString& audioFilePath, const QString& outputJsonPath);

private slots:
    // 响应QMediaPlayer的状态变化
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    // 响应来自InputThread的按键信号
    void onKeyPressed(int key);
    // 在程序退出前保存谱面
    void saveChart();

private:
    // 终端模式控制
    void enableRawMode();
    void restoreOriginalMode();

    // 核心对象
    QMediaPlayer m_player;
    QAudioOutput m_audioOutput;
    QElapsedTimer m_timer;
    InputThread* m_inputThread;

    // 数据存储
    QList<KeyEventRecord> m_records;
    QString m_outputFilePath;

// 平台相关的成员变量
#ifdef Q_OS_WIN
    HANDLE m_hStdin;
    DWORD m_dwOriginalMode;
#else
    struct termios m_originalTermios;
#endif
};

#endif // CHARTMAKER_H
