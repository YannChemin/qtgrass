#include "grassnetanalysis.h"
#include "../grassinterface.h"
#include "../module/grassmoduledialog.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

struct NetTool {
    QString module;
    QString label;
    QString description;
};

static const QVector<NetTool> kNetTools = {
    { "v.net",              "Build network",
      "Create a network by connecting a vector line map to a vector point map." },
    { "v.net.path",         "Shortest path",
      "Find the shortest path(s) between pairs of nodes using Dijkstra." },
    { "v.net.iso",          "Isodistance / isochrone",
      "Partition network nodes into classes by cost distance from centres." },
    { "v.net.alloc",        "Allocate to centres",
      "Assign network arcs to the nearest centre (Voronoi on network)." },
    { "v.net.salesman",     "Traveling salesman",
      "Compute an approximate optimal route visiting all specified nodes." },
    { "v.net.distance",     "Node-to-node distance",
      "Compute shortest distance between sets of source and destination nodes." },
    { "v.net.flow",         "Maximum flow",
      "Compute maximum flow / minimum cut in a directed network." },
    { "v.net.bridge",       "Bridges & articulation",
      "Detect bridges and articulation points in an undirected network." },
    { "v.net.components",   "Connected components",
      "Find weakly or strongly connected components of the network." },
    { "v.net.visibility",   "Visibility network",
      "Build a visibility graph from a set of points." },
    { "v.net.centrality",   "Centrality measures",
      "Compute betweenness, closeness and eigenvector centrality for nodes." },
    { "v.net.steiner",      "Steiner tree",
      "Find the minimum Steiner tree connecting a set of terminal nodes." },
};

GrassNetAnalysisDialog::GrassNetAnalysisDialog(GrassInterface *iface, QWidget *parent)
    : QDialog(parent), mIface(iface)
{
    setWindowTitle("Network Analysis (v.net.*)");
    resize(620, 540);

    auto *outerV = new QVBoxLayout(this);
    outerV->addWidget(new QLabel(
        "Select an operation to open its full parameter dialog:", this));

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *content = new QWidget(scroll);
    scroll->setWidget(content);

    auto *grid = new QGridLayout(content);
    grid->setSpacing(8);
    grid->setColumnStretch(1, 1);

    int row = 0;
    for (const auto &t : kNetTools)
    {
        auto *lbl = new QLabel(
            QString("<b>%1</b><br/><small style='color:#555'>%2</small><br/>"
                    "<code style='color:#338'>%3</code>")
            .arg(t.label, t.description, t.module), content);
        lbl->setWordWrap(true);
        lbl->setTextFormat(Qt::RichText);
        grid->addWidget(lbl, row, 0, 1, 2);

        auto *btn = new QPushButton("Launch…", content);
        btn->setFixedWidth(90);
        const QString mod = t.module;
        connect(btn, &QPushButton::clicked, this, [this, mod] {
            auto *dlg = new GrassModuleDialog(mod, {}, mIface, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
        grid->addWidget(btn, row, 2, Qt::AlignRight | Qt::AlignTop);

        if (row < kNetTools.size() - 1)
        {
            auto *sep = new QFrame(content);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("color:#ddd;");
            grid->addWidget(sep, ++row, 0, 1, 3);
        }
        ++row;
    }

    outerV->addWidget(scroll, 1);

    auto *closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto *brow = new QHBoxLayout;
    brow->addStretch(); brow->addWidget(closeBtn);
    outerV->addLayout(brow);
}
