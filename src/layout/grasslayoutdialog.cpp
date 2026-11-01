#include "grasslayoutdialog.h"
#include "grasslayoutitems.h"
#include "../grassinterface.h"
#include "../canvas/grassmapcanvas.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDomDocument>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontDialog>
#include <QFormLayout>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPageSize>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QSvgGenerator>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

// ─── GrassLayoutScene ─────────────────────────────────────────────────────────

GrassLayoutScene::GrassLayoutScene(QObject *parent) : QGraphicsScene(parent)
{
    setSceneRect(-50, -50, 310, 397);  // A4 portrait + margin
}

void GrassLayoutScene::setPageSize(double wMM, double hMM)
{
    mPageW = wMM; mPageH = hMM;
    setSceneRect(-30, -30, wMM + 60, hMM + 60);
    update();
}

void GrassLayoutScene::drawBackground(QPainter *p, const QRectF &)
{
    p->fillRect(sceneRect(), QColor(160, 165, 175));  // grey surround

    // Drop shadow
    p->fillRect(QRectF(4, 4, mPageW, mPageH), QColor(100, 100, 100, 100));
    // White page
    p->fillRect(QRectF(0, 0, mPageW, mPageH), Qt::white);
    // Page border
    p->setPen(QPen(QColor(100, 100, 100), 0.3));
    p->drawRect(QRectF(0, 0, mPageW, mPageH));
}

void GrassLayoutScene::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)
    {
        for (QGraphicsItem *item : selectedItems())
        {
            removeItem(item);
            delete item;
        }
        return;
    }
    QGraphicsScene::keyPressEvent(e);
}

// ─── GrassLayoutDialog ───────────────────────────────────────────────────────

GrassLayoutDialog::GrassLayoutDialog(GrassInterface *iface,
                                      GrassMapCanvas *canvas,
                                      QWidget *parent)
    : QDialog(parent)
    , mIface(iface)
    , mCanvas(canvas)
    , mScene(new GrassLayoutScene(this))
{
    setWindowTitle("Print Composer");
    resize(1100, 750);

    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    buildToolBar();

    auto *hSplit = new QSplitter(Qt::Horizontal, this);
    buildScene();
    hSplit->addWidget(mView);

    buildPropsPanel();
    hSplit->addWidget(mPropsScroll);
    hSplit->setStretchFactor(0, 3);
    hSplit->setStretchFactor(1, 1);
    hSplit->setSizes({790, 280});

    vbox->addWidget(hSplit, 1);

    connect(mScene, &QGraphicsScene::selectionChanged,
            this, &GrassLayoutDialog::onSelectionChanged);
    onSelectionChanged();  // initial state
}

// ─── Toolbar ────────────────────────────────────────────────────────────────

void GrassLayoutDialog::buildToolBar()
{
    auto *tb = new QToolBar(this);
    tb->setMovable(false);
    static_cast<QVBoxLayout *>(layout())->insertWidget(0, tb);

    tb->addAction("New", this, [this] {
        if (QMessageBox::question(this, "New Layout", "Clear current layout?",
                                   QMessageBox::Yes | QMessageBox::No)
            != QMessageBox::Yes) return;
        for (QGraphicsItem *item : mScene->items()) {
            mScene->removeItem(item); delete item;
        }
        mFilePath.clear();
        setWindowTitle("Print Composer");
    });

    tb->addAction("Open…", this, [this] {
        const QString f = QFileDialog::getOpenFileName(
            this, "Open Layout", {}, "GRASS Layouts (*.gpl);;All Files (*)");
        if (!f.isEmpty()) loadFile(f);
    });

    tb->addAction("Save", this, [this] {
        if (mFilePath.isEmpty())
            mFilePath = QFileDialog::getSaveFileName(
                this, "Save Layout", {}, "GRASS Layouts (*.gpl)");
        if (!mFilePath.isEmpty()) saveFile(mFilePath);
    });

    tb->addSeparator();

    tb->addAction("+ Map",      this, &GrassLayoutDialog::addMapFrame);
    tb->addAction("+ Legend",   this, &GrassLayoutDialog::addLegend);
    tb->addAction("+ Scale Bar",this, &GrassLayoutDialog::addScaleBar);
    tb->addAction("+ N Arrow",  this, &GrassLayoutDialog::addNorthArrow);
    tb->addAction("+ Text",     this, &GrassLayoutDialog::addText);
    tb->addAction("+ Image",    this, &GrassLayoutDialog::addImage);

    tb->addSeparator();

    tb->addAction("Zoom In",  this, [this] { mView->scale(1.2, 1.2); });
    tb->addAction("Zoom Out", this, [this] { mView->scale(1/1.2, 1/1.2); });
    tb->addAction("Fit",      this, [this] {
        mView->fitInView(QRectF(-10, -10, mPageW + 20, mPageH + 20),
                         Qt::KeepAspectRatio);
    });

    tb->addSeparator();

    tb->addAction("Export PDF…", this, &GrassLayoutDialog::doExportPdf);
    tb->addAction("Export PNG…", this, &GrassLayoutDialog::doExportPng);
    tb->addAction("Export SVG…", this, &GrassLayoutDialog::doExportSvg);

    tb->addSeparator();
    tb->addAction("Close", this, &QDialog::reject);
}

// ─── Scene view ──────────────────────────────────────────────────────────────

void GrassLayoutDialog::buildScene()
{
    mView = new QGraphicsView(mScene, this);
    mView->setRenderHint(QPainter::Antialiasing);
    mView->setRenderHint(QPainter::SmoothPixmapTransform);
    mView->setDragMode(QGraphicsView::RubberBandDrag);
    mView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    // Fit A4 page in view at startup
    mView->fitInView(QRectF(-10, -10, mPageW + 20, mPageH + 20),
                     Qt::KeepAspectRatio);
}

// ─── Properties panel ────────────────────────────────────────────────────────

void GrassLayoutDialog::buildPropsPanel()
{
    mPropsScroll = new QScrollArea(this);
    mPropsScroll->setMinimumWidth(240);
    mPropsScroll->setWidgetResizable(true);

    mPropsWidget = new QWidget(mPropsScroll);
    mPropsScroll->setWidget(mPropsWidget);

    auto *vbox = new QVBoxLayout(mPropsWidget);
    vbox->setSpacing(4);

    // Page settings group (always visible at top)
    auto *pageGroup = new QGroupBox("Page", mPropsWidget);
    auto *pform = new QFormLayout(pageGroup);

    mPageSizeCombo = new QComboBox(pageGroup);
    mPageSizeCombo->addItem("A4  210×297mm",  QSizeF(210, 297));
    mPageSizeCombo->addItem("A3  297×420mm",  QSizeF(297, 420));
    mPageSizeCombo->addItem("A2  420×594mm",  QSizeF(420, 594));
    mPageSizeCombo->addItem("Letter",         QSizeF(215.9, 279.4));
    mPageSizeCombo->addItem("Tabloid",        QSizeF(279.4, 431.8));
    pform->addRow("Size:",   mPageSizeCombo);

    mOrientCombo = new QComboBox(pageGroup);
    mOrientCombo->addItem("Portrait",  "portrait");
    mOrientCombo->addItem("Landscape", "landscape");
    pform->addRow("Orient:", mOrientCombo);

    mDpiSpin = new QSpinBox(pageGroup);
    mDpiSpin->setRange(72, 600);
    mDpiSpin->setValue(150);
    mDpiSpin->setSuffix(" DPI");
    pform->addRow("Export DPI:", mDpiSpin);

    auto *applyPageBtn = new QPushButton("Apply Page Settings", pageGroup);
    connect(applyPageBtn, &QPushButton::clicked,
            this, &GrassLayoutDialog::applyPageSettings);
    pform->addRow(applyPageBtn);
    vbox->addWidget(pageGroup);

    // Position/Size group (visible when item selected)
    auto *geomGroup = new QGroupBox("Position & Size (mm)", mPropsWidget);
    auto *gform = new QFormLayout(geomGroup);

    mXSpin = new QDoubleSpinBox(geomGroup); mXSpin->setRange(-999, 9999); mXSpin->setDecimals(1);
    mYSpin = new QDoubleSpinBox(geomGroup); mYSpin->setRange(-999, 9999); mYSpin->setDecimals(1);
    mWSpin = new QDoubleSpinBox(geomGroup); mWSpin->setRange(1, 9999);    mWSpin->setDecimals(1);
    mHSpin = new QDoubleSpinBox(geomGroup); mHSpin->setRange(1, 9999);    mHSpin->setDecimals(1);
    gform->addRow("X:", mXSpin);
    gform->addRow("Y:", mYSpin);
    gform->addRow("W:", mWSpin);
    gform->addRow("H:", mHSpin);

    auto *applyGeomBtn = new QPushButton("Apply Position/Size", geomGroup);
    connect(applyGeomBtn, &QPushButton::clicked, this, [this] {
        auto *item = selectedItem();
        if (!item) return;
        item->setPos(mXSpin->value(), mYSpin->value());
        item->setItemSize(mWSpin->value(), mHSpin->value());
    });
    gform->addRow(applyGeomBtn);
    vbox->addWidget(geomGroup);

    // Item-specific area — rebuilt by onSelectionChanged
    mPropsForm = new QFormLayout;
    vbox->addLayout(mPropsForm);
    vbox->addStretch();
}

void GrassLayoutDialog::clearProps()
{
    while (mPropsForm->rowCount() > 0)
        mPropsForm->removeRow(0);
}

void GrassLayoutDialog::applyPageSettings()
{
    QSizeF size = mPageSizeCombo->currentData().toSizeF();
    if (mOrientCombo->currentData().toString() == "landscape")
        size.transpose();
    mPageW = size.width();
    mPageH = size.height();
    mExportDpi = mDpiSpin->value();
    mScene->setPageSize(mPageW, mPageH);
    mView->fitInView(QRectF(-10, -10, mPageW + 20, mPageH + 20),
                     Qt::KeepAspectRatio);
}

// ─── Selection → properties panel ────────────────────────────────────────────

GrassLayoutItem *GrassLayoutDialog::selectedItem() const
{
    const auto sel = mScene->selectedItems();
    if (sel.size() == 1) return dynamic_cast<GrassLayoutItem *>(sel.first());
    return nullptr;
}

GrassLayoutMapItem *GrassLayoutDialog::findFirstMapItem() const
{
    for (QGraphicsItem *it : mScene->items())
        if (auto *m = dynamic_cast<GrassLayoutMapItem *>(it)) return m;
    return nullptr;
}

void GrassLayoutDialog::onSelectionChanged()
{
    clearProps();
    auto *item = selectedItem();
    if (!item)
    {
        mXSpin->setEnabled(false);
        mYSpin->setEnabled(false);
        mWSpin->setEnabled(false);
        mHSpin->setEnabled(false);
        return;
    }

    mXSpin->setEnabled(true); mXSpin->setValue(item->pos().x());
    mYSpin->setEnabled(true); mYSpin->setValue(item->pos().y());
    mWSpin->setEnabled(true); mWSpin->setValue(item->itemW());
    mHSpin->setEnabled(true); mHSpin->setValue(item->itemH());

    connect(item, &GrassLayoutItem::geometryChanged, this, [this, item] {
        if (selectedItem() != item) return;
        mXSpin->blockSignals(true); mXSpin->setValue(item->pos().x()); mXSpin->blockSignals(false);
        mYSpin->blockSignals(true); mYSpin->setValue(item->pos().y()); mYSpin->blockSignals(false);
        mWSpin->blockSignals(true); mWSpin->setValue(item->itemW());   mWSpin->blockSignals(false);
        mHSpin->blockSignals(true); mHSpin->setValue(item->itemH());   mHSpin->blockSignals(false);
    });

    populatePropsForItem(item);
}

void GrassLayoutDialog::populatePropsForItem(GrassLayoutItem *item)
{
    if (auto *mapItem = dynamic_cast<GrassLayoutMapItem *>(item))
    {
        Q_UNUSED(mapItem)
        auto *refreshBtn = new QPushButton("Refresh Map Frame");
        connect(refreshBtn, &QPushButton::clicked, this, [mapItem] { mapItem->update(); });
        mPropsForm->addRow(refreshBtn);
    }
    else if (auto *legItem = dynamic_cast<GrassLayoutLegendItem *>(item))
    {
        auto *titleEdit = new QLineEdit(legItem->title());
        connect(titleEdit, &QLineEdit::textChanged, this,
                [legItem](const QString &t) { legItem->setTitle(t); });
        mPropsForm->addRow("Title:", titleEdit);

        auto *fontBtn = new QPushButton("Font…");
        connect(fontBtn, &QPushButton::clicked, this, [this, legItem] {
            bool ok;
            const QFont f = QFontDialog::getFont(&ok, legItem->font(), this);
            if (ok) legItem->setFont(f);
        });
        mPropsForm->addRow("Font:", fontBtn);
    }
    else if (auto *sbItem = dynamic_cast<GrassLayoutScaleBarItem *>(item))
    {
        auto *unitsCombo = new QComboBox;
        unitsCombo->addItems({"m", "km", "mi"});
        unitsCombo->setCurrentText(sbItem->units());
        connect(unitsCombo, &QComboBox::currentTextChanged,
                this, [sbItem](const QString &u) { sbItem->setUnits(u); });
        mPropsForm->addRow("Units:", unitsCombo);

        auto *segSpin = new QSpinBox;
        segSpin->setRange(1, 10);
        segSpin->setValue(sbItem->segments());
        connect(segSpin, &QSpinBox::valueChanged,
                this, [sbItem](int n) { sbItem->setSegments(n); });
        mPropsForm->addRow("Segments:", segSpin);
    }
    else if (auto *txtItem = dynamic_cast<GrassLayoutTextItem *>(item))
    {
        auto *textEdit = new QTextEdit;
        textEdit->setMaximumHeight(80);
        textEdit->setPlainText(txtItem->text());
        connect(textEdit, &QTextEdit::textChanged, this, [textEdit, txtItem] {
            txtItem->setText(textEdit->toPlainText());
        });
        mPropsForm->addRow("Text:", textEdit);

        auto *fontBtn = new QPushButton("Font…");
        connect(fontBtn, &QPushButton::clicked, this, [this, txtItem] {
            bool ok;
            const QFont f = QFontDialog::getFont(&ok, txtItem->font(), this);
            if (ok) txtItem->setFont(f);
        });
        auto *colorBtn = new QPushButton("Color…");
        connect(colorBtn, &QPushButton::clicked, this, [this, txtItem] {
            const QColor c = QColorDialog::getColor(txtItem->color(), this);
            if (c.isValid()) txtItem->setColor(c);
        });
        auto *btnRow = new QWidget;
        auto *bh = new QHBoxLayout(btnRow);
        bh->setContentsMargins(0,0,0,0);
        bh->addWidget(fontBtn); bh->addWidget(colorBtn);
        mPropsForm->addRow(btnRow);

        auto *alignCombo = new QComboBox;
        alignCombo->addItem("Left",   static_cast<int>(Qt::AlignLeft   | Qt::AlignVCenter));
        alignCombo->addItem("Center", static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter));
        alignCombo->addItem("Right",  static_cast<int>(Qt::AlignRight  | Qt::AlignVCenter));
        alignCombo->setCurrentIndex(0);
        connect(alignCombo, &QComboBox::currentIndexChanged, this,
                [alignCombo, txtItem](int) {
                    txtItem->setAlignment(
                        static_cast<Qt::Alignment>(alignCombo->currentData().toInt()));
                });
        mPropsForm->addRow("Align:", alignCombo);
    }
    else if (auto *imgItem = dynamic_cast<GrassLayoutImageItem *>(item))
    {
        auto *pathLabel = new QLabel(imgItem->filePath());
        pathLabel->setWordWrap(true);
        auto *browseBtn = new QPushButton("Browse…");
        connect(browseBtn, &QPushButton::clicked, this, [this, imgItem, pathLabel] {
            const QString f = QFileDialog::getOpenFileName(
                this, "Select Image", {},
                "Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp *.svg);;All Files (*)");
            if (!f.isEmpty()) { imgItem->setFilePath(f); pathLabel->setText(f); }
        });
        mPropsForm->addRow("File:", pathLabel);
        mPropsForm->addRow(browseBtn);
    }
}

// ─── Add item helpers ─────────────────────────────────────────────────────────

static QPointF centreOf(double pageW, double pageH, double w, double h)
{
    return { (pageW - w) / 2, (pageH - h) / 2 };
}

void GrassLayoutDialog::addMapFrame()
{
    auto *item = new GrassLayoutMapItem(mCanvas, 170, 130);
    item->setPos(centreOf(mPageW, mPageH, 170, 130));
    mScene->addItem(item);
    mScene->clearSelection();
    item->setSelected(true);
}

void GrassLayoutDialog::addLegend()
{
    auto *item = new GrassLayoutLegendItem(mCanvas, 50, 70);
    item->setPos(mPageW - 60, 20);
    mScene->addItem(item);
    mScene->clearSelection();
    item->setSelected(true);
}

void GrassLayoutDialog::addScaleBar()
{
    auto *item = new GrassLayoutScaleBarItem(
        mCanvas, findFirstMapItem(), 80, 12);
    item->setPos(20, mPageH - 25);
    mScene->addItem(item);
    mScene->clearSelection();
    item->setSelected(true);
}

void GrassLayoutDialog::addNorthArrow()
{
    auto *item = new GrassLayoutNorthArrowItem(18, 24);
    item->setPos(mPageW - 30, 20);
    mScene->addItem(item);
    mScene->clearSelection();
    item->setSelected(true);
}

void GrassLayoutDialog::addText()
{
    bool ok;
    const QString text = QInputDialog::getText(
        this, "Add Text", "Text:", QLineEdit::Normal, "Map Title", &ok);
    if (!ok) return;

    auto *item = new GrassLayoutTextItem(text, 170, 14);
    item->setPos(centreOf(mPageW, mPageH, 170, 14).x(), 8);
    mScene->addItem(item);
    mScene->clearSelection();
    item->setSelected(true);
}

void GrassLayoutDialog::addImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Image", {},
        "Images (*.png *.jpg *.jpeg *.tif *.bmp *.svg);;All Files (*)");
    auto *item = new GrassLayoutImageItem(path, 30, 30);
    item->setPos(10, mPageH - 45);
    mScene->addItem(item);
    mScene->clearSelection();
    item->setSelected(true);
}

// ─── Save / load ─────────────────────────────────────────────────────────────

void GrassLayoutDialog::saveFile(const QString &path)
{
    QDomDocument doc;
    QDomElement root = doc.createElement("grasslayout");
    root.setAttribute("version",   "1.0");
    root.setAttribute("pageWidth",  mPageW);
    root.setAttribute("pageHeight", mPageH);
    root.setAttribute("dpi",        mExportDpi);
    doc.appendChild(root);

    QDomElement items = doc.createElement("items");
    for (QGraphicsItem *gi : mScene->items())
    {
        auto *li = dynamic_cast<GrassLayoutItem *>(gi);
        if (!li) continue;
        QDomElement el = doc.createElement("item");
        li->saveToXml(el);
        items.appendChild(el);
    }
    root.appendChild(items);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Save failed", "Cannot write: " + path);
        return;
    }
    QTextStream out(&f);
    doc.save(out, 2);
    setWindowTitle("Print Composer — " + QFileInfo(path).baseName());
}

void GrassLayoutDialog::loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Load failed", "Cannot read: " + path);
        return;
    }
    QDomDocument doc;
    auto result = doc.setContent(&f);
    f.close();
    if (!result) { QMessageBox::warning(this, "Parse error", path); return; }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "grasslayout") return;

    mPageW    = root.attribute("pageWidth",  "210").toDouble();
    mPageH    = root.attribute("pageHeight", "297").toDouble();
    mExportDpi = root.attribute("dpi", "150").toInt();
    mScene->setPageSize(mPageW, mPageH);

    // Clear existing items
    for (QGraphicsItem *gi : mScene->items()) { mScene->removeItem(gi); delete gi; }

    GrassLayoutMapItem *lastMapItem = nullptr;
    QDomElement itemsEl = root.firstChildElement("items");
    for (QDomElement el = itemsEl.firstChildElement("item");
         !el.isNull(); el = el.nextSiblingElement("item"))
    {
        const QString t = el.attribute("type");
        GrassLayoutItem *li = nullptr;

        if      (t == "map")
            { auto *m = new GrassLayoutMapItem(mCanvas, 1, 1); lastMapItem = m; li = m; }
        else if (t == "legend")
            li = new GrassLayoutLegendItem(mCanvas, 1, 1);
        else if (t == "scalebar")
            li = new GrassLayoutScaleBarItem(mCanvas, lastMapItem, 1, 1);
        else if (t == "northarrow")
            li = new GrassLayoutNorthArrowItem(1, 1);
        else if (t == "text")
            li = new GrassLayoutTextItem({}, 1, 1);
        else if (t == "image")
            li = new GrassLayoutImageItem({}, 1, 1);

        if (li) { li->loadFromXml(el); mScene->addItem(li); }
    }

    mFilePath = path;
    setWindowTitle("Print Composer — " + QFileInfo(path).baseName());
}

// ─── Export ──────────────────────────────────────────────────────────────────

void GrassLayoutDialog::renderToDevice(QPainter *painter,
                                        double scaleX, double scaleY)
{
    // Temporarily deselect all to hide handles
    QList<QGraphicsItem *> wasSelected;
    for (QGraphicsItem *it : mScene->items())
        if (it->isSelected()) { wasSelected.append(it); it->setSelected(false); }

    painter->scale(scaleX, scaleY);
    mScene->render(painter, QRectF(), QRectF(0, 0, mPageW, mPageH));

    for (QGraphicsItem *it : wasSelected) it->setSelected(true);
}

void GrassLayoutDialog::doExportPng()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export PNG", {}, "PNG Image (*.png)");
    if (path.isEmpty()) return;

    const double pxPerMM = mExportDpi / 25.4;
    const int w = qRound(mPageW * pxPerMM);
    const int h = qRound(mPageH * pxPerMM);

    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::white);
    img.setDotsPerMeterX(qRound(mExportDpi / 0.0254));
    img.setDotsPerMeterY(qRound(mExportDpi / 0.0254));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    renderToDevice(&p, pxPerMM, pxPerMM);
    p.end();

    if (!img.save(path, "PNG"))
        QMessageBox::warning(this, "Export failed", "Cannot save PNG: " + path);
    else
        QMessageBox::information(this, "PNG exported",
            QString("Saved %1×%2 px @ %3 DPI\n%4").arg(w).arg(h).arg(mExportDpi).arg(path));
}

void GrassLayoutDialog::doExportPdf()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export PDF", {}, "PDF (*.pdf)");
    if (path.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QSizeF(mPageW, mPageH), QPageSize::Millimeter));
    printer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    printer.setResolution(mExportDpi);

    QPainter p(&printer);
    const double pxPerMM = printer.resolution() / 25.4;
    renderToDevice(&p, pxPerMM, pxPerMM);
    p.end();

    QMessageBox::information(this, "PDF exported", "Saved: " + path);
}

void GrassLayoutDialog::doExportSvg()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export SVG", {}, "SVG (*.svg)");
    if (path.isEmpty()) return;

    const double pxPerMM = 3.7795;  // 96 DPI ≈ standard SVG unit
    const int w = qRound(mPageW * pxPerMM);
    const int h = qRound(mPageH * pxPerMM);

    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(QSize(w, h));
    gen.setViewBox(QRect(0, 0, w, h));
    gen.setResolution(96);
    gen.setTitle(windowTitle());

    QPainter p(&gen);
    renderToDevice(&p, pxPerMM, pxPerMM);
    p.end();

    QMessageBox::information(this, "SVG exported", "Saved: " + path);
}
