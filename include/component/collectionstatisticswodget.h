#ifndef COLLECTIONSTATISTICSWODGET_H
#define COLLECTIONSTATISTICSWODGET_H

#include "base/BaseWidget.h"

namespace Ui {
class CollectionStatisticsWodget;
}

class CollectionStatisticsWodget : public BaseWidget
{
    Q_OBJECT

public:
    explicit CollectionStatisticsWodget(QWidget *parent = nullptr);
    ~CollectionStatisticsWodget();

    void languageChange() override;

private:
    Ui::CollectionStatisticsWodget *ui;
};

#endif // COLLECTIONSTATISTICSWODGET_H
