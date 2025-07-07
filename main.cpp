#include "chartmaker.h"
#include <QTimer>
#include <QStringList>
#include <QDebug>

int main(int argc, char *argv[])
{
    ChartMaker a(argc, argv);

    QStringList args = a.arguments();
    if (args.size() < 3) {
        qWarning() << "Usage: ChartMaker <path/to/audio.wav> <path/to/output.json>";
        return 1;
    }

    QString audioFile = args.at(1);
    QString outputFile = args.at(2);

    QTimer::singleShot(0, &a, [&](){
        a.run(audioFile, outputFile);
    });

    return a.exec();
}
