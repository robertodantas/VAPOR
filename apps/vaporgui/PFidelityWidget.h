#pragma once

#include "PSection.h"
#include "PLineItem.h"
class VComboBox;

class PFidelityWidget : public PSection {
public:
    PFidelityWidget();
};

class PQuickFidelitySelector : public PLineItem {
    Q_OBJECT
    VComboBox *_vComboBox;

public:
    PQuickFidelitySelector();

protected:
    virtual void updateGUI() const override;
    bool         requireDataMgr() const override { return true; }

private slots:
    void dropdownTextChanged(std::string text);
};

class PLODSelector : public PLineItem {
    Q_OBJECT
    VComboBox *_vComboBox;

public:
    PLODSelector();

protected:
    virtual void updateGUI() const override;
    bool         requireDataMgr() const override { return true; }

private slots:
    void dropdownIndexChanged(int i);
};

class PRefinementSelector : public PLineItem {
    Q_OBJECT
    VComboBox *_vComboBox;

public:
    PRefinementSelector();

protected:
    virtual void updateGUI() const override;
    bool         requireDataMgr() const override { return true; }

private slots:
    void dropdownIndexChanged(int i);
};