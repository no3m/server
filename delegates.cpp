#include "delegates.hpp"

// spinboxes

PrioritySpinBoxDelegate::PrioritySpinBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *PrioritySpinBoxDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &/* option */,
                                       const QModelIndex &/* index */) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setFrame(false);
    editor->setMinimum(0);
    editor->setMaximum(100);

    return editor;
}
void PrioritySpinBoxDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->setValue(value);
}
void PrioritySpinBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->interpretText();
    int value = spinBox->value();

    model->setData(index, value, Qt::EditRole);
}
void PrioritySpinBoxDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}
/*
void PrioritySpinBoxDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
      //QSpinBox* _spinBox = new QSpinBox;
    	//_spinBox->setGeometry(option.rect);
    	//_spinBox->setValue(index.data().toInt());
    	//QPixmap map = QPixmap::grabWidget(_spinBox);
    	//painter->drawPixmap(option.rect.x(), option.rect.y(), map);
      //painter->drawText(option.rect, QString::number(index.model()->data(index, Qt::EditRole).toInt()));
}
*/
/*
// just a reference
void customStyledItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{
    QStyleOptionViewItem subop(option);
    subop.state &= ~(QStyle::State_Selected);
    QStyledItemDelegate::paint(painter, subop, index);

//>      QStyleOptionViewItemV4 subop = option;
//>      QPalette pal = qApp->palette();
//>      if( selected ) {
//>         pal.setColor(QPalette::Highlight,Qt::darkGray);
//>         pal.setColor(QPalette::WindowText,Qt::white);
//>         pal.setColor(QPalette::HighlightedText,Qt::white);
//>         pal.setColor(QPalette::Text,Qt::white);
//>         subop.state |= QStyle::State_Selected;
//>         subop.palette = pal;
//>     }
//>
//>     BaseClass::paint(painter,subop,index);	// Note: subop.
}
*/

CatIdSpinBoxDelegate::CatIdSpinBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *CatIdSpinBoxDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &/* option */,
                                       const QModelIndex &/* index */) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setFrame(false);
    editor->setMinimum(0);
    editor->setMaximum(1024);

    return editor;
}
void CatIdSpinBoxDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->setValue(value);
}
void CatIdSpinBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->interpretText();
    int value = spinBox->value();

    model->setData(index, value, Qt::EditRole);
}
void CatIdSpinBoxDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}


DegreesSpinBoxDelegate::DegreesSpinBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *DegreesSpinBoxDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &/* option */,
                                       const QModelIndex &/* index */) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setFrame(false);
    editor->setMinimum(0);
    editor->setMaximum(359);

    return editor;
}
void DegreesSpinBoxDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->setValue(value);
}
void DegreesSpinBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->interpretText();
    int value = spinBox->value();

    model->setData(index, value, Qt::EditRole);
}
void DegreesSpinBoxDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}

GainSpinBoxDelegate::GainSpinBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *GainSpinBoxDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &/* option */,
                                       const QModelIndex &/* index */) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setFrame(false);
    editor->setMinimum(-32);
    editor->setMaximum(32);

    return editor;
}
void GainSpinBoxDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->setValue(value);
}
void GainSpinBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->interpretText();
    int value = spinBox->value();

    model->setData(index, value, Qt::EditRole);
}
void GainSpinBoxDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}

FrequencySpinBoxDelegate::FrequencySpinBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *FrequencySpinBoxDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &/* option */,
                                       const QModelIndex &/* index */) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setFrame(false);
    editor->setMinimum(1);
    editor->setMaximum(30000);

    return editor;
}
void FrequencySpinBoxDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->setValue(value);
}
void FrequencySpinBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
    spinBox->interpretText();
    int value = spinBox->value();

    model->setData(index, value, Qt::EditRole);
}
void FrequencySpinBoxDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}


// comboboxes

YesNoComboBoxItemDelegate::YesNoComboBoxItemDelegate(QObject *parent)
                          : QStyledItemDelegate(parent)
{

}

YesNoComboBoxItemDelegate::~YesNoComboBoxItemDelegate()
{

}

QWidget *YesNoComboBoxItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &/*option*/,
                                            const QModelIndex &/*index*/) const
{
   // Create the combobox and populate it
   QComboBox *cb = new QComboBox(parent);
   //const int row = index.row();
   //cb->addItem(QString("one in row %1").arg(row));
   cb->insertItem(0, kNo);
   cb->insertItem(1, kYes);
   return cb;
}

void YesNoComboBoxItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   // get the index of the text in the combobox that matches the current value of the item
   //const QString currentText = index.data(Qt::EditRole).toString();
   //const int cbIndex = cb->findText(currentText);
   // if it is valid, adjust the combobox
   //if (cbIndex >= 0)
   //   cb->setCurrentIndex(cbIndex);
   cb->setCurrentIndex(index.data(Qt::EditRole).toInt());
}

void YesNoComboBoxItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   //model->setData(index, cb->currentText(), Qt::EditRole);
   model->setData(index, cb->currentIndex(), Qt::EditRole);
}

void YesNoComboBoxItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{

  //if (option.state & QStyle::State_Selected)
  // painter->fillRect(option.rect, option.palette.highlight());

  QString text;
  int value = index.data(Qt::EditRole).toInt();
  if (value) text = kYes;
  else text = kNo;
  painter->drawText(option.rect.adjusted(5, 0, 0, 0), Qt::AlignVCenter|Qt::AlignLeft, text);
}

RadioComboBoxItemDelegate::RadioComboBoxItemDelegate(QSettings &s, QObject *parent)
                          : QStyledItemDelegate(parent), settings(s)
{

}

RadioComboBoxItemDelegate::~RadioComboBoxItemDelegate()
{

}

QWidget *RadioComboBoxItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &/*option*/,
                                            const QModelIndex &/*index*/) const
{
   // Create the combobox and populate it
   QComboBox *cb = new QComboBox(parent);
   //const int row = index.row();
   //cb->addItem(QString("one in row %1").arg(row));
   for (int i = 0; i<NRIG; ++i) {
     //cb->insertItem(i, QString::number(i+1));
     cb->insertItem(i, settings.value(s_radioName[i], QString::number(i)).toString());
   }
   //cb->setMaxVisibleItems(10);
   cb->setStyleSheet("combobox-popup: 0;");
   return cb;
}

void RadioComboBoxItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   // get the index of the text in the combobox that matches the current value of the item
   //const QString currentText = index.data(Qt::EditRole).toString();
   //const int cbIndex = cb->findText(currentText);
   // if it is valid, adjust the combobox
   //if (cbIndex >= 0)
   //   cb->setCurrentIndex(cbIndex);
   cb->setCurrentIndex(index.data(Qt::EditRole).toInt());
}

void RadioComboBoxItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   //model->setData(index, cb->currentText(), Qt::EditRole);
   model->setData(index, cb->currentIndex(), Qt::EditRole);
}

void RadioComboBoxItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{

  //if (option.state & QStyle::State_Selected)
  // painter->fillRect(option.rect, option.palette.highlight());

  QString text;
  int value = index.data(Qt::EditRole).toInt();
  //text = QString::number(value+1);
  text = settings.value(s_radioName[value], QString::number(value)).toString();
  painter->drawText(option.rect.adjusted(5, 0, 0, 0), Qt::AlignVCenter|Qt::AlignLeft, text);
}

AuxComboBoxItemDelegate::AuxComboBoxItemDelegate(QObject *parent)
                          : QStyledItemDelegate(parent)
{

}

AuxComboBoxItemDelegate::~AuxComboBoxItemDelegate()
{

}

QWidget *AuxComboBoxItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &/*option*/,
                                            const QModelIndex &/*index*/) const
{
   // Create the combobox and populate it
   QComboBox *cb = new QComboBox(parent);
   //const int row = index.row();
   //cb->addItem(QString("one in row %1").arg(row));
   cb->insertItem(0, kOff);
   cb->insertItem(1, kOn);
   cb->insertItem(2, kSplit);
   //cb->insertItem(3, kUser);
   return cb;
}

void AuxComboBoxItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   // get the index of the text in the combobox that matches the current value of the item
   //const QString currentText = index.data(Qt::EditRole).toString();
   //const int cbIndex = cb->findText(currentText);
   // if it is valid, adjust the combobox
   //if (cbIndex >= 0)
   //   cb->setCurrentIndex(cbIndex);
   cb->setCurrentIndex(index.data(Qt::EditRole).toInt());
}

void AuxComboBoxItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   //model->setData(index, cb->currentText(), Qt::EditRole);
   model->setData(index, cb->currentIndex(), Qt::EditRole);
}

void AuxComboBoxItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{

  //if (option.state & QStyle::State_Selected)
  // painter->fillRect(option.rect, option.palette.highlight());

  QString text;
  int value = index.data(Qt::EditRole).toInt();
  switch (value) {
    case 0:
      text = kOff;
      break;
    case 1:
      text = kOn;
      break;
    case 2:
      text = kSplit;
      break;
    //case 3:
    //  text = kUser;
    //  break;
    default:
      text = kOff;
      break;
  }
  painter->drawText(option.rect.adjusted(5, 0, 0, 0), Qt::AlignVCenter|Qt::AlignLeft, text);
}

CompassComboBoxItemDelegate::CompassComboBoxItemDelegate(QObject *parent)
                          : QStyledItemDelegate(parent)
{

}

CompassComboBoxItemDelegate::~CompassComboBoxItemDelegate()
{

}

QWidget *CompassComboBoxItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &/*option*/,
                                            const QModelIndex &/*index*/) const
{
   // Create the combobox and populate it
   QComboBox *cb = new QComboBox(parent);
   //const int row = index.row();
   //cb->addItem(QString("one in row %1").arg(row));
   cb->insertItem(0, kList);
   cb->insertItem(1, kCompass);
   return cb;
}

void CompassComboBoxItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   // get the index of the text in the combobox that matches the current value of the item
   //const QString currentText = index.data(Qt::EditRole).toString();
   //const int cbIndex = cb->findText(currentText);
   // if it is valid, adjust the combobox
   //if (cbIndex >= 0)
   //   cb->setCurrentIndex(cbIndex);
   cb->setCurrentIndex(index.data(Qt::EditRole).toInt());
}

void CompassComboBoxItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   //model->setData(index, cb->currentText(), Qt::EditRole);
   model->setData(index, cb->currentIndex(), Qt::EditRole);
}

void CompassComboBoxItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const
{

  //if (option.state & QStyle::State_Selected)
  // painter->fillRect(option.rect, option.palette.highlight());

  QString text;
  int value = index.data(Qt::EditRole).toInt();
  if (value) text = kCompass;
  else text = kList;
  painter->drawText(option.rect.adjusted(5, 0, 0, 0), Qt::AlignVCenter|Qt::AlignLeft, text);
}


PortComboBoxItemDelegate::PortComboBoxItemDelegate(QObject *parent)
                          : QStyledItemDelegate(parent)
{

}

PortComboBoxItemDelegate::~PortComboBoxItemDelegate()
{

}

QWidget *PortComboBoxItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &/*option*/,
                                            const QModelIndex &/*index*/) const
{
   // Create the combobox and populate it
   QComboBox *cb = new QComboBox(parent);
   //const int row = index.row();
   //cb->addItem(QString("one in row %1").arg(row));
   for (int i = 0; i<=40; ++i) {
     cb->insertItem(i, QString::number(i));
   }
   cb->setMaxVisibleItems(10);
   cb->setStyleSheet("combobox-popup: 0;");
   return cb;
}

void PortComboBoxItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   // get the index of the text in the combobox that matches the current value of the item
   //const QString currentText = index.data(Qt::EditRole).toString();
   //const int cbIndex = cb->findText(currentText);
   // if it is valid, adjust the combobox
   //if (cbIndex >= 0)
   //   cb->setCurrentIndex(cbIndex);
   cb->setCurrentIndex(index.data(Qt::EditRole).toInt());
}

void PortComboBoxItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   //model->setData(index, cb->currentText(), Qt::EditRole);
   model->setData(index, cb->currentIndex(), Qt::EditRole);
}


VantComboBoxItemDelegate::VantComboBoxItemDelegate(QObject *parent)
                          : QStyledItemDelegate(parent)
{

}

VantComboBoxItemDelegate::~VantComboBoxItemDelegate()
{

}

QWidget *VantComboBoxItemDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &/*option*/,
                                            const QModelIndex &/*index*/) const
{
   // Create the combobox and populate it
   QComboBox *cb = new QComboBox(parent);
   //const int row = index.row();
   for (int i = 0; i<=200; ++i) {
     cb->insertItem(i, QString::number(i));
   }
   cb->setMaxVisibleItems(10);
   cb->setStyleSheet("combobox-popup: 0;");
   return cb;
}

void VantComboBoxItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);

   cb->setCurrentIndex(index.data(Qt::EditRole).toInt());
}

void VantComboBoxItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
   QComboBox *cb = qobject_cast<QComboBox *>(editor);
   Q_ASSERT(cb);
   model->setData(index, cb->currentIndex(), Qt::EditRole);
}




// misc

class BoolProxy : public QIdentityProxyModel {
    int m_column;
public:
    BoolProxy(int column, QObject * parent = nullptr) :
        QIdentityProxyModel{parent}, m_column{column} {}
    QVariant data(const QModelIndex & index, int role) const override {
        auto val = QIdentityProxyModel::data(index, role);
        if (index.column() != m_column) return val;
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            if (val.toInt() == 0) {
                if (role == Qt::DisplayRole) return kNo;
                return false;
            } else {
                if (role == Qt::DisplayRole) return kYes;
                return true;
            }
        }
        return val;
    }
    bool setData(const QModelIndex & index, const QVariant & value, int role) override {
        auto val = value;
        if (index.column() == m_column && role == Qt::EditRole)
            val = val.toBool() ? 1 : 0;
        return QIdentityProxyModel::setData(index, val, role);
    }
};

class YesNoDelegate : public QStyledItemDelegate {
    QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override {
        auto ed = QStyledItemDelegate::createEditor(parent, option, index);
        auto combo = qobject_cast<QComboBox*>(ed);
        combo->setItemText(0, kNo);
        combo->setItemText(1, kYes);
        return ed;
    }
};
