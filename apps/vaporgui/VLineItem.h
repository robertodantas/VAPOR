#pragma once

#include <QWidget>
#include <QLayoutItem>
#include <string>

//! \class VLineItem
//! A widget that creates a label - input pair separated by a spacer
//! as is used throughout vapor

class VLineItem : public QWidget {
    Q_OBJECT

public:
    VLineItem(const std::string &label, QLayoutItem *centerItem, QWidget *rightWidget);
    VLineItem(const std::string &label, QWidget *centerWidget, QWidget *rightWidget);
    VLineItem(const std::string &label, QWidget *rightWidget);
};
