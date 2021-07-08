#pragma once

#include "defines.hpp"

class PrioritySpinBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    PrioritySpinBoxDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
/*
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const override;
*/
};

class CatIdSpinBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    CatIdSpinBoxDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

class DegreesSpinBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    DegreesSpinBoxDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

class GainSpinBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    GainSpinBoxDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

class FrequencySpinBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    FrequencySpinBoxDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;
};

class YesNoComboBoxItemDelegate : public QStyledItemDelegate {

   Q_OBJECT

 public:
   YesNoComboBoxItemDelegate(QObject *parent = nullptr);
   ~YesNoComboBoxItemDelegate();

   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   void setEditorData(QWidget *editor, const QModelIndex &index) const override;
   void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
   void paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const override;
};

class RadioComboBoxItemDelegate : public QStyledItemDelegate {

   Q_OBJECT

 public:
   RadioComboBoxItemDelegate(QSettings &s, QObject *parent = nullptr);
   ~RadioComboBoxItemDelegate();

   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   void setEditorData(QWidget *editor, const QModelIndex &index) const override;
   void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
   void paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const override;

  private:
    QSettings& settings;
};

class AuxComboBoxItemDelegate : public QStyledItemDelegate {

   Q_OBJECT

 public:
   AuxComboBoxItemDelegate(QObject *parent = nullptr);
   ~AuxComboBoxItemDelegate();

   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   void setEditorData(QWidget *editor, const QModelIndex &index) const override;
   void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
   void paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const override;
};

class CompassComboBoxItemDelegate : public QStyledItemDelegate {

   Q_OBJECT

 public:
   CompassComboBoxItemDelegate(QObject *parent = nullptr);
   ~CompassComboBoxItemDelegate();

   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   void setEditorData(QWidget *editor, const QModelIndex &index) const override;
   void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
   void paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const override;
};

class PortComboBoxItemDelegate : public QStyledItemDelegate {

   Q_OBJECT

 public:
   PortComboBoxItemDelegate(QObject *parent = nullptr);
   ~PortComboBoxItemDelegate();

   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   void setEditorData(QWidget *editor, const QModelIndex &index) const override;
   void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
};

class VantComboBoxItemDelegate : public QStyledItemDelegate {

   Q_OBJECT

 public:
   VantComboBoxItemDelegate(QObject *parent = nullptr);
   ~VantComboBoxItemDelegate();

   QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
   void setEditorData(QWidget *editor, const QModelIndex &index) const override;
   void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
};
