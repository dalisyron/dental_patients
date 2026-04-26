#pragma once

#include <QDialog>

namespace DentalPatients {

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace DentalPatients
