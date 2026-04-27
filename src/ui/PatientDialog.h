#pragma once

#include "db/Patient.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QCheckBox;

namespace DentalPatients {

class PatientRepository;

class PatientDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Add, Edit };
    enum class Field { FamilyName, GivenName, FileNumber, Phone, Notes };

    PatientDialog(Mode mode, PatientRepository* repo, Patient initial, QWidget* parent = nullptr);
    PatientDialog(Mode mode, PatientRepository* repo, Patient initial, Field initialFocusField,
                  QWidget* parent = nullptr);

    Patient result() const { return m_current; }

private slots:
    void onSave();
    void onValidate();
    void onAutoFileNumberToggled(bool checked);

private:
    void buildUi();
    bool validate(QString* error) const;
    bool assignNextFileNumber(bool showError);
    void updateAutoFileNumberUi(bool checked, bool showError);
    void updateAutoFileNumberHint(const QString& fileNumber);
    QWidget* widgetForField(Field field) const;
    void focusInitialField();

    Mode m_mode;
    PatientRepository* m_repo;
    Patient m_current;
    Field m_initialFocusField = Field::FamilyName;
    qint64 m_originalId = -1;
    QString m_originalFileNumber;

    QLineEdit* m_familyName = nullptr;
    QLineEdit* m_givenName = nullptr;
    QLineEdit* m_fileNumber = nullptr;
    QLineEdit* m_phone = nullptr;
    QPlainTextEdit* m_notes = nullptr;
    QCheckBox* m_autoFileNumber = nullptr;
    QLabel* m_autoFileNumberHint = nullptr;
    QLabel* m_errorLabel = nullptr;
    class QPushButton* m_saveBtn = nullptr;
};

} // namespace DentalPatients
