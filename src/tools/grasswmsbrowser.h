#pragma once

#include <QDialog>
#include <QDomElement>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;

struct WmsLayer {
    QString name;
    QString title;
    QStringList crsList;
};

// ── WMS / WMTS browser and importer ──────────────────────────────────────────
// Fetches a WMS GetCapabilities document, parses the layer list, and
// lets the user import a layer via r.in.wms.
class GrassWmsBrowserDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GrassWmsBrowserDialog(QWidget *parent = nullptr);

private:
    void fetchCapabilities();
    void parseCapabilities(const QByteArray &xml);
    void parseLayers(const QDomElement &parent, QVector<WmsLayer> &out);
    void onLayerSelected(int row);
    void importLayer();

    QLineEdit   *mUrlEdit     = nullptr;
    QListWidget *mLayerList   = nullptr;
    QLabel      *mTitleLabel  = nullptr;
    QComboBox   *mCrsCombo    = nullptr;
    QLineEdit   *mOutputEdit  = nullptr;
    QLabel      *mStatusLabel = nullptr;

    QNetworkAccessManager *mNam   = nullptr;
    QVector<WmsLayer>      mLayers;
};
