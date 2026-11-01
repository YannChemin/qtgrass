#pragma once
#include <QObject>
#include <QString>
#include <QVector>

struct TemporalFrame {
    int     index     = 0;
    QString mapName;    // name@mapset
    QString startTime;  // ISO datetime string
    QString endTime;
};

// Wraps a GRASS space-time raster or vector dataset.
// Queries t.rast.list / t.vect.list for its frame list.
class GrassTemporalDataset : public QObject
{
    Q_OBJECT
public:
    enum class DataType { Raster, Vector };

    explicit GrassTemporalDataset(QObject *parent = nullptr);

    bool load(const QString &datasetName, DataType type = DataType::Raster);

    const QVector<TemporalFrame> &frames() const { return mFrames; }
    QString  name()       const { return mName;          }
    DataType dataType()   const { return mType;          }
    int      frameCount() const { return mFrames.size(); }
    QString  lastError()  const { return mError;         }

    // Lists available temporal datasets of a given type ("strds" / "stvds").
    static QStringList listDatasets(const QString &type = "strds");

signals:
    void loaded(int frameCount);
    void loadError(const QString &msg);

private:
    QString              mName;
    DataType             mType  = DataType::Raster;
    QString              mError;
    QVector<TemporalFrame> mFrames;
};
