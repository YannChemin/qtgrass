#pragma once

#include <QDialog>

class GrassMapCanvas;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;

// Export the current canvas view to PNG / JPEG / TIFF / PDF / SVG.
//
// "Screen resolution" renders at the widget's pixel dimensions.
// "High DPI" upscales by the chosen DPI factor before writing the file.
class GrassMapExportDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit GrassMapExportDialog( GrassMapCanvas *canvas,
                                    QWidget *parent = nullptr );

  private slots:
    void chooseFile();
    void doExport();
    void updatePreview();

  private:
    void exportPng(  const QString &path, int w, int h );
    void exportPdf(  const QString &path, int w, int h );
    void exportSvg(  const QString &path, int w, int h );

    GrassMapCanvas *mCanvas = nullptr;

    QLineEdit *mPathEdit   = nullptr;
    QComboBox *mFormatCombo= nullptr;
    QSpinBox  *mWidthSpin  = nullptr;
    QSpinBox  *mHeightSpin = nullptr;
    QLabel    *mPreview    = nullptr;
    QLabel    *mStatus     = nullptr;
};
