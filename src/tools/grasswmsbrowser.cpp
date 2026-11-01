#include "grasswmsbrowser.h"

#include <QApplication>
#include <QComboBox>
#include <QDomDocument>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

GrassWmsBrowserDialog::GrassWmsBrowserDialog(QWidget *parent)
    : QDialog(parent)
    , mNam(new QNetworkAccessManager(this))
{
    setWindowTitle("WMS / WMTS Browser");
    resize(700, 560);

    auto *vbox = new QVBoxLayout(this);

    // ── URL bar ──
    auto *urlRow = new QHBoxLayout;
    urlRow->addWidget(new QLabel("URL:", this));
    mUrlEdit = new QLineEdit(this);
    mUrlEdit->setPlaceholderText(
        "https://ows.terrestris.de/osm/service?SERVICE=WMS&VERSION=1.3.0");
    urlRow->addWidget(mUrlEdit, 1);
    auto *fetchBtn = new QPushButton("Get Capabilities", this);
    connect(fetchBtn, &QPushButton::clicked,
            this, &GrassWmsBrowserDialog::fetchCapabilities);
    urlRow->addWidget(fetchBtn);
    vbox->addLayout(urlRow);

    // ── Layer list ──
    auto *split = new QHBoxLayout;
    mLayerList = new QListWidget(this);
    mLayerList->setMinimumWidth(280);
    connect(mLayerList, &QListWidget::currentRowChanged,
            this, &GrassWmsBrowserDialog::onLayerSelected);
    split->addWidget(mLayerList, 1);

    // ── Layer details + import settings ──
    auto *detailBox = new QGroupBox("Layer", this);
    auto *dv = new QVBoxLayout(detailBox);
    mTitleLabel = new QLabel("—", detailBox);
    mTitleLabel->setWordWrap(true);
    dv->addWidget(mTitleLabel);

    auto *crsRow = new QFormLayout;
    mCrsCombo = new QComboBox(detailBox);
    crsRow->addRow("CRS:", mCrsCombo);
    mOutputEdit = new QLineEdit("wms_import", detailBox);
    crsRow->addRow("Output map:", mOutputEdit);
    dv->addLayout(crsRow);

    dv->addWidget(new QLabel(
        "<small>The current computational region defines the bounding box "
        "and output resolution.</small>", detailBox));
    dv->addStretch();

    auto *importBtn = new QPushButton("Import via r.in.wms", detailBox);
    importBtn->setDefault(true);
    connect(importBtn, &QPushButton::clicked,
            this, &GrassWmsBrowserDialog::importLayer);
    dv->addWidget(importBtn);
    split->addWidget(detailBox);
    vbox->addLayout(split, 1);

    // ── Status ──
    mStatusLabel = new QLabel("Enter a WMS URL and press Get Capabilities.", this);
    mStatusLabel->setWordWrap(true);
    mStatusLabel->setStyleSheet("color:#555; font-size:8pt;");
    vbox->addWidget(mStatusLabel);

    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto *brow = new QHBoxLayout;
    brow->addStretch(); brow->addWidget(closeBtn);
    vbox->addLayout(brow);
}

void GrassWmsBrowserDialog::fetchCapabilities()
{
    QString url = mUrlEdit->text().trimmed();
    if (url.isEmpty()) return;

    // Strip any existing query and add GetCapabilities
    const int q = url.indexOf('?');
    const QString base = (q >= 0) ? url.left(q) : url;
    const QString capUrl = base +
        "?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities";

    mStatusLabel->setText("Fetching capabilities from:\n" + capUrl);
    mLayerList->clear();
    mLayers.clear();

    QUrl qurl{capUrl};
    QNetworkRequest req{qurl};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = mNam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            mStatusLabel->setText("Error: " + reply->errorString());
            return;
        }
        parseCapabilities(reply->readAll());
    });
}

void GrassWmsBrowserDialog::parseCapabilities(const QByteArray &xml)
{
    QDomDocument doc;
    auto res = doc.setContent(xml);
    if (!res)
    {
        mStatusLabel->setText("Failed to parse XML: " + res.errorMessage);
        return;
    }

    QDomElement root = doc.documentElement();
    QDomElement cap  = root.firstChildElement("Capability");
    if (cap.isNull()) cap = root.firstChildElement("capability");

    mLayers.clear();
    parseLayers(cap, mLayers);

    mLayerList->clear();
    for (const auto &l : mLayers)
        mLayerList->addItem(l.title.isEmpty() ? l.name : l.title + " [" + l.name + "]");

    mStatusLabel->setText(QString("Found %1 layers.").arg(mLayers.size()));
    if (!mLayers.isEmpty()) mLayerList->setCurrentRow(0);
}

void GrassWmsBrowserDialog::parseLayers(const QDomElement &parent,
                                         QVector<WmsLayer> &out)
{
    for (QDomElement el = parent.firstChildElement("Layer");
         !el.isNull(); el = el.nextSiblingElement("Layer"))
    {
        WmsLayer l;
        l.name  = el.firstChildElement("Name").text().trimmed();
        l.title = el.firstChildElement("Title").text().trimmed();

        for (QDomElement crs = el.firstChildElement("CRS");
             !crs.isNull(); crs = crs.nextSiblingElement("CRS"))
            l.crsList.append(crs.text().trimmed());
        for (QDomElement srs = el.firstChildElement("SRS");
             !srs.isNull(); srs = srs.nextSiblingElement("SRS"))
            l.crsList.append(srs.text().trimmed());
        l.crsList.removeDuplicates();

        if (!l.name.isEmpty()) out.append(l);
        parseLayers(el, out);   // recurse into nested Layer elements
    }
}

void GrassWmsBrowserDialog::onLayerSelected(int row)
{
    if (row < 0 || row >= mLayers.size()) return;
    const auto &l = mLayers[row];
    mTitleLabel->setText(QString("<b>%1</b><br/><small>%2</small>")
                         .arg(l.title, l.name));
    mCrsCombo->clear();
    mCrsCombo->addItems(l.crsList.isEmpty()
                        ? QStringList{"EPSG:4326"} : l.crsList);
    // Prefer EPSG:4326 or EPSG:3857
    int idx = mCrsCombo->findText("EPSG:4326");
    if (idx < 0) idx = mCrsCombo->findText("EPSG:3857");
    if (idx >= 0) mCrsCombo->setCurrentIndex(idx);

    if (mOutputEdit->text() == "wms_import" && !l.name.isEmpty())
    {
        const QString safe = QString(l.name).replace(QRegularExpression("[^A-Za-z0-9_]"), "_");
        mOutputEdit->setText(safe.left(30));
    }
}

void GrassWmsBrowserDialog::importLayer()
{
    const int row = mLayerList->currentRow();
    if (row < 0 || row >= mLayers.size())
    {
        mStatusLabel->setText("Select a layer first.");
        return;
    }

    const auto &l   = mLayers[row];
    QString url      = mUrlEdit->text().trimmed();
    const int q      = url.indexOf('?');
    const QString base = (q >= 0) ? url.left(q) : url;

    QString srs = mCrsCombo->currentText().trimmed();
    // r.in.wms 'srs' parameter is just the numeric EPSG code
    srs.remove(QRegularExpression("^EPSG:", QRegularExpression::CaseInsensitiveOption));

    const QString output = mOutputEdit->text().trimmed();
    if (output.isEmpty()) { mStatusLabel->setText("Enter an output map name."); return; }

    const QStringList args = {"url=" + base,
                               "layers=" + l.name,
                               "srs="    + srs,
                               "output=" + output,
                               "method=nearest",
                               "--overwrite"};

    mStatusLabel->setText("Running: r.in.wms " + args.join(' '));
    QApplication::processEvents();

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("r.in.wms", args);
    if (!p.waitForFinished(300000))
    {
        mStatusLabel->setText("r.in.wms timed out (>5 min).");
        return;
    }

    const QString out = QString::fromLocal8Bit(p.readAll()).trimmed();
    if (p.exitCode() != 0)
        mStatusLabel->setText("Error: " + out.left(300));
    else
        mStatusLabel->setText("Imported → " + output + "  (" + l.title + ")");
}
