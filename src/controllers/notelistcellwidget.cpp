#include "notelistcellwidget.h"
#include "ui_notelistcellwidget.h"

#include "tools.h"
#include <QDebug>

NoteListCellWidget::NoteListCellWidget(NoteModel *noteModel, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NoteListCellWidget),
    mNoteModel(noteModel)
{
    ui->setupUi(this);
    reloadData();
}

NoteListCellWidget::~NoteListCellWidget()
{
    delete ui;
}

void NoteListCellWidget::reloadData()
{
    ui->label_title->setText(mNoteModel->getTitle());
    ui->frame_body->setText(mNoteModel->getNoteText());
    ui->label_categoryList->setText(mNoteModel->getCategory());
    ui->label_tagList->setText(mNoteModel->getStringFormTagList());
}

void NoteListCellWidget::paintEvent(QPaintEvent *event)
{
//    qDebug()  << mNoteModel->getTitle();

    QWidget::paintEvent(event);
}
