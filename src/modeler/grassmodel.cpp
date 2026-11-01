#include "grassmodel.h"

#include <QDomDocument>
#include <QFile>
#include <QTextStream>
#include <algorithm>

GrassModel::GrassModel(QObject *parent) : QObject(parent) {}

void GrassModel::clear()
{
    mActions.clear();
    mDataNodes.clear();
    mRelations.clear();
    mNextId = 0;
    emit modelChanged();
}

// ── Persistence (.gxm XML) ───────────────────────────────────────────────────
//
//  <model version="0.9" name="..." description="...">
//    <action id="1" name="r.slope.aspect" label="..." x="100" y="100" enabled="1">
//      <params><param name="elevation" value="dem"/></params>
//      <flags><flag name="a"/></flags>
//    </action>
//    <data id="2" type="raster" value="dem" label="DEM"
//          x="50" y="100" enabled="1" intermediate="0"/>
//    <relation id="3" from="2" to="1" param="elevation"/>
//  </model>

bool GrassModel::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QDomDocument doc;
    auto result = doc.setContent(&f);
    f.close();
    if (!result) return false;

    QDomElement root = doc.documentElement();
    if (root.tagName() != "model") return false;

    clear();
    mName        = root.attribute("name",        "Unnamed model");
    mDescription = root.attribute("description", {});

    auto intAttr = [](const QDomElement &e, const QString &n, int def = 0) {
        return e.hasAttribute(n) ? e.attribute(n).toInt() : def;
    };

    for (QDomElement e = root.firstChildElement("action");
         !e.isNull(); e = e.nextSiblingElement("action"))
    {
        ModelActionNode n;
        n.id         = intAttr(e, "id");
        n.moduleName = e.attribute("name");
        n.label      = e.attribute("label", n.moduleName);
        n.pos        = { e.attribute("x").toDouble(), e.attribute("y").toDouble() };
        n.enabled    = intAttr(e, "enabled", 1) != 0;

        QDomElement params = e.firstChildElement("params");
        for (QDomElement p = params.firstChildElement("param");
             !p.isNull(); p = p.nextSiblingElement("param"))
            n.params[p.attribute("name")] = p.attribute("value");

        QDomElement flags = e.firstChildElement("flags");
        for (QDomElement fl = flags.firstChildElement("flag");
             !fl.isNull(); fl = fl.nextSiblingElement("flag"))
            n.flags.append(fl.attribute("name"));

        mNextId = qMax(mNextId, n.id);
        mActions.append(n);
    }

    for (QDomElement e = root.firstChildElement("data");
         !e.isNull(); e = e.nextSiblingElement("data"))
    {
        ModelDataNode n;
        n.id           = intAttr(e, "id");
        n.type         = e.attribute("type",  "raster");
        n.value        = e.attribute("value", {});
        n.label        = e.attribute("label", n.value);
        n.pos          = { e.attribute("x").toDouble(), e.attribute("y").toDouble() };
        n.enabled      = intAttr(e, "enabled",      1) != 0;
        n.intermediate = intAttr(e, "intermediate", 0) != 0;

        mNextId = qMax(mNextId, n.id);
        mDataNodes.append(n);
    }

    for (QDomElement e = root.firstChildElement("relation");
         !e.isNull(); e = e.nextSiblingElement("relation"))
    {
        ModelRelation r;
        r.id        = intAttr(e, "id");
        r.fromId    = intAttr(e, "from");
        r.toId      = intAttr(e, "to");
        r.paramName = e.attribute("param");
        mNextId = qMax(mNextId, r.id);
        mRelations.append(r);
    }

    emit modelChanged();
    return true;
}

bool GrassModel::saveFile(const QString &path) const
{
    QDomDocument doc;
    QDomElement root = doc.createElement("model");
    root.setAttribute("version",     "0.9");
    root.setAttribute("name",        mName);
    root.setAttribute("description", mDescription);
    doc.appendChild(root);

    for (const ModelActionNode &n : mActions)
    {
        QDomElement e = doc.createElement("action");
        e.setAttribute("id",      n.id);
        e.setAttribute("name",    n.moduleName);
        e.setAttribute("label",   n.label);
        e.setAttribute("x",       n.pos.x());
        e.setAttribute("y",       n.pos.y());
        e.setAttribute("enabled", n.enabled ? 1 : 0);

        if (!n.params.isEmpty())
        {
            QDomElement pe = doc.createElement("params");
            for (auto it = n.params.cbegin(); it != n.params.cend(); ++it)
            {
                QDomElement p = doc.createElement("param");
                p.setAttribute("name",  it.key());
                p.setAttribute("value", it.value());
                pe.appendChild(p);
            }
            e.appendChild(pe);
        }

        if (!n.flags.isEmpty())
        {
            QDomElement fe = doc.createElement("flags");
            for (const QString &f : n.flags)
            {
                QDomElement fl = doc.createElement("flag");
                fl.setAttribute("name", f);
                fe.appendChild(fl);
            }
            e.appendChild(fe);
        }
        root.appendChild(e);
    }

    for (const ModelDataNode &n : mDataNodes)
    {
        QDomElement e = doc.createElement("data");
        e.setAttribute("id",           n.id);
        e.setAttribute("type",         n.type);
        e.setAttribute("value",        n.value);
        e.setAttribute("label",        n.label);
        e.setAttribute("x",            n.pos.x());
        e.setAttribute("y",            n.pos.y());
        e.setAttribute("enabled",      n.enabled      ? 1 : 0);
        e.setAttribute("intermediate", n.intermediate ? 1 : 0);
        root.appendChild(e);
    }

    for (const ModelRelation &r : mRelations)
    {
        QDomElement e = doc.createElement("relation");
        e.setAttribute("id",    r.id);
        e.setAttribute("from",  r.fromId);
        e.setAttribute("to",    r.toId);
        e.setAttribute("param", r.paramName);
        root.appendChild(e);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    doc.save(out, 2);
    return true;
}

// ── Mutation helpers ─────────────────────────────────────────────────────────

int GrassModel::addAction(const QString &module, const QPointF &pos,
                           const QString &label)
{
    ModelActionNode n;
    n.id         = nextId();
    n.moduleName = module;
    n.label      = label.isEmpty() ? module : label;
    n.pos        = pos;
    mActions.append(n);
    emit modelChanged();
    return n.id;
}

int GrassModel::addData(const QString &type, const QString &value,
                         const QPointF &pos, const QString &label)
{
    ModelDataNode n;
    n.id    = nextId();
    n.type  = type;
    n.value = value;
    n.label = label.isEmpty() ? value : label;
    n.pos   = pos;
    mDataNodes.append(n);
    emit modelChanged();
    return n.id;
}

int GrassModel::addRelation(int fromDataId, int toActionId,
                              const QString &paramName)
{
    ModelRelation r;
    r.id        = nextId();
    r.fromId    = fromDataId;
    r.toId      = toActionId;
    r.paramName = paramName;
    mRelations.append(r);
    emit modelChanged();
    return r.id;
}

bool GrassModel::removeNode(int id)
{
    mRelations.erase(
        std::remove_if(mRelations.begin(), mRelations.end(),
            [id](const ModelRelation &r){ return r.fromId == id || r.toId == id; }),
        mRelations.end());

    int removed = 0;
    mActions.erase(
        std::remove_if(mActions.begin(), mActions.end(),
            [id, &removed](const ModelActionNode &n){
                if (n.id == id) { ++removed; return true; } return false; }),
        mActions.end());

    if (!removed)
        mDataNodes.erase(
            std::remove_if(mDataNodes.begin(), mDataNodes.end(),
                [id, &removed](const ModelDataNode &n){
                    if (n.id == id) { ++removed; return true; } return false; }),
            mDataNodes.end());

    if (removed) { emit modelChanged(); return true; }
    return false;
}

bool GrassModel::removeRelation(int relId)
{
    const int before = mRelations.size();
    mRelations.erase(
        std::remove_if(mRelations.begin(), mRelations.end(),
            [relId](const ModelRelation &r){ return r.id == relId; }),
        mRelations.end());
    if (mRelations.size() < before) { emit modelChanged(); return true; }
    return false;
}

void GrassModel::setActionPos(int id, const QPointF &pos)
{
    if (auto *n = actionById(id)) { n->pos = pos; emit modelChanged(); }
}

void GrassModel::setDataPos(int id, const QPointF &pos)
{
    if (auto *n = dataById(id)) { n->pos = pos; emit modelChanged(); }
}

void GrassModel::setActionParam(int id, const QString &param,
                                 const QString &value)
{
    if (auto *n = actionById(id))
    {
        if (value.isEmpty()) n->params.remove(param);
        else n->params[param] = value;
        emit modelChanged();
    }
}

void GrassModel::setActionFlags(int id, const QStringList &flags)
{
    if (auto *n = actionById(id)) { n->flags = flags; emit modelChanged(); }
}

void GrassModel::setDataValue(int id, const QString &value)
{
    if (auto *n = dataById(id)) { n->value = value; emit modelChanged(); }
}

void GrassModel::setDataType(int id, const QString &type)
{
    if (auto *n = dataById(id)) { n->type = type; emit modelChanged(); }
}

void GrassModel::setNodeLabel(int id, const QString &label)
{
    if (auto *n = actionById(id)) { n->label = label; emit modelChanged(); return; }
    if (auto *n = dataById(id))   { n->label = label; emit modelChanged(); }
}

void GrassModel::setNodeEnabled(int id, bool enabled)
{
    if (auto *n = actionById(id)) { n->enabled = enabled; emit modelChanged(); return; }
    if (auto *n = dataById(id))   { n->enabled = enabled; emit modelChanged(); }
}

ModelActionNode *GrassModel::actionById(int id)
{
    for (auto &n : mActions) if (n.id == id) return &n;
    return nullptr;
}

ModelDataNode *GrassModel::dataById(int id)
{
    for (auto &n : mDataNodes) if (n.id == id) return &n;
    return nullptr;
}

ModelRelation *GrassModel::relationById(int id)
{
    for (auto &r : mRelations) if (r.id == id) return &r;
    return nullptr;
}

QVector<ModelRelation> GrassModel::relationsForNode(int nodeId) const
{
    QVector<ModelRelation> result;
    for (const auto &r : mRelations)
        if (r.fromId == nodeId || r.toId == nodeId)
            result.append(r);
    return result;
}

void GrassModel::setName(const QString &n)
{
    mName = n; emit modelChanged();
}

void GrassModel::setDescription(const QString &d)
{
    mDescription = d; emit modelChanged();
}
