#pragma once

#include <QPlainTextEdit>

namespace DentalPatients {

class AnchoredPlaceholderPlainTextEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit AnchoredPlaceholderPlainTextEdit(QWidget* parent = nullptr);
    explicit AnchoredPlaceholderPlainTextEdit(const QString& text, QWidget* parent = nullptr);

    void setAnchoredPlaceholderText(const QString& text);
    QString anchoredPlaceholderText() const { return m_placeholderText; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_placeholderText;
};

} // namespace DentalPatients
