#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QPointF>
#include <QVector>

// ── Data nodes ────────────────────────────────────────────────────────────────
struct ModelActionNode {
    int     id         = 0;
    QString label;
    QString moduleName;
    QMap<QString,QString> params;   // param name → value
    QStringList           flags;    // active flag names
    QPointF pos;
    bool    enabled    = true;
};

struct ModelDataNode {
    int     id           = 0;
    QString label;
    QString type;           // raster | vector | raster_3d | any
    QString value;          // map name or %variable%
    QPointF pos;
    bool    enabled      = true;
    bool    intermediate = false;   // produced by a module, not user-supplied
};

struct ModelRelation {
    int     id        = 0;
    int     fromId    = 0;  // data node id
    int     toId      = 0;  // action node id
    QString paramName;       // input param this data feeds
};

// ── Model container ───────────────────────────────────────────────────────────
class GrassModel : public QObject {
    Q_OBJECT
public:
    explicit GrassModel(QObject *parent = nullptr);

    void clear();
    bool loadFile(const QString &path);
    bool saveFile(const QString &path) const;

    int  addAction  (const QString &module, const QPointF &pos,
                     const QString &label = {});
    int  addData    (const QString &type,   const QString &value,
                     const QPointF &pos,   const QString &label = {});
    int  addRelation(int fromDataId, int toActionId, const QString &paramName);

    bool removeNode    (int id);
    bool removeRelation(int relId);

    void setActionPos  (int id, const QPointF &pos);
    void setDataPos    (int id, const QPointF &pos);
    void setActionParam(int id, const QString &param, const QString &value);
    void setActionFlags(int id, const QStringList &flags);
    void setDataValue  (int id, const QString &value);
    void setDataType   (int id, const QString &type);
    void setNodeLabel  (int id, const QString &label);
    void setNodeEnabled(int id, bool enabled);

    const QVector<ModelActionNode> &actions()   const { return mActions;   }
    const QVector<ModelDataNode>   &dataNodes() const { return mDataNodes; }
    const QVector<ModelRelation>   &relations() const { return mRelations; }

    ModelActionNode *actionById  (int id);
    ModelDataNode   *dataById    (int id);
    ModelRelation   *relationById(int id);
    QVector<ModelRelation> relationsForNode(int nodeId) const;

    QString name()        const { return mName;        }
    QString description() const { return mDescription; }
    void    setName       (const QString &n);
    void    setDescription(const QString &d);

signals:
    void modelChanged();

private:
    int nextId() { return ++mNextId; }

    QString mName        = "Unnamed model";
    QString mDescription;
    int     mNextId      = 0;

    QVector<ModelActionNode> mActions;
    QVector<ModelDataNode>   mDataNodes;
    QVector<ModelRelation>   mRelations;
};
