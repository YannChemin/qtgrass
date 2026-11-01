#include "grasstplot.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>
#include <limits>
#include <cmath>

// ─── TplotChart ───────────────────────────────────────────────────────────────

TplotChart::TplotChart(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);
    QPalette pal = palette(); pal.setColor(QPalette::Window, Qt::white); setPalette(pal);
}

void TplotChart::setData(const QVector<QPair<double,double>> &data,
                          const QStringList &xLabels)
{
    mData    = data;
    mXLabels = xLabels;
    update();
}

void TplotChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    const QRectF plot(55, 15, width() - 70, height() - 55);
    p.fillRect(plot, QColor(248, 248, 252));
    p.setPen(QPen(QColor(200, 200, 210)));
    p.drawRect(plot);

    if (mData.isEmpty())
    {
        p.setPen(Qt::gray);
        QFont f; f.setPointSize(10); p.setFont(f);
        p.drawText(plot, Qt::AlignCenter, "No data.\nSet coordinates and run the plot.");
        return;
    }

    // Value range
    double lo =  std::numeric_limits<double>::max();
    double hi = -std::numeric_limits<double>::max();
    for (const auto &pt : mData)
    {
        if (!std::isnan(pt.second)) { lo = qMin(lo, pt.second); hi = qMax(hi, pt.second); }
    }
    if (lo >= hi) { lo -= 1; hi += 1; }
    const double margin = (hi - lo) * 0.05;
    lo -= margin; hi += margin;

    const int n = mData.size();
    auto mapX = [&](int i) { return plot.left() + (n > 1 ? i * plot.width() / (n - 1) : plot.width() / 2); };
    auto mapY = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };

    // Grid lines (5 horizontal)
    QFont sf; sf.setPointSize(7); p.setFont(sf);
    p.setPen(QPen(QColor(220, 220, 230), 0.5));
    for (int i = 0; i <= 5; ++i)
    {
        const double v = lo + i * (hi - lo) / 5;
        const double py = mapY(v);
        p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
        p.setPen(Qt::black);
        p.drawText(QRectF(2, py - 7, 50, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', 2));
        p.setPen(QPen(QColor(220, 220, 230), 0.5));
    }

    // Line + points
    QPainterPath path;
    bool first = true;
    for (int i = 0; i < n; ++i)
    {
        if (std::isnan(mData[i].second)) { first = true; continue; }
        const QPointF pt(mapX(i), mapY(mData[i].second));
        if (first) { path.moveTo(pt); first = false; }
        else path.lineTo(pt);
    }
    p.setPen(QPen(QColor(30, 100, 200), 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Dots
    p.setBrush(QColor(30, 100, 200));
    p.setPen(QPen(Qt::white, 1));
    for (int i = 0; i < n; ++i)
    {
        if (std::isnan(mData[i].second)) continue;
        p.drawEllipse(QPointF(mapX(i), mapY(mData[i].second)), 4, 4);
    }

    // X-axis labels (sample every N)
    p.setPen(Qt::black);
    p.setFont(sf);
    const int step = qMax(1, n / 8);
    for (int i = 0; i < n; i += step)
    {
        const QString lbl = (i < mXLabels.size()) ? mXLabels[i].left(10)
                                                   : QString::number(i + 1);
        p.save();
        p.translate(mapX(i), plot.bottom() + 3);
        p.rotate(30);
        p.drawText(QRectF(0, 0, 70, 14), Qt::AlignLeft, lbl);
        p.restore();
    }

    // Y-axis label
    p.save();
    p.translate(10, plot.center().y());
    p.rotate(-90);
    QFont yf; yf.setPointSize(8); p.setFont(yf);
    p.drawText(QRectF(-plot.height()/2, -10, plot.height(), 20),
               Qt::AlignCenter, "Value");
    p.restore();
}

// ─── GrassTplotDialog ────────────────────────────────────────────────────────

GrassTplotDialog::GrassTplotDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Temporal Time-Series Plot (tplot)");
    resize(740, 500);

    auto *vbox = new QVBoxLayout(this);

    // Controls
    auto *form = new QFormLayout;
    mStrdCombo = new QComboBox(this);
    mStrdCombo->setEditable(true);
    mStrdCombo->setMinimumWidth(200);
    loadStrds();
    form->addRow("STRDS:", mStrdCombo);

    auto *coordRow = new QHBoxLayout;
    mEastEdit  = new QLineEdit(this);  mEastEdit->setPlaceholderText("East");
    mNorthEdit = new QLineEdit(this);  mNorthEdit->setPlaceholderText("North");
    coordRow->addWidget(mEastEdit); coordRow->addWidget(mNorthEdit);
    form->addRow("E / N:", coordRow);

    vbox->addLayout(form);

    auto *btnRow = new QHBoxLayout;
    auto *plotBtn = new QPushButton("▶ Plot", this);
    plotBtn->setDefault(true);
    connect(plotBtn, &QPushButton::clicked, this, &GrassTplotDialog::runTplot);
    btnRow->addStretch(); btnRow->addWidget(plotBtn);
    vbox->addLayout(btnRow);

    mChart = new TplotChart(this);
    vbox->addWidget(mChart, 1);

    mStatusLabel = new QLabel("", this);
    mStatusLabel->setStyleSheet("font-size:8pt; color:#555;");
    auto *statRow = new QHBoxLayout;
    statRow->addWidget(mStatusLabel, 1);
    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    statRow->addWidget(closeBtn);
    vbox->addLayout(statRow);
}

void GrassTplotDialog::loadStrds()
{
    QProcess p;
    p.start("t.list", {"type=strds", "columns=name"});
    if (!p.waitForFinished(8000)) return;
    for (const QString &l : QString::fromLocal8Bit(p.readAllStandardOutput())
                            .split('\n', Qt::SkipEmptyParts))
    {
        const QString s = l.trimmed();
        if (s.isEmpty() || s.startsWith('-') || s.startsWith("Space time")
            || !s.contains('@')) continue;
        mStrdCombo->addItem(s);
    }
}

void GrassTplotDialog::runTplot()
{
    const QString strds  = mStrdCombo->currentText().trimmed();
    const QString east   = mEastEdit->text().trimmed();
    const QString north  = mNorthEdit->text().trimmed();

    if (strds.isEmpty() || east.isEmpty() || north.isEmpty())
    {
        mStatusLabel->setText("Fill in all fields.");
        return;
    }

    // Step 1: get map list + timestamps
    QProcess p1;
    p1.setProcessChannelMode(QProcess::MergedChannels);
    p1.start("t.rast.list",
             {"input=" + strds, "separator=pipe",
              "columns=name,start_time", "-u"});
    if (!p1.waitForFinished(30000))
    { mStatusLabel->setText("t.rast.list timed out."); return; }

    struct Frame { QString name; QString date; };
    QVector<Frame> frames;
    for (const QString &line :
         QString::fromLocal8Bit(p1.readAll()).split('\n', Qt::SkipEmptyParts))
    {
        const QStringList parts = line.split('|');
        if (parts.size() < 1 || !parts[0].contains('@')) continue;
        Frame f;
        f.name = parts[0].trimmed();
        f.date = (parts.size() > 1) ? parts[1].trimmed() : f.name;
        frames.append(f);
    }

    if (frames.isEmpty())
    { mStatusLabel->setText("No frames found in " + strds); return; }

    mStatusLabel->setText(
        QString("Querying %1 frames at E=%2, N=%3…").arg(frames.size()).arg(east, north));
    QApplication::processEvents();

    // Step 2: r.what for each frame
    QVector<QPair<double,double>> data;
    QStringList labels;
    const QString coords = east + "," + north;
    int done = 0;
    for (const Frame &fr : frames)
    {
        QProcess p2;
        p2.setProcessChannelMode(QProcess::MergedChannels);
        p2.start("r.what",
                 {"map=" + fr.name,
                  "coordinates=" + coords,
                  "separator=pipe"});
        p2.waitForFinished(10000);

        // Output: east|north|site|value
        const QString out = QString::fromLocal8Bit(p2.readAll()).trimmed();
        double val = std::numeric_limits<double>::quiet_NaN();
        const QStringList cols = out.split('|');
        if (cols.size() >= 4)
        {
            bool ok;
            const double v = cols[3].trimmed().toDouble(&ok);
            if (ok) val = v;
        }
        data.append({ static_cast<double>(done++), val });
        labels.append(fr.date.left(10));
    }

    mChart->setData(data, labels);
    const long valid = std::count_if(data.begin(), data.end(),
        [](const QPair<double,double> &pt){ return !std::isnan(pt.second); });
    mStatusLabel->setText(
        QString("%1 frames plotted (%2 valid values) for %3")
        .arg(frames.size()).arg(valid).arg(strds));
}
