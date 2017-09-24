#ifndef CATEGORIESWIDGET_H
#define CATEGORIESWIDGET_H

#include "categoriesmodel.h"

#include <QWidget>
#include <QPropertyAnimation>
#include <QDesktopWidget>

namespace Ui
{
    class CategoriesWidget;
}

class CategoriesWidget : public QWidget
{
Q_OBJECT

public:
    explicit CategoriesWidget(QWidget *parent = 0);

    ~CategoriesWidget() override;

signals:

    void categoriesChanged();

public slots:

    void resizeWindow(QSize size);

private slots:

    void animationFinished();

    void on_listWidget_data_clicked(const QModelIndex &index);

    void on_pushButton_add_clicked();

    void on_lineEdit_textChanged(const QString &arg1);

    void on_listWidget_data_doubleClicked(const QModelIndex &index);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Ui::CategoriesWidget *ui;
    QList<CategoriesModel *> mCategoriesModelList;

    void mSetListWidgetList();

    void mFiltrateListWidgetList();
};

#endif // CATEGORIESWIDGET_H
