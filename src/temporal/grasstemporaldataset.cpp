#include "grasstemporaldataset.h"

#include <QProcess>

GrassTemporalDataset::GrassTemporalDataset(QObject *parent) : QObject(parent) {}

QStringList GrassTemporalDataset::listDatasets(const QString &type)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("t.list", {"type=" + type, "columns=name"});
    if (!p.waitForFinished(15000)) return {};

    QStringList result;
    for (const QString &raw : QString::fromLocal8Bit(p.readAll())
                              .split('\n', Qt::SkipEmptyParts))
    {
        const QString line = raw.trimmed();
        // Filter out header / separator lines produced by t.list
        if (line.isEmpty()       ||
            line.startsWith('-') ||
            line.startsWith("Space time") ||
            line.startsWith("No space") ||
            line.startsWith("No temporal") ||
            !line.contains('@'))
            continue;
        result.append(line);
    }
    return result;
}

bool GrassTemporalDataset::load(const QString &datasetName, DataType type)
{
    mName  = datasetName;
    mType  = type;
    mFrames.clear();
    mError.clear();

    const QString prog = (type == DataType::Vector)
                         ? "t.vect.list" : "t.rast.list";

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(prog, {"input="  + datasetName,
                   "separator=pipe",
                   "columns=name,start_time,end_time"});

    if (!p.waitForFinished(30000))
    {
        mError = prog + " timed out for " + datasetName;
        emit loadError(mError);
        return false;
    }

    if (p.exitCode() != 0)
    {
        mError = QString::fromLocal8Bit(p.readAll()).trimmed();
        if (mError.isEmpty())
            mError = prog + " failed for " + datasetName;
        emit loadError(mError);
        return false;
    }

    int idx = 0;
    for (const QString &raw : QString::fromLocal8Bit(p.readAll())
                              .split('\n', Qt::SkipEmptyParts))
    {
        const QString line = raw.trimmed();
        if (line.isEmpty() || !line.contains('@')) continue;

        const QStringList cols = line.split('|');
        if (cols.isEmpty()) continue;

        TemporalFrame f;
        f.index     = idx++;
        f.mapName   = cols[0].trimmed();
        f.startTime = (cols.size() > 1) ? cols[1].trimmed() : QString();
        f.endTime   = (cols.size() > 2) ? cols[2].trimmed() : QString();
        mFrames.append(f);
    }

    if (mFrames.isEmpty())
    {
        mError = "No maps found in dataset: " + datasetName;
        emit loadError(mError);
        return false;
    }

    emit loaded(mFrames.size());
    return true;
}
